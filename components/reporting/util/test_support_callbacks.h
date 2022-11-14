// Copyright 2021 The Chromium Authors
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
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace test {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   Now wait for e.cb() to be called and return the collected result.
//
//   ... = e.result();
//
template <typename ResType>
class TestEvent : base::test::TestFuture<ResType> {
 public:
  TestEvent() = default;
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;

  template <std::enable_if_t<std::is_move_constructible<ResType>::value, bool> =
                false>
  [[nodiscard]] const ResType& ref_result() {
    return std::forward<ResType>(base::test::TestFuture<ResType>::Get());
  }

  template <
      std::enable_if_t<std::is_move_constructible<ResType>::value, bool> = true>
  [[nodiscard]] ResType result() {
    return std::forward<ResType>(base::test::TestFuture<ResType>::Take());
  }

  // Returns true if the event callback was never invoked.
  [[nodiscard]] bool no_result() const {
    return !base::test::TestFuture<ResType>::IsReady();
  }

  // Completion callback to hand over to the processing method.
  [[nodiscard]] base::OnceCallback<void(ResType res)> cb() {
    return base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                              base::test::TestFuture<ResType>::GetCallback());
  }

  // Repeating completion callback to hand over to the processing method.
  // Even though it is repeating, it can be only called once, since
  // `result` only waits for one value; repeating declaration is only needed
  // for cases when the caller requires it.
  [[nodiscard]] base::RepeatingCallback<void(ResType res)> repeating_cb() {
    return base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindRepeating(
            [](base::WeakPtr<TestEvent<ResType>> self, ResType res) {
              if (self) {
                std::move(self->GetCallback()).Run(res);
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  base::WeakPtrFactory<TestEvent<ResType>> weak_ptr_factory_{this};
};

// Usage (in tests only):
//
//   TestMultiEvent<ResType1, Restype2, ...> e;
//
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType1, Restype2, ...)> type which also may
//   perform some other action specified by |done| callback provided by the
//   caller. Now wait for e.cb() to be called and return the collected results.
//
//   std::tie(res1, res2, ...) = e.result();
//
template <typename... ResType>
class TestMultiEvent : base::test::TestFuture<ResType...> {
 public:
  TestMultiEvent() = default;
  ~TestMultiEvent() = default;
  TestMultiEvent(const TestMultiEvent& other) = delete;
  TestMultiEvent& operator=(const TestMultiEvent& other) = delete;

  template <std::enable_if_t<
                std::is_move_constructible<std::tuple<ResType...>>::value,
                bool> = false>
  [[nodiscard]] const std::tuple<ResType...>& ref_result() {
    return std::forward<std::tuple<ResType...>>(
        base::test::TestFuture<ResType...>::Get());
  }

  template <std::enable_if_t<
                std::is_move_constructible<std::tuple<ResType...>>::value,
                bool> = true>
  [[nodiscard]] std::tuple<ResType...> result() {
    return std::forward<std::tuple<ResType...>>(
        base::test::TestFuture<ResType...>::Take());
  }

  // Returns true if the event callback was never invoked.
  [[nodiscard]] bool no_result() const {
    return !base::test::TestFuture<ResType...>::IsReady();
  }

  // Completion callback to hand over to the processing method.
  [[nodiscard]] base::OnceCallback<void(ResType... res)> cb() {
    return base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::test::TestFuture<ResType...>::GetCallback());
  }

  // Repeating completion callback to hand over to the processing method.
  // Even though it is repeating, it can be only called once, since
  // `result` only waits for one value; repeating declaration is only needed
  // for cases when the caller requires it.
  [[nodiscard]] base::RepeatingCallback<void(ResType... res)> repeating_cb() {
    return base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindRepeating(
            [](base::WeakPtr<TestMultiEvent<ResType...>> self, ResType... res) {
              if (self) {
                std::move(self->GetCallback()).Run(res...);
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  base::WeakPtrFactory<TestMultiEvent<ResType...>> weak_ptr_factory_{this};
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

class TestCallbackWaiter : public base::test::TestFuture<bool> {
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
    std::move(signaled_cb_).Run(true);
  }

  void Wait() {
    Signal();  // Rid of the constructor's ownership.
    ASSERT_TRUE(base::test::TestFuture<bool>::Get());
  }

 private:
  base::AtomicRefCount counter_{1};  // Owned by constructor.
  base::OnceCallback<void(bool)> signaled_cb_;
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
