// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

constexpr char kReceiverId[] = "test_receiver_id";
constexpr char kConnectionId[] = "test_connection_id";

TEST(UpdateKioskReceiverStateRequestTest, GetRelativeUrl) {
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(),
            "/v1/receivers/test_receiver_id/connections/"
            "test_connection_id:updateState");
}

TEST(UpdateKioskReceiverStateRequestTest, GetRequestBody) {
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::DoNothing());
  std::optional<std::string> request_body = request.GetRequestBody();
  ASSERT_TRUE(request_body.has_value());
  std::optional<base::Value> value = base::JSONReader::Read(
      *request_body, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const base::Value::Dict& dict = value->GetDict();
  const std::string* state = dict.FindString("state");
  ASSERT_TRUE(state);
  EXPECT_EQ(*state, "CONNECTED");
}

TEST(UpdateKioskReceiverStateRequestTest, OnSuccess) {
  std::optional<::boca::ReceiverConnectionState> received_state;
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::BindLambdaForTesting(
          [&received_state](
              std::optional<::boca::ReceiverConnectionState> state) {
            received_state = state;
          }));

  base::Value::Dict response_dict;
  response_dict.Set("state", "CONNECTED");
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(received_state.has_value());
  EXPECT_EQ(received_state.value(), ::boca::ReceiverConnectionState::CONNECTED);
}

TEST(UpdateKioskReceiverStateRequestTest, OnSuccessInvalidResponse) {
  bool callback_called = false;
  std::optional<::boca::ReceiverConnectionState> received_state;
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::BindLambdaForTesting(
          [&received_state, &callback_called](
              std::optional<::boca::ReceiverConnectionState> state) {
            received_state = state;
            callback_called = true;
          }));

  base::Value::Dict response_dict;
  response_dict.Set("wrong_key", "CONNECTED");
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(received_state.has_value());
}

TEST(UpdateKioskReceiverStateRequestTest, OnError) {
  std::optional<::boca::ReceiverConnectionState> received_state;
  bool callback_called = false;
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::BindLambdaForTesting(
          [&received_state, &callback_called](
              std::optional<::boca::ReceiverConnectionState> state) {
            received_state = state;
            callback_called = true;
          }));

  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(received_state.has_value());
}

TEST(UpdateKioskReceiverStateRequestTest, GetRequestType) {
  UpdateKioskReceiverStateRequest request(
      kReceiverId, kConnectionId, ::boca::ReceiverConnectionState::CONNECTED,
      base::DoNothing());
  EXPECT_EQ(request.GetRequestType(), google_apis::HttpRequestMethod::kPatch);
}

}  // namespace
}  // namespace ash::boca_receiver
