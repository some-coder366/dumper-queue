#pragma once
#include "spinlock.h"
#include "ring_buffer.h"
#include "thread.h"
#include <vector>

namespace LIB_NAMESPACE
{
	/*
	a simple method to send much data between multiple producer threads and one consumer without high mutex and condition variables overhead
	the producer should construct the data then move it into the queue in order to reduce the locking time
	the producer can be configured to either block when the ring buffer is full or overwrite old data
	in case of overwriting the user should use suitable size for the buffers and ensure that the consumer isn't beyond the producers
	*/

	template <class T>
	class DumperQueue;

	template <class T, bool OverWrite = true>
	class SubmitionQueue
	{

	public:

		friend class DumperQueue<T>;

		SubmitionQueue() = default;

		void Push(T&& value)
		{
			mtx.lock();
			if constexpr (!OverWrite)
			{
				while (rbf.full())
				{
					mtx.unlock();
					this_thread::sleep_for(std::chrono::microseconds(10));
					mtx.lock();
				}
				rbf.emplace_back_without_checks(std::move(value));
			}
			else
			{
				rbf.emplace_back(std::move(value));
			}
			mtx.unlock();
		}

	private:

		SpinLock mtx;
		ring_buffer<T> rbf;

		void Reserve(size_t size) { rbf.set_capacity(size); }

		void SwapQueue(ring_buffer<T>& other) noexcept
		{
			mtx.lock();
			rbf.swap(other);
			mtx.unlock();
		}

	};

	template <class T>
	class DumperQueue
	{
		std::vector<SubmitionQueue<T>> SQs; // queue per producer
		ring_buffer<T> CQ;

	public:

		DumperQueue() = default;

		DumperQueue(size_t thds) : SQs(thds) {}

		void SetThreadsNum(size_t thds) { SQs = std::vector<SubmitionQueue<T>>(thds); }

		void SetCapacity(size_t cap) { CQ.set_capacity(cap); for (auto& q : SQs) q.Reserve(cap); }

		template <class Fn>
		void Dump(Fn OnEntry)
		{
			for (auto& SQ : SQs)
			{
				SQ.SwapQueue(CQ);
				for (auto& entry : CQ)
					OnEntry(entry);
				CQ.clear();
			}
		}

		std::span<SubmitionQueue<T>> GetSQs() { return { SQs.data(), SQs.data() + SQs.size() }; }

	};



};
