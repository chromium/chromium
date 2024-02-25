// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_
#define COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_

#include <tuple>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {

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
class TestMultiEvent {
 public:
  TestMultiEvent() = default;
  ~TestMultiEvent() = default;
  TestMultiEvent(const TestMultiEvent& other) = delete;
  TestMultiEvent& operator=(const TestMultiEvent& other) = delete;

  using TupleType = std::tuple<std::remove_reference_t<ResType>...>;

  [[nodiscard]] const TupleType& ref_result() {
    static_assert(
        !IsMovable<TupleType>(),
        "std::tuple<ResType...> is not movable. Please use result().");
    RunUntilHasResult();
    CHECK(result_.has_value()) << "Result must have been set";
    return result_.value();
  }

  [[nodiscard]] TupleType result() {
    static_assert(
        IsMovable<TupleType>(),
        "std::tuple<ResType...> is movable. Please use ref_result().");
    CHECK(!result_retrieved_) << "Result already retrieved";
    RunUntilHasResult();
    CHECK(result_.has_value()) << "Result must have been set";
    auto value = std::move(result_.value());
    result_.reset();
    result_retrieved_ = true;
    return value;
  }

  // Returns true if the event callback was never invoked.
  [[nodiscard]] bool no_result() const { return !result_.has_value(); }

  // Completion callback to hand over to the processing method.
  [[nodiscard]] base::OnceCallback<void(ResType... res)> cb() {
    return base::BindPostTaskToCurrentDefault(base::BindOnce(
        &TestMultiEvent::SetResult, weak_ptr_factory_.GetWeakPtr()));
  }

  // Repeating completion callback to hand over to the processing method.
  // Even though it is repeating, it can be only called once, since
  // `result` only waits for one value; repeating declaration is only needed
  // for cases when the caller requires it.
  [[nodiscard]] base::RepeatingCallback<void(ResType... res)> repeating_cb() {
    return base::BindPostTaskToCurrentDefault(base::BindRepeating(
        [](base::WeakPtr<TestMultiEvent<ResType...>> self, ResType... res) {
          if (!self) {
            return;
          }
          ASSERT_FALSE(self->repeated_cb_called_)
              << "repeating_cb() called more than once, but it is only "
                 "intended to be called once.";
          self->SetResult(std::forward<ResType>(res)...);
          self->repeated_cb_called_ = true;
        },
        weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  // Can type T be moved from, i.e., T is move constructible and assignable.
  template <typename T>
  static constexpr bool IsMovable() {
    return std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;
  }

 private:
  void RunUntilHasResult() {
    base::RunLoop run_loop;
    while (true) {
      {
        base::AutoLock auto_lock(lock_);
        if (result_.has_value()) {
          break;
        }
      }
      run_loop.RunUntilIdle();
    }
  }

  void SetResult(ResType... res) {
    base::AutoLock auto_lock(lock_);
    CHECK(!result_.has_value()) << "Can only be called once";
    result_.emplace(std::forward<ResType>(res)...);
  }

  base::Lock lock_;
  std::optional<TupleType> result_;
  bool repeated_cb_called_ = false;
  bool result_retrieved_ = false;
  base::WeakPtrFactory<TestMultiEvent<ResType...>> weak_ptr_factory_{this};
};

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
class TestEvent : public TestMultiEvent<ResType> {
 public:
  TestEvent() = default;
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;

  [[nodiscard]] const ResType& ref_result() {
    static_assert(!TestMultiEvent<ResType>::template IsMovable<ResType>(),
                  "ResType is movable. Please use result().");
    return std::get<0>(TestMultiEvent<ResType>::ref_result());
  }

  [[nodiscard]] ResType result() {
    static_assert(TestMultiEvent<ResType>::template IsMovable<ResType>(),
                  "ResType is not movable. Please use ref_result().");
    return std::get<0>(TestMultiEvent<ResType>::result());
  }
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

class TestCallbackWaiter : public TestEvent<bool> {
 public:
  TestCallbackWaiter();
  ~TestCallbackWaiter();
  TestCallbackWaiter(const TestCallbackWaiter& other) = delete;
  TestCallbackWaiter& operator=(const TestCallbackWaiter& other) = delete;

  void Attach(int more = 1) {
    const int old_counter = counter_.Increment(more);
    CHECK_GT(old_counter, 0) << "Cannot attach when already being released";
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
    ASSERT_TRUE(result());
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
}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_UTIL_TEST_SUPPORT_CALLBACKS_H_
