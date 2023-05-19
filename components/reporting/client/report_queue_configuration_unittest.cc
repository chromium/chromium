// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_configuration.h"

#include <stdio.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/reporting/util/wrapped_rate_limiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace reporting {
namespace {

constexpr char kDmToken[] = "dm_token";

class MockRateLimiter : public RateLimiterInterface {
 public:
  MOCK_METHOD(bool, Acquire, (size_t event_size), (override));
};

class ReportQueueConfigurationTest : public ::testing::Test {
 protected:
  using PolicyCheckCallback = ReportQueueConfiguration::PolicyCheckCallback;

  const Destination kInvalidDestination = Destination::UNDEFINED_DESTINATION;
  const Destination kValidDestination = Destination::UPLOAD_EVENTS;
  const PolicyCheckCallback kValidCallback = GetSuccessfulCallback();
  const PolicyCheckCallback kInvalidCallback = GetInvalidCallback();

  static PolicyCheckCallback GetSuccessfulCallback() {
    return base::BindRepeating([]() { return Status::StatusOK(); });
  }

  static PolicyCheckCallback GetInvalidCallback() {
    return base::RepeatingCallback<Status(void)>();
  }

  base::test::TaskEnvironment task_environment_;
};

// Tests to ensure that only valid parameters are used to generate a
// ReportQueueConfiguration.
TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithInvalidDestination) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(kDmToken, kInvalidDestination,
                                                kValidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithInvalidDestinationInvalidCallback) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   /*dm_token=*/kDmToken, kInvalidDestination, kInvalidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithValidParams) {
  EXPECT_OK(ReportQueueConfiguration::Create(
      /*dm_token*=*/kDmToken, kValidDestination, kValidCallback));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithNoDMToken) {
  EXPECT_OK(ReportQueueConfiguration::Create(
      /*dm_token*=*/"", kValidDestination, kValidCallback));
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidDestination) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   /*dm_token*=*/"", kInvalidDestination, kValidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidCallback) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   /*dm_token=*/"", kValidDestination, kInvalidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidDestinationInvalidCallback) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   /*dm_token*=*/"", kInvalidDestination, kInvalidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithDeviceEventType) {
  EXPECT_OK(ReportQueueConfiguration::Create(
      EventType::kDevice, kValidDestination, kValidCallback));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithUserEventType) {
  EXPECT_OK(ReportQueueConfiguration::Create(
      EventType::kUser, kValidDestination, kValidCallback));
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidDestination) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   EventType::kDevice, kInvalidDestination, kValidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidCallback) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   EventType::kDevice, kValidDestination, kInvalidCallback)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidReservedSpace) {
  EXPECT_FALSE(ReportQueueConfiguration::Create(
                   EventType::kDevice, kValidDestination, kValidCallback,
                   /*rate_limiter=*/nullptr,
                   /*reserved_space=*/-1L)
                   .ok());
}

TEST_F(ReportQueueConfigurationTest, UsesProvidedPolicyCheckCallback) {
  const Destination destination = Destination::UPLOAD_EVENTS;

  testing::MockFunction<Status(void)> mock_handler;
  EXPECT_CALL(mock_handler, Call()).WillOnce(Return(Status::StatusOK()));

  auto config_result = ReportQueueConfiguration::Create(
      kDmToken, destination,
      base::BindRepeating(&::testing::MockFunction<Status(void)>::Call,
                          base::Unretained(&mock_handler)));
  ASSERT_OK(config_result) << config_result.status();

  const auto config = std::move(config_result.ValueOrDie());
  EXPECT_OK(config->CheckPolicy());
  EXPECT_THAT(config->reserved_space(), Eq(0L));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithRateLimiter) {
  auto rate_limiter = std::make_unique<MockRateLimiter>();
  auto* const mock_rate_limiter = rate_limiter.get();
  auto config_result =
      ReportQueueConfiguration::Create(EventType::kDevice, kValidDestination,
                                       kValidCallback, std::move(rate_limiter));
  ASSERT_OK(config_result) << config_result.status();
  const auto config = std::move(config_result.ValueOrDie());
  const auto is_event_allowed_cb = config->is_event_allowed_cb();
  ASSERT_TRUE(is_event_allowed_cb);

  EXPECT_CALL(*mock_rate_limiter, Acquire(_)).WillOnce(Return(false));
  test::TestEvent<bool> rejected_event;
  is_event_allowed_cb.Run(/*event_size=*/1000, rejected_event.cb());
  EXPECT_FALSE(rejected_event.result());

  EXPECT_CALL(*mock_rate_limiter, Acquire(_)).WillOnce(Return(true));
  test::TestEvent<bool> acquired_event;
  is_event_allowed_cb.Run(/*event_size=*/1, acquired_event.cb());
  EXPECT_TRUE(acquired_event.result());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithRateLimiterAfterRemoval) {
  auto rate_limiter = std::make_unique<MockRateLimiter>();
  auto config_result =
      ReportQueueConfiguration::Create(EventType::kDevice, kValidDestination,
                                       kValidCallback, std::move(rate_limiter));
  ASSERT_OK(config_result) << config_result.status();
  auto config = std::move(config_result.ValueOrDie());
  const auto is_event_allowed_cb = config->is_event_allowed_cb();
  ASSERT_TRUE(is_event_allowed_cb);
  config.reset();

  test::TestEvent<bool> rejected_event;
  is_event_allowed_cb.Run(/*event_size=*/1000, rejected_event.cb());
  EXPECT_FALSE(rejected_event.result());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithReservedSpaceSetting) {
  static constexpr int64_t kReservedSpace = 12345L;
  auto config_result = ReportQueueConfiguration::Create(
      EventType::kDevice, kValidDestination, kValidCallback,
      /*rate_limiter=*/nullptr, kReservedSpace);
  ASSERT_OK(config_result) << config_result.status();

  const auto config = std::move(config_result.ValueOrDie());
  EXPECT_THAT(config->reserved_space(), Eq(kReservedSpace));
}
}  // namespace
}  // namespace reporting
