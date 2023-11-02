// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_prediction_waiter.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class WaiterClient : public PasswordFormPredictionWaiter::Client {
 public:
  WaiterClient() = default;
  ~WaiterClient() = default;

  WaiterClient(const WaiterClient&) = delete;
  WaiterClient& operator=(const WaiterClient&) = delete;

  int wait_completed_count() const { return wait_completed_count_; }
  bool timed_out() const { return timed_out_; }
  void Reset() {
    wait_completed_count_ = 0;
    timed_out_ = false;
  }

 protected:
  void OnWaitCompleted() override { wait_completed_count_++; }
  void OnTimeout() override { timed_out_ = true; }

  int wait_completed_count_ = 0;
  bool timed_out_ = false;
};

class PasswordFormPredictionWaiterTest : public testing::Test {
 public:
  PasswordFormPredictionWaiterTest() : prediction_waiter_(&client_) {}

 protected:
  WaiterClient client_;
  PasswordFormPredictionWaiter prediction_waiter_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PasswordFormPredictionWaiterTest, WaitCompletedOnTimeout) {
  prediction_waiter_.StartTimer();
  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
  EXPECT_TRUE(client_.timed_out());
}

TEST_F(PasswordFormPredictionWaiterTest,
       WaitCompletedOnTimeoutWithOutstandingCallback) {
  prediction_waiter_.StartTimer();
  auto closure1 = prediction_waiter_.CreateClosure();
  auto closure2 = prediction_waiter_.CreateClosure();
  std::move(closure1).Run();

  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
  EXPECT_EQ(client_.wait_completed_count(), 0);
  EXPECT_TRUE(client_.timed_out());
}

TEST_F(PasswordFormPredictionWaiterTest, WaitCompletedOnSingleCallback) {
  auto closure1 = prediction_waiter_.CreateClosure();
  std::move(closure1).Run();

  EXPECT_EQ(client_.wait_completed_count(), 1);
  EXPECT_FALSE(client_.timed_out());
}

TEST_F(PasswordFormPredictionWaiterTest, WaitCompletedOnMultipleCallbacks) {
  prediction_waiter_.StartTimer();
  auto closure1 = prediction_waiter_.CreateClosure();
  auto closure2 = prediction_waiter_.CreateClosure();
  auto closure3 = prediction_waiter_.CreateClosure();
  std::move(closure1).Run();
  std::move(closure2).Run();
  std::move(closure3).Run();

  EXPECT_EQ(client_.wait_completed_count(), 1);
  EXPECT_FALSE(client_.timed_out());
}

TEST_F(PasswordFormPredictionWaiterTest,
       WaitCompletedOnTimeoutAndCallbackCompletion) {
  prediction_waiter_.StartTimer();
  auto closure1 = prediction_waiter_.CreateClosure();

  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
  EXPECT_TRUE(client_.timed_out());
  EXPECT_EQ(client_.wait_completed_count(), 0);
  std::move(closure1).Run();
  EXPECT_EQ(client_.wait_completed_count(), 1);
}

TEST_F(PasswordFormPredictionWaiterTest,
       MultipleWaitsCompletedOnTimeoutAndCallbackCompletion) {
  prediction_waiter_.StartTimer();
  auto closure1 = prediction_waiter_.CreateClosure();
  auto closure2 = prediction_waiter_.CreateClosure();

  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
  EXPECT_TRUE(client_.timed_out());
  EXPECT_EQ(client_.wait_completed_count(), 0);
  std::move(closure1).Run();
  std::move(closure2).Run();
  EXPECT_EQ(client_.wait_completed_count(), 2);
}

}  // namespace

}  // namespace password_manager