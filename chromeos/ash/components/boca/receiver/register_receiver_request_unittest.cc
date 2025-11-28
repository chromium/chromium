// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kFcmToken = "fcm_token";
constexpr std::string_view kReceiverId = "AB12";

TEST(RegisterReceiverRequestTest, RelativeUrl) {
  RegisterReceiverRequest request(kFcmToken, base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(), "/v1/kioskReceiver:register");
}

TEST(RegisterReceiverRequestTest, RequestBody) {
  RegisterReceiverRequest request(kFcmToken, base::DoNothing());
  std::optional<std::string> request_body = request.GetRequestBody();
  ASSERT_TRUE(request_body.has_value());
  std::optional<base::Value::Dict> request_dict = base::JSONReader::ReadDict(
      request_body.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(request_dict.has_value());
  ASSERT_THAT(request_dict.value().FindString("token"), testing::NotNull());
  EXPECT_EQ(*request_dict.value().FindString("token"), kFcmToken);
  EXPECT_EQ(request_dict.value().size(), 1ul);
}

TEST(RegisterReceiverRequestTest, OnSuccess) {
  std::optional<std::string> response_body;
  RegisterReceiverRequest request(
      kFcmToken, base::BindLambdaForTesting(
                     [&response_body](std::optional<std::string> response) {
                       response_body = std::move(response);
                     }));
  base::Value::Dict response_dict;
  response_dict.Set("receiverId", kReceiverId);
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(response_body.has_value());
  EXPECT_EQ(response_body.value(), kReceiverId);
}

TEST(RegisterReceiverRequestTest, OnSuccess_InvalidType) {
  bool called = false;
  std::optional<std::string> response_body;
  RegisterReceiverRequest request(
      kFcmToken,
      base::BindLambdaForTesting(
          [&response_body, &called](std::optional<std::string> response) {
            called = true;
            response_body = std::move(response);
          }));
  request.OnSuccess(std::make_unique<base::Value>("invalid type"));

  EXPECT_TRUE(called);
  EXPECT_FALSE(response_body.has_value());
}

TEST(RegisterReceiverRequestTest, OnSuccess_ReceiverIdMissing) {
  bool called = false;
  std::optional<std::string> response_body;
  RegisterReceiverRequest request(
      kFcmToken,
      base::BindLambdaForTesting(
          [&response_body, &called](std::optional<std::string> response) {
            called = true;
            response_body = std::move(response);
          }));
  base::Value::Dict response_dict;
  response_dict.Set("invalid_key", kReceiverId);
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  EXPECT_TRUE(called);
  EXPECT_FALSE(response_body.has_value());
}

TEST(RegisterReceiverRequestTest, OnError) {
  bool called = false;
  std::optional<std::string> response_body;
  RegisterReceiverRequest request(
      kFcmToken,
      base::BindLambdaForTesting(
          [&response_body, &called](std::optional<std::string> response) {
            called = true;
            response_body = std::move(response);
          }));
  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_TRUE(called);
  EXPECT_FALSE(response_body.has_value());
}

}  // namespace
}  // namespace ash::boca_receiver
