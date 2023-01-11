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
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  bool result = false;
  // Created context reference is self-destruct upon completion of 'Start',
  // but the context itself lives through until all tasks are done.
  base::RunLoop run_loop;
  Start<SingleActionContext>(
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* var, bool val) {
            *var = val;
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop.Run();
  EXPECT_TRUE(result);
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

  uint32_t result = 0;
  base::RunLoop run_loop;
  Start<SeriesOfActionsContext>(
      128,
      base::BindOnce(
          [](base::RunLoop* run_loop, uint32_t* var, uint32_t val) {
            *var = val;
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop.Run();
  EXPECT_EQ(result, 7u);
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

  // Run on another thread, so that we can wait on the quit event here
  // and avoid RunLoopIdle (which would exit on the first delay).
  uint32_t result = 0;
  base::RunLoop run_loop;
  Start<SeriesOfDelaysContext>(
      128,
      base::BindOnce(
          [](base::RunLoop* run_loop, uint32_t* var, uint32_t val) {
            *var = val;
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop.Run();
  EXPECT_EQ(result, 7u);
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

  // Run on another thread, so that we can wait on the quit event here
  // and avoid RunLoopIdle (which would exit on the first delay).
  uint32_t result = 0;
  base::RunLoop run_loop;
  Start<SeriesOfAsyncsContext>(
      128,
      base::BindOnce(
          [](base::RunLoop* run_loop, uint32_t* var, uint32_t val) {
            *var = val;
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());

  run_loop.Run();
  EXPECT_EQ(result, 7u);
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
      DCHECK(!callback_.is_null());
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
  base::RunLoop run_loop;
  size_t count = expected_fibo_results.size();
  // Start all calculations (they will intermix on the same sequential runner).
  for (uint32_t n = 0; n < expected_fibo_results.size(); ++n) {
    uint32_t* const result = &actual_fibo_results[n];
    *result = 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](size_t* count, base::RunLoop* run_loop, uint32_t n,
                          uint32_t* result) {
                         Start<TreeOfActionsContext>(
                             n,
                             base::BindOnce(
                                 [](size_t* count, base::RunLoop* run_loop,
                                    uint32_t* var, uint32_t val) {
                                   *var = val;
                                   if (!--*count) {
                                     run_loop->Quit();
                                   }
                                 },
                                 count, run_loop, result),
                             base::SequencedTaskRunner::GetCurrentDefault());
                       },
                       &count, &run_loop, n, result));
  }
  // Wait for it all to finish and compare the results.
  run_loop.Run();
  EXPECT_THAT(actual_fibo_results, ::testing::Eq(expected_fibo_results));
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

  Status result(error::UNKNOWN, "Not yet set");
  base::RunLoop run_loop;
  Start<ActionsWithStatusContext>(
      std::vector<Status>({Status::StatusOK(), Status::StatusOK(),
                           Status::StatusOK(),
                           Status(error::CANCELLED, "Cancelled"),
                           Status::StatusOK(), Status::StatusOK()}),
      base::BindOnce(
          [](base::RunLoop* run_loop, Status* result, Status res) {
            *result = res;
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop.Run();
  EXPECT_EQ(result, Status(error::CANCELLED, "Cancelled"));
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
        if (!vector_->at(index).ok()) {
          Schedule(&ActionsWithStatusOrContext::Pick, base::Unretained(this),
                   index + 1);
          return;
        }
        Response(std::move(vector_->at(index)));
        return;
      }
      Response(Status(error::OUT_OF_RANGE, "All statuses are OK"));
    }

    void OnStart() override { Pick(0); }

    std::vector<StatusOrPtr>* const vector_;
  };

  const int kI = 0;
  std::vector<StatusOrPtr> vector;
  vector.emplace_back(Status(error::CANCELLED, "Cancelled"));
  vector.emplace_back(Status(error::CANCELLED, "Cancelled"));
  vector.emplace_back(Status(error::CANCELLED, "Cancelled"));
  vector.emplace_back(Status(error::CANCELLED, "Cancelled"));
  vector.emplace_back(Status(error::CANCELLED, "Cancelled"));
  vector.emplace_back(std::make_unique<WrappedValue>(kI));
  StatusOrPtr result;
  base::RunLoop run_loop;
  Start<ActionsWithStatusOrContext>(
      &vector,
      base::BindOnce(
          [](base::RunLoop* run_loop, StatusOrPtr* result, StatusOrPtr res) {
            *result = std::move(res);
            run_loop->Quit();
          },
          &run_loop, &result),
      base::SequencedTaskRunner::GetCurrentDefault());
  run_loop.Run();
  EXPECT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result.ValueOrDie()->value(), kI);
}

}  // namespace
}  // namespace reporting
