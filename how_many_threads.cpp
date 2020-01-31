#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <cstdint>

#include <pthread.h>

namespace check {

using counter_t = std::uint_fast64_t;

struct thread_info_t {
	pthread_t thread_handle_;
	std::mutex lock_;
	std::condition_variable wakeup_cv_;
	bool wakeup_{false};
};

using thread_info_uptr_t = std::unique_ptr<thread_info_t>;

using thread_info_container_t = std::vector<thread_info_uptr_t>;

class threads_joiner_t {
	thread_info_container_t & cont_;

public:
	threads_joiner_t(thread_info_container_t & cont) : cont_{cont} {}
	~threads_joiner_t() {
		std::cout << "--- start joining threads (" << cont_.size()
				<< ") ---" << std::endl;
		std::size_t j = 1u;
		for(auto & info: cont_) {
			std::cout << j << "\r" << std::flush;
			{
				std::lock_guard<std::mutex> lock{info->lock_};
				info->wakeup_ = true;
				info->wakeup_cv_.notify_one();
			}
			void * thread_ret_val;
			auto rc = pthread_join(info->thread_handle_, &thread_ret_val);
			if(0 != rc) {
				std::cout << "\n" "pthread_join returns " << rc << std::endl;
			}

			++j;
		}
		std::cout << "\n" << "--- finish joining threads ---" << std::endl;
	}
};

void *
thread_func(void * raw_arg) {
	auto * info = reinterpret_cast<thread_info_t *>(raw_arg);

	std::unique_lock<std::mutex> lock{info->lock_};
	while(!info->wakeup_) {
		info->wakeup_cv_.wait(lock, [info]{ return info->wakeup_; });
	}

	return nullptr;
}

counter_t
do_create_threads(
		thread_info_container_t & threads) {
	counter_t counter = 0u;

	for(;; ++counter) {
		threads.emplace_back(thread_info_uptr_t{new thread_info_t{}});
		auto & info = threads.back();

		auto rc = pthread_create(&info->thread_handle_, nullptr,
				thread_func, info.get());
		if(0 != rc) {
			threads.pop_back();
			std::cout << "\n" "pthread_create returns " << rc << std::endl;
			break;
		}

		std::cout << "created " << (counter+1) << "\r" << std::flush;
	}

	return counter;
}

void
do_check() {
	counter_t counter = 0u;

	{
		thread_info_container_t threads;
		threads.reserve(32767u);
		threads_joiner_t joiner{threads};

		try {
			counter = do_create_threads(threads);
		}
		catch(const std::exception & x) {
			std::cout << "\n" "*** exception caught: " << x.what() << std::endl;
		}
		catch(...) {
			std::cout << "\n" "*** unknown exception caught" << std::endl;
		}
	}

	std::cout << "\n" "maximal value: " << counter << std::endl;
}

} /* namespace check */

int main() {
	check::do_check();
}

