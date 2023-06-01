// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/wrapped_rate_limiter.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace reporting {
namespace {

class MockRateLimiter : public RateLimiterInterface {
 public:
  MOCK_METHOD(bool, Acquire, (size_t /*event_size*/), (override));
};

class WrappedRateLimiterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto rate_limiter = std::make_unique<MockRateLimiter>();
    mock_rate_limiter_ = rate_limiter.get();
    wrapped_rate_limiter_ = WrappedRateLimiter::Create(std::move(rate_limiter));
    async_acquire_cb_ = wrapped_rate_limiter_->async_acquire_cb();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  WrappedRateLimiter::SmartPtr wrapped_rate_limiter_{
      nullptr, base::OnTaskRunnerDeleter(nullptr)};
  WrappedRateLimiter::AsyncAcquireCb async_acquire_cb_;
  raw_ptr<MockRateLimiter> mock_rate_limiter_;
};

TEST_F(WrappedRateLimiterTest, SuccessfulAcquire) {
  EXPECT_CALL(*mock_rate_limiter_, Acquire(_)).WillOnce(Return(true));
  test::TestEvent<bool> acuire_event;
  async_acquire_cb_.Run(123U, acuire_event.cb());
  EXPECT_TRUE(acuire_event.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  WrappedRateLimiter::kRateLimitedEventsUma),
              UnorderedElementsAre(base::Bucket(true, 1)));
}

TEST_F(WrappedRateLimiterTest, RejectedAcquire) {
  EXPECT_CALL(*mock_rate_limiter_, Acquire(_)).WillOnce(Return(false));
  test::TestEvent<bool> acuire_event;
  async_acquire_cb_.Run(123U, acuire_event.cb());
  EXPECT_FALSE(acuire_event.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  WrappedRateLimiter::kRateLimitedEventsUma),
              UnorderedElementsAre(base::Bucket(false, 1)));
}

TEST_F(WrappedRateLimiterTest, MultipleCalls) {
  EXPECT_CALL(*mock_rate_limiter_, Acquire(_))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  {
    test::TestEvent<bool> acuire_event;
    async_acquire_cb_.Run(123U, acuire_event.cb());
    EXPECT_TRUE(acuire_event.result());
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    WrappedRateLimiter::kRateLimitedEventsUma),
                UnorderedElementsAre(base::Bucket(true, 1)));
  }
  {
    test::TestEvent<bool> acuire_event;
    async_acquire_cb_.Run(123U, acuire_event.cb());
    EXPECT_FALSE(acuire_event.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            WrappedRateLimiter::kRateLimitedEventsUma),
        UnorderedElementsAre(base::Bucket(true, 1), base::Bucket(false, 1)));
  }
  {
    test::TestEvent<bool> acuire_event;
    async_acquire_cb_.Run(123U, acuire_event.cb());
    EXPECT_TRUE(acuire_event.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            WrappedRateLimiter::kRateLimitedEventsUma),
        UnorderedElementsAre(base::Bucket(true, 2), base::Bucket(false, 1)));
  }
  {
    test::TestEvent<bool> acuire_event;
    async_acquire_cb_.Run(123U, acuire_event.cb());
    EXPECT_FALSE(acuire_event.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            WrappedRateLimiter::kRateLimitedEventsUma),
        UnorderedElementsAre(base::Bucket(true, 2), base::Bucket(false, 2)));
  }
  {
    test::TestEvent<bool> acuire_event;
    async_acquire_cb_.Run(123U, acuire_event.cb());
    EXPECT_TRUE(acuire_event.result());
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(
            WrappedRateLimiter::kRateLimitedEventsUma),
        UnorderedElementsAre(base::Bucket(true, 3), base::Bucket(false, 2)));
  }
}

TEST_F(WrappedRateLimiterTest, AcquireAfterDestruction) {
  test::TestEvent<bool> acuire_event;
  mock_rate_limiter_ = nullptr;
  wrapped_rate_limiter_.reset();
  async_acquire_cb_.Run(123U, acuire_event.cb());
  EXPECT_FALSE(acuire_event.result());
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  WrappedRateLimiter::kRateLimitedEventsUma),
              UnorderedElementsAre(base::Bucket(false, 1)));
}
}  // namespace
}  // namespace reporting
