// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/task_runner_context.h"

#include <functional>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

class TaskRunner : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// This is the simplest test - runs one action only on a sequenced task runner.
TEST_F(TaskRunner, SingleAction) {
  class SingleActionContext : public TaskRunnerContext<bool> {
   public:
    SingleActionContext(base::OnceCallback<void(bool)> callback,
                        scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<bool>(std::move(callback), std::move(task_runner)) {
    }

   private:
    void OnStart() override { Response(true); }
  };

  test::TestEvent<bool> test_event;
  Start<SingleActionContext>(test_event.cb(),
                             base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_TRUE(test_event.result());
}

// This test runs a series of action on a sequenced task runner.
TEST_F(TaskRunner, SeriesOfActions) {
  class SeriesOfActionsContext : public TaskRunnerContext<uint32_t> {
   public:
    SeriesOfActionsContext(uint32_t init_value,
                           base::OnceCallback<void(uint32_t)> callback,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<uint32_t>(std::move(callback),
                                      std::move(task_runner)),
          init_value_(init_value) {}

   private:
    void Halve(uint32_t value, uint32_t log) {
      CheckOnValidSequence();
      if (value <= 1) {
        Response(log);
        return;
      }
      Schedule(&SeriesOfActionsContext::Halve, base::Unretained(this),
               value / 2, log + 1);
    }

    void OnStart() override { Halve(init_value_, 0); }

    const uint32_t init_value_;
  };

  test::TestEvent<uint32_t> test_event;
  Start<SeriesOfActionsContext>(128, test_event.cb(),
                                base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_THAT(test_event.result(), Eq(7u));
}

// This test runs the same series of actions injecting delays.
TEST_F(TaskRunner, SeriesOfDelays) {
  class SeriesOfDelaysContext : public TaskRunnerContext<uint32_t> {
   public:
    SeriesOfDelaysContext(uint32_t init_value,
                          base::OnceCallback<void(uint32_t)> callback,
                          scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<uint32_t>(std::move(callback),
                                      std::move(task_runner)),
          init_value_(init_value),
          delay_(base::Seconds(0.1)) {}

   private:
    void Halve(uint32_t value, uint32_t log) {
      CheckOnValidSequence();
      if (value <= 1) {
        Response(log);
        return;
      }
      delay_ += base::Seconds(0.1);
      ScheduleAfter(delay_, &SeriesOfDelaysContext::Halve,
                    base::Unretained(this), value / 2, log + 1);
    }

    void OnStart() override { Halve(init_value_, 0); }

    const uint32_t init_value_;
    base::TimeDelta delay_;
  };

  test::TestEvent<uint32_t> test_event;
  Start<SeriesOfDelaysContext>(128, test_event.cb(),
                               base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_THAT(test_event.result(), Eq(7u));
}

// This test runs the same series of actions offsetting them to a random threads
// and then taking control back to the sequenced task runner.
TEST_F(TaskRunner, SeriesOfAsyncs) {
  class SeriesOfAsyncsContext : public TaskRunnerContext<uint32_t> {
   public:
    SeriesOfAsyncsContext(uint32_t init_value,
                          base::OnceCallback<void(uint32_t)> callback,
                          scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<uint32_t>(std::move(callback),
                                      std::move(task_runner)),
          init_value_(init_value),
          delay_(base::Seconds(0.1)) {}

   private:
    void Halve(uint32_t value, uint32_t log) {
      CheckOnValidSequence();
      if (value <= 1) {
        Response(log);
        return;
      }
      // Perform a calculation on a generic thread pool with delay,
      // then get back to the sequence by calling Schedule from there.
      delay_ += base::Seconds(0.1);
      base::ThreadPool::PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](uint32_t value, uint32_t log, SeriesOfAsyncsContext* context) {
                // Action executed asyncrhonously.
                value /= 2;
                ++log;
                // Getting back to the sequence.
                context->Schedule(&SeriesOfAsyncsContext::Halve,
                                  base::Unretained(context), value, log);
              },
              value, log, base::Unretained(this)),
          delay_);
    }

    void OnStart() override { Halve(init_value_, 0); }

    const uint32_t init_value_;
    base::TimeDelta delay_;
  };

  test::TestEvent<uint32_t> test_event;
  Start<SeriesOfAsyncsContext>(128, test_event.cb(),
                               base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_THAT(test_event.result(), Eq(7u));
}

// This test calculates Fibonacci as a tree of recurrent actions on a sequenced
// task runner. Note that 2 actions are scheduled in parallel.
TEST_F(TaskRunner, TreeOfActions) {
  // Helper class accepts multiple 'AddIncoming' calls to add numbers,
  // and invokes 'callback' when last reference to it is dropped.
  class Summator : public base::RefCounted<Summator> {
   public:
    explicit Summator(base::OnceCallback<void(uint32_t)> callback)
        : callback_(std::move(callback)) {}

    void AddIncoming(uint32_t incoming) { result_ += incoming; }

   protected:
    virtual ~Summator() {
      CHECK(!callback_.is_null());
      std::move(callback_).Run(result_);
    }

   private:
    friend class base::RefCounted<Summator>;

    uint32_t result_ = 0;
    base::OnceCallback<void(uint32_t)> callback_;
  };

  // Context class for Fibonacci asynchronous recursion tree.
  class TreeOfActionsContext : public TaskRunnerContext<uint32_t> {
   public:
    TreeOfActionsContext(uint32_t init_value,
                         base::OnceCallback<void(uint32_t)> callback,
                         scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<uint32_t>(std::move(callback),
                                      std::move(task_runner)),
          init_value_(init_value) {}

   private:
    void FibonacciSplit(uint32_t value, scoped_refptr<Summator> join) {
      CheckOnValidSequence();
      if (value < 2u) {
        join->AddIncoming(value);  // Fib(0) == 1, Fib(1) == 1
        return;                    // No more actions to schedule.
      }
      // Schedule two asynchronous recursive calls.
      // 'join' above will self-destruct once both callbacks complete
      // and drop references to it. Each callback spawns additional
      // callbacks, and when they complete, adds the results to its
      // own 'Summator' instance.
      for (const uint32_t subval : {value - 1, value - 2}) {
        Schedule(&TreeOfActionsContext::FibonacciSplit, base::Unretained(this),
                 subval,
                 base::MakeRefCounted<Summator>(
                     base::BindOnce(&Summator::AddIncoming, join)));
      }
    }

    void OnStart() override {
      FibonacciSplit(init_value_, base::MakeRefCounted<Summator>(base::BindOnce(
                                      &TreeOfActionsContext::Response,
                                      base::Unretained(this))));
    }

    const uint32_t init_value_;
  };

  const std::vector<uint32_t> expected_fibo_results(
      {0,  1,  1,   2,   3,   5,   8,   13,   21,   34,
       55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181});
  std::vector<uint32_t> actual_fibo_results(expected_fibo_results.size());
  test::TestCallbackWaiter waiter;
  // Start all calculations (they will intermix on the same sequential runner).
  for (uint32_t n = 0; n < expected_fibo_results.size(); ++n) {
    waiter.Attach();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([n, &waiter, &actual_fibo_results]() {
          Start<TreeOfActionsContext>(
              n,
              base::BindLambdaForTesting(
                  [n, &waiter, &actual_fibo_results](uint32_t value) {
                    actual_fibo_results[n] = value;
                    waiter.Signal();
                  }),
              base::SequencedTaskRunner::GetCurrentDefault());
        }));
  }
  waiter.Wait();
  EXPECT_THAT(actual_fibo_results, Eq(expected_fibo_results));
}

// This test runs a series of actions returning non-primitive object as a result
// (Status).
TEST_F(TaskRunner, ActionsWithStatus) {
  class ActionsWithStatusContext : public TaskRunnerContext<Status> {
   public:
    ActionsWithStatusContext(
        const std::vector<Status>& vector,
        base::OnceCallback<void(Status)> callback,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<Status>(std::move(callback),
                                    std::move(task_runner)),
          vector_(vector) {}

   private:
    void Pick(size_t index) {
      CheckOnValidSequence();
      if (index < vector_.size()) {
        if (vector_[index].ok()) {
          Schedule(&ActionsWithStatusContext::Pick, base::Unretained(this),
                   index + 1);
          return;
        }
        Response(vector_[index]);
        return;
      }
      Response(Status(error::OUT_OF_RANGE, "All statuses are OK"));
    }

    void OnStart() override { Pick(0); }

    const std::vector<Status> vector_;
  };

  test::TestEvent<Status> test_event;
  Start<ActionsWithStatusContext>(
      std::vector<Status>({Status::StatusOK(), Status::StatusOK(),
                           Status::StatusOK(),
                           Status(error::CANCELLED, "Cancelled"),
                           Status::StatusOK(), Status::StatusOK()}),
      test_event.cb(), base::SequencedTaskRunner::GetCurrentDefault());
  EXPECT_THAT(test_event.result(), Eq(Status(error::CANCELLED, "Cancelled")));
}

// This test runs a series of actions returning non-primitive non-copyable
// object as a result (StatusOr<std::unique_ptr<...>>).
TEST_F(TaskRunner, ActionsWithStatusOrPtr) {
  class WrappedValue {
   public:
    explicit WrappedValue(int value) : value_(value) {}
    ~WrappedValue() = default;

    WrappedValue(const WrappedValue& other) = delete;
    WrappedValue& operator=(const WrappedValue& other) = delete;

    int value() const { return value_; }

   private:
    const int value_;
  };
  using StatusOrPtr = StatusOr<std::unique_ptr<WrappedValue>>;
  class ActionsWithStatusOrContext : public TaskRunnerContext<StatusOrPtr> {
   public:
    ActionsWithStatusOrContext(
        std::vector<StatusOrPtr>* vector,
        base::OnceCallback<void(StatusOrPtr)> callback,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
        : TaskRunnerContext<StatusOrPtr>(std::move(callback),
                                         std::move(task_runner)),
          vector_(std::move(vector)) {}

   private:
    void Pick(size_t index) {
      CheckOnValidSequence();
      if (index < vector_->size()) {
        if (!vector_->at(index).has_value()) {
          Schedule(&ActionsWithStatusOrContext::Pick, base::Unretained(this),
                   index + 1);
          return;
        }
        Response(std::move(vector_->at(index)));
        return;
      }
      Response(
          base::unexpected(Status(error::OUT_OF_RANGE, "All statuses are OK")));
    }

    void OnStart() override { Pick(0); }

    const raw_ptr<std::vector<StatusOrPtr>> vector_;
  };

  const int kI = 0;
  std::vector<StatusOrPtr> vector;
  for (int i = 0; i < 5; ++i) {
    vector.emplace_back(
        base::unexpected(Status(error::CANCELLED, "Cancelled")));
  }
  vector.emplace_back(std::make_unique<WrappedValue>(kI));
  test::TestEvent<StatusOrPtr> test_event;
  Start<ActionsWithStatusOrContext>(
      &vector, test_event.cb(), base::SequencedTaskRunner::GetCurrentDefault());
  const StatusOrPtr result = test_event.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result.value()->value(), kI);
}

}  // namespace
}  // namespace reporting
