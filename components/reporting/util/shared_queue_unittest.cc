// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/shared_queue.h"

#include <vector>

#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class QueueTester {
 public:
  explicit QueueTester(scoped_refptr<SharedQueue<int>> queue)
      : queue_(queue),
        sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
        pop_result_(Status(error::FAILED_PRECONDITION, "Pop hasn't run yet")) {}

  ~QueueTester() = default;

  void Push(int value) { queue_->Push(value, base::DoNothing()); }

  void Pop() {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QueueTester::PopInternal, base::Unretained(this)));
  }

  void Swap() {
    queue_->Swap(base::queue<int>(),
                 base::BindOnce(&QueueTester::OnSwap, base::Unretained(this)));
  }

  void PushPop(int value) {
    queue_->Push(value, base::BindOnce(&QueueTester::OnPushPopComplete,
                                       base::Unretained(this)));
  }

  void Wait() {
    completed_.Wait();
    completed_.Reset();
  }

  StatusOr<int> pop_result() { return pop_result_; }
  base::queue<int>* swap_result() { return &swap_result_; }

 private:
  void OnPushPopComplete() { Pop(); }

  void PopInternal() {
    queue_->Pop(
        base::BindOnce(&QueueTester::OnPopComplete, base::Unretained(this)));
  }

  void OnPopComplete(StatusOr<int> pop_result) {
    pop_result_ = pop_result;
    Signal();
  }

  void OnSwap(base::queue<int> swap_result) {
    swap_result_ = swap_result;
    Signal();
  }

  void Signal() { completed_.Signal(); }

  scoped_refptr<SharedQueue<int>> queue_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  base::WaitableEvent completed_;

  StatusOr<int> pop_result_;
  base::queue<int> swap_result_;
};

TEST(SharedQueueTest, SuccessfulPushPop) {
  base::test::TaskEnvironment task_envrionment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const int kExpectedValue = 1234;

  auto queue = SharedQueue<int>::Create();
  QueueTester queue_tester(queue);

  queue_tester.PushPop(kExpectedValue);
  queue_tester.Wait();

  auto pop_result = queue_tester.pop_result();
  ASSERT_OK(pop_result);
  EXPECT_EQ(pop_result.ValueOrDie(), kExpectedValue);
}

TEST(SharedQueueTest, PushOrderMaintained) {
  base::test::TaskEnvironment task_envrionment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::vector<int> kExpectedValues = {1, 1, 2, 3, 5, 8, 13, 21};

  auto queue = SharedQueue<int>::Create();
  QueueTester queue_tester(queue);

  for (auto value : kExpectedValues) {
    queue_tester.Push(value);
  }

  for (auto value : kExpectedValues) {
    queue_tester.Pop();
    queue_tester.Wait();
    auto pop_result = queue_tester.pop_result();
    ASSERT_OK(pop_result);
    EXPECT_EQ(pop_result.ValueOrDie(), value);
  }
}

TEST(SharedQueueTest, SwapSuccessful) {
  base::test::TaskEnvironment task_envrionment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::vector<int> kExpectedValues = {1, 1, 2, 3, 5, 8, 13, 21};

  auto queue = SharedQueue<int>::Create();
  QueueTester queue_tester(queue);

  for (auto value : kExpectedValues) {
    queue_tester.Push(value);
  }

  queue_tester.Swap();
  queue_tester.Wait();

  auto* swapped_queue = queue_tester.swap_result();

  for (auto value : kExpectedValues) {
    EXPECT_EQ(swapped_queue->front(), value);
    swapped_queue->pop();
  }

  // Test to ensure the SharedQueue is empty.
  queue_tester.Pop();
  queue_tester.Wait();

  auto pop_result = queue_tester.pop_result();

  EXPECT_FALSE(pop_result.ok());
  EXPECT_EQ(pop_result.status().error_code(), error::OUT_OF_RANGE);
}

}  // namespace
}  // namespace reporting
