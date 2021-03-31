// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_
#define COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_

#include <atomic>
#include <tuple>

#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"

namespace reporting {
namespace test {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {}
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_.Run();
    return std::forward<ResType>(result_);
  }

  // Repeating callback to hand over to the processing method.
  // Even though it is repeating, it can be only called once, since
  // it quits run_loop; repeating declaration is only needed for cases
  // when the caller requires it.
  // If the caller expects OnceCallback, result will be converted automatically.
  base::RepeatingCallback<void(ResType res)> cb() {
    return base::BindRepeating(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(&run_loop_), base::Unretained(&result_));
  }

 private:
  base::RunLoop run_loop_;
  ResType result_;
};

// Usage (in tests only):
//
//   TestMultiEvent<ResType1, Restype2, ...> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType1, Restype2, ...)> type which also may
//   perform some other action specified by |done| callback provided by the
//   caller. std::tie(res1, res2, ...) = e.result();  // Will wait for e.cb() to
//   be called and return the collected results.
//
template <typename... ResType>
class TestMultiEvent {
 public:
  TestMultiEvent() : run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {}
  ~TestMultiEvent() = default;
  TestMultiEvent(const TestMultiEvent& other) = delete;
  TestMultiEvent& operator=(const TestMultiEvent& other) = delete;
  std::tuple<ResType...> result() {
    run_loop_.Run();
    return std::forward<std::tuple<ResType...>>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::RepeatingCallback<void(ResType... res)> cb() {
    return base::BindRepeating(
        [](base::RunLoop* run_loop, std::tuple<ResType...>* result,
           ResType... res) {
          *result = std::forward_as_tuple(res...);
          run_loop->Quit();
        },
        base::Unretained(&run_loop_), base::Unretained(&result_));
  }

 private:
  base::RunLoop run_loop_;
  std::tuple<ResType...> result_;
};

// Usage (in tests only):
//
//  TestCallbackWaiter waiter;
//  ... do something
//  waiter.Wait();
//
//  or, with multithreadeded activity:
//
//  TestCallbackWaiter waiter;
//  waiter.Attach(N);  // N - is a number of asynchronous actions
//  ...
//  waiter.Wait();
//
//  And  in each of N actions: waiter.Signal(); when done

class TestCallbackWaiter {
 public:
  TestCallbackWaiter();
  ~TestCallbackWaiter();
  TestCallbackWaiter(const TestCallbackWaiter& other) = delete;
  TestCallbackWaiter& operator=(const TestCallbackWaiter& other) = delete;

  void Attach(size_t more = 1) {
    const size_t old_counter = counter_.fetch_add(more);
    DCHECK_GT(old_counter, 0u) << "Cannot attach when already being released";
  }

  void Signal() {
    const size_t old_counter = counter_.fetch_sub(1);
    DCHECK_GT(old_counter, 0u) << "Already being released";
    if (old_counter > 1u) {
      // There are more owners.
      return;
    }
    // Dropping the last owner.
    run_loop_.Quit();
  }

  void Wait() {
    Signal();  // Rid of the constructor's ownership.
    run_loop_.Run();
  }

 private:
  std::atomic<size_t> counter_{1};  // Owned by constructor.
  base::RunLoop run_loop_;
};

// RAAI wrapper for TestCallbackWaiter.
//
// Usage:
// {
//   TestCallbackAutoWaiter waiter;  // Implicitely Attach(1);
//   ...
//   Launch async activity, which will eventually do waiter.Signal();
//   ...
// }   // Here the waiter will automatically wait.

class TestCallbackAutoWaiter : public TestCallbackWaiter {
 public:
  TestCallbackAutoWaiter();
  ~TestCallbackAutoWaiter();
};

}  // namespace test
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_