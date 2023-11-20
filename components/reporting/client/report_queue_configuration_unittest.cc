// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_configuration.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/reporting/util/wrapped_rate_limiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
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
  StatusOr<std::unique_ptr<ReportQueueConfiguration>> config_result =
      ReportQueueConfiguration::Builder({.destination = kInvalidDestination})
          .SetDMToken(kDmToken)
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithInvalidDestinationInvalidCallback) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kInvalidDestination})
          .SetPolicyCheckCallback(kInvalidCallback)
          .SetDMToken(kDmToken)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithValidParams) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kValidDestination})
          .SetDMToken(kDmToken)
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_OK(config_result) << config_result.error();
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithNoDMToken) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_OK(config_result) << config_result.error();
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidDestination) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kInvalidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidCallback) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kValidDestination})
          .SetPolicyCheckCallback(kInvalidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithNoDMTokenInvalidDestinationInvalidCallback) {
  auto config_result =
      ReportQueueConfiguration::Create({.destination = kInvalidDestination})
          .SetPolicyCheckCallback(kInvalidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithDeviceEventType) {
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_OK(config_result) << config_result.error();
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithUserEventType) {
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_OK(config_result) << config_result.error();
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidDestination) {
  auto config_result =
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = kInvalidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidCallback) {
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice, .destination = kValidDestination})
          .SetPolicyCheckCallback(kInvalidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithEventTypeInvalidReservedSpace) {
  auto config_result =
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = kValidDestination,
                                        .reserved_space = -1L})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest, UsesProvidedPolicyCheckCallback) {
  const Destination destination = Destination::UPLOAD_EVENTS;

  testing::MockFunction<Status(void)> mock_handler;
  EXPECT_CALL(mock_handler, Call()).WillOnce(Return(Status::StatusOK()));

  auto config_result =
      ReportQueueConfiguration::Create({.destination = destination})
          .SetDMToken(kDmToken)
          .SetPolicyCheckCallback(
              base::BindRepeating(&::testing::MockFunction<Status(void)>::Call,
                                  base::Unretained(&mock_handler)))
          .Build();
  ASSERT_OK(config_result) << config_result.error();

  const auto config = std::move(config_result.value());
  EXPECT_OK(config->CheckPolicy());
  EXPECT_THAT(config->reserved_space(), Eq(0L));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithRateLimiter) {
  auto rate_limiter = std::make_unique<MockRateLimiter>();
  auto* const mock_rate_limiter = rate_limiter.get();
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .SetRateLimiter(std::move(rate_limiter))
          .Build();
  ASSERT_TRUE(config_result.has_value()) << config_result.error();
  const auto config = std::move(config_result.value());
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
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kDevice, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .SetRateLimiter(std::move(rate_limiter))
          .Build();
  ASSERT_TRUE(config_result.has_value()) << config_result.error();
  auto config = std::move(config_result.value());
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
  auto config_result =
      ReportQueueConfiguration::Create({.event_type = EventType::kDevice,
                                        .destination = kValidDestination,
                                        .reserved_space = kReservedSpace})
          .SetPolicyCheckCallback(kValidCallback)
          .Build();
  ASSERT_TRUE(config_result.has_value()) << config_result.error();

  const auto config = std::move(config_result.value());
  EXPECT_THAT(config->reserved_space(), Eq(kReservedSpace));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithSource) {
  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .SetSourceInfo(source_info)
          .Build();
  ASSERT_TRUE(config_result.has_value()) << config_result.error();

  const auto config = std::move(config_result.value());
  ASSERT_TRUE(config->source_info().has_value());
  EXPECT_THAT(config->source_info().value().source(), Eq(source_info.source()));
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithUnsetSource) {
  SourceInfo source_info;
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .SetSourceInfo(std::move(source_info))
          .Build();
  EXPECT_FALSE(config_result.has_value());
}

TEST_F(ReportQueueConfigurationTest, ValidateConfigurationWithSourceVersion) {
  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  source_info.set_source_version("1.0.0");
  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = EventType::kUser, .destination = kValidDestination})
          .SetPolicyCheckCallback(kValidCallback)
          .SetSourceInfo(source_info)
          .Build();
  ASSERT_TRUE(config_result.has_value()) << config_result.error();

  const auto config = std::move(config_result.value());
  ASSERT_TRUE(config->source_info().has_value());
  const auto config_source_info = config->source_info().value();
  EXPECT_THAT(config_source_info.source(), Eq(source_info.source()));
  EXPECT_THAT(config_source_info.source_version(),
              StrEq(source_info.source_version()));
}

TEST_F(ReportQueueConfigurationTest,
       ValidateConfigurationWithSourceVersionAndInvalidSource) {
  SourceInfo source_info;
  source_info.set_source(SourceInfo::SOURCE_UNSPECIFIED);
  source_info.set_source_version("1.0.0");
  auto config_result =
      ReportQueueConfiguration::Create({
                                           .event_type = EventType::kUser,
                                           .destination = kValidDestination,
                                       })
          .SetPolicyCheckCallback(kValidCallback)
          .SetSourceInfo(std::move(source_info))
          .Build();
  EXPECT_FALSE(config_result.has_value());
}
}  // namespace
}  // namespace reporting
