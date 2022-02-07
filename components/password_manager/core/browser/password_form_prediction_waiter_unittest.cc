// Copyright 2022 The Chromium Authors. All rights reserved.
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

  bool wait_completed() const { return wait_completed_; }
  void Reset() { wait_completed_ = false; }

 protected:
  void OnWaitCompleted() override { wait_completed_ = true; }

  bool wait_completed_ = false;
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
  EXPECT_TRUE(client_.wait_completed());
}

TEST_F(PasswordFormPredictionWaiterTest,
       WaitCompletedOnTimeoutWithOutstandingCallback) {
  prediction_waiter_.StartTimer();
  prediction_waiter_.InitializeClosure(2);
  prediction_waiter_.closure().Run();
  EXPECT_FALSE(client_.wait_completed());

  task_environment_.FastForwardBy(kMaxFillingDelayForAsyncPredictions);
  EXPECT_TRUE(client_.wait_completed());
}

TEST_F(PasswordFormPredictionWaiterTest, WaitCompletedOnSingleBarrierCallback) {
  prediction_waiter_.InitializeClosure(1);
  prediction_waiter_.closure().Run();

  EXPECT_TRUE(prediction_waiter_.closure().is_null());
  EXPECT_TRUE(client_.wait_completed());
}

TEST_F(PasswordFormPredictionWaiterTest,
       WaitCompletedOnMultipleBarrierCallbacks) {
  prediction_waiter_.InitializeClosure(3);
  for (int i = 0; i < 3; ++i) {
    prediction_waiter_.closure().Run();
  }

  EXPECT_TRUE(prediction_waiter_.closure().is_null());
  EXPECT_TRUE(client_.wait_completed());
}

}  // namespace

}  // namespace password_manager