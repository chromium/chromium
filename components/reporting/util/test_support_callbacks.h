// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_
#define COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_

#include <tuple>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

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
//   This class follows base::BarrierCallback in using mutex Lock.
//   It can only be done in tests, never in production code.
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
    base::ReleasableAutoLock lock(&mutex_);
    return std::forward<ResType>(result_);
  }

  // Repeating callback to hand over to the processing method.
  // Even though it is repeating, it can be only called once, since
  // it quits run_loop; repeating declaration is only needed for cases
  // when the caller requires it.
  // If the caller expects OnceCallback, result will be converted automatically.
  base::RepeatingCallback<void(ResType res)> cb() {
    return base::BindRepeating(&TestEvent<ResType>::Callback,
                               base::Unretained(this));
  }

 private:
  void Callback(ResType res) {
    {
      base::ReleasableAutoLock lock(&mutex_);
      result_ = std::forward<ResType>(res);
    }
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::Lock mutex_;
  ResType result_ GUARDED_BY(mutex_);
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
//   This class follows base::BarrierCallback in using mutex Lock.
//   It can only be done in tests, never in production code.
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
    base::ReleasableAutoLock lock(&mutex_);
    return std::forward<std::tuple<ResType...>>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::RepeatingCallback<void(ResType... res)> cb() {
    return base::BindRepeating(&TestMultiEvent<ResType...>::Callback,
                               base::Unretained(this));
  }

 private:
  void Callback(ResType... res) {
    {
      base::ReleasableAutoLock lock(&mutex_);
      result_ = std::forward_as_tuple(res...);
    }
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::Lock mutex_;
  std::tuple<ResType...> result_ GUARDED_BY(mutex_);
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

  void Attach(int more = 1) {
    const int old_counter = counter_.Increment(more);
    DCHECK_GT(old_counter, 0) << "Cannot attach when already being released";
  }

  void Signal() {
    if (counter_.Decrement()) {
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
  base::AtomicRefCount counter_{1};  // Owned by constructor.
  base::RunLoop run_loop_;
};

// RAII wrapper for TestCallbackWaiter.
//
// Usage:
// {
//   TestCallbackAutoWaiter waiter;  // Implicitly Attach(1);
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