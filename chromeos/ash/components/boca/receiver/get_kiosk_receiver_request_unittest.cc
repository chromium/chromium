// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/get_kiosk_receiver_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {

constexpr std::string_view kReceiverId = "receiver_id";
constexpr std::string_view kConnectionId = "connection_id";
constexpr std::string_view kRobotEmail = "robot@email.com";
constexpr std::string_view kReceiverState = "START_REQUESTED";

TEST(GetKioskReceiverRequestTest, RelativeUrlWithConnectionId) {
  GetKioskReceiverRequest request(
      std::string(kReceiverId), std::string(kConnectionId), base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(),
            "/v1/receivers/receiver_id?connectionId=connection_id");
}

TEST(GetKioskReceiverRequestTest, RelativeUrlWithoutConnectionId) {
  GetKioskReceiverRequest request(std::string(kReceiverId),
                                  /*connection_id=*/std::nullopt,
                                  base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(), "/v1/receivers/receiver_id");
}

TEST(GetKioskReceiverRequestTest, OnSuccess) {
  std::optional<::boca::KioskReceiver> response_body;
  GetKioskReceiverRequest request(
      std::string(kReceiverId), std::string(kConnectionId),
      base::BindLambdaForTesting(
          [&response_body](std::optional<::boca::KioskReceiver> response) {
            response_body = std::move(response);
          }));
  base::Value::Dict response_dict;
  response_dict.Set(boca::kRobotEmail, kRobotEmail);
  response_dict.Set(boca::kReceiverConnectionState, kReceiverState);
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(response_body.has_value());
  EXPECT_EQ(response_body->robot_email(), kRobotEmail);
  EXPECT_EQ(response_body->state(), ::boca::START_REQUESTED);
}

TEST(GetKioskReceiverRequestTest, OnError) {
  bool called = false;
  std::optional<::boca::KioskReceiver> response_body;
  GetKioskReceiverRequest request(
      std::string(kReceiverId), std::string(kConnectionId),
      base::BindLambdaForTesting(
          [&response_body,
           &called](std::optional<::boca::KioskReceiver> response) {
            called = true;
            response_body = std::move(response);
          }));

  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_TRUE(called);
  EXPECT_FALSE(response_body.has_value());
}
}  // namespace ash::boca_receiver
