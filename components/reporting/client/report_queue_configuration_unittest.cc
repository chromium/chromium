// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue_configuration.h"

#include <stdio.h>

#include "base/bind.h"
#include "base/callback.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using testing::_;
using testing::Invoke;
using testing::WithArgs;

constexpr char kDmToken[] = "dm_token";

class ReportQueueConfigurationTest : public testing::Test {
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

  ReportQueueConfigurationTest() = default;
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

TEST_F(ReportQueueConfigurationTest, UsesProvidedPolicyCheckCallback) {
  const Destination destination = Destination::UPLOAD_EVENTS;

  testing::MockFunction<Status(void)> mock_handler;
  EXPECT_CALL(mock_handler, Call())
      .WillOnce(::testing::Return(Status::StatusOK()));

  auto config_result = ReportQueueConfiguration::Create(
      kDmToken, destination,
      base::BindRepeating(&testing::MockFunction<Status(void)>::Call,
                          base::Unretained(&mock_handler)));
  EXPECT_OK(config_result);

  auto config = std::move(config_result.ValueOrDie());
  EXPECT_OK(config->CheckPolicy());
}

}  // namespace
}  // namespace reporting
