// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/get_receiver_connection_info_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kReceiverId = "receiver-id";
constexpr std::string_view kConnectionIdPair =
    R"("connectionId": "connection-id",)";
constexpr std::string_view kConnectionStatePair =
    R"("receiverConnectionState": "CONNECTED",)";
constexpr std::string_view kConnectionDetailsPair =
    R"("connectionDetails": {
          "presenter": {
            "user": {
              "gaiaId": "presenter-gaia",
              "email": "presenter@email.com",
              "fullName": "Presenter Name",
              "photoUrl": "http://presenter"
            },
            "deviceInfo": {
              "deviceId": "presenter-device"
            }
          },
          "initiator": {
            "user": {
              "gaiaId": "initiator-gaia",
              "email": "initiator@email.com",
              "fullName": "Initiator Name",
              "photoUrl": "http://initiator"
            },
            "deviceInfo": {
              "deviceId": "initiator-device"
            }
          },
          "connectionCode": {
            "connectionCode": "123456"
          }
        })";

TEST(GetReceiverConnectionInfoRequestTest, RelativeUrl) {
  GetReceiverConnectionInfoRequest request(kReceiverId, base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(),
            "/v1/receivers/receiver-id/kioskReceiver:getConnectionInfo");
}

TEST(GetReceiverConnectionInfoRequestTest, RelativeUrlWithConnectionId) {
  GetReceiverConnectionInfoRequest request(kReceiverId, "connection-id",
                                           base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(),
            "/v1/receivers/receiver-id/"
            "kioskReceiver:getConnectionInfo?connectionId=connection-id");
}

TEST(GetReceiverConnectionInfoRequestTest, RequestBody) {
  GetReceiverConnectionInfoRequest request(kReceiverId, base::DoNothing());
  EXPECT_FALSE(request.GetRequestBody().has_value());
}

TEST(GetReceiverConnectionInfoRequestTest, OnSuccess) {
  std::optional<::boca::KioskReceiverConnection> response_proto;
  GetReceiverConnectionInfoRequest request(
      kReceiverId,
      base::BindLambdaForTesting(
          [&response_proto](
              std::optional<::boca::KioskReceiverConnection> response) {
            response_proto = std::move(response);
          }));

  std::string response_json =
      base::StrCat({"{", kConnectionIdPair, kConnectionStatePair,
                    kConnectionDetailsPair, "}"});
  std::optional<base::Value> response_value = base::JSONReader::Read(
      response_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  request.OnSuccess(
      std::make_unique<base::Value>(std::move(response_value.value())));

  ASSERT_TRUE(response_proto.has_value());
  EXPECT_EQ(response_proto->connection_id(), "connection-id");
  EXPECT_EQ(response_proto->receiver_connection_state(),
            ::boca::ReceiverConnectionState::CONNECTED);
  EXPECT_EQ(
      response_proto->connection_details().connection_code().connection_code(),
      "123456");
  ::boca::UserIdentity presenter =
      response_proto->connection_details().presenter().user_identity();
  ::boca::UserIdentity initiator =
      response_proto->connection_details().initiator().user_identity();
  EXPECT_EQ(presenter.gaia_id(), "presenter-gaia");
  EXPECT_EQ(presenter.email(), "presenter@email.com");
  EXPECT_EQ(presenter.full_name(), "Presenter Name");
  EXPECT_EQ(initiator.gaia_id(), "initiator-gaia");
  EXPECT_EQ(initiator.email(), "initiator@email.com");
  EXPECT_EQ(initiator.full_name(), "Initiator Name");
}

TEST(GetReceiverConnectionInfoRequestTest, MissinConnectionId) {
  std::optional<::boca::KioskReceiverConnection> response_proto;
  GetReceiverConnectionInfoRequest request(
      kReceiverId,
      base::BindLambdaForTesting(
          [&response_proto](
              std::optional<::boca::KioskReceiverConnection> response) {
            response_proto = std::move(response);
          }));

  std::string response_json =
      base::StrCat({"{", kConnectionStatePair, kConnectionDetailsPair, "}"});
  std::optional<base::Value> response_value = base::JSONReader::Read(
      response_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  request.OnSuccess(
      std::make_unique<base::Value>(std::move(response_value.value())));

  EXPECT_TRUE(response_proto.has_value());
  EXPECT_FALSE(response_proto->has_connection_details());
  EXPECT_TRUE(response_proto->connection_id().empty());
}

TEST(GetReceiverConnectionInfoRequestTest, MissinConnectionState) {
  std::optional<::boca::KioskReceiverConnection> response_proto;
  GetReceiverConnectionInfoRequest request(
      kReceiverId,
      base::BindLambdaForTesting(
          [&response_proto](
              std::optional<::boca::KioskReceiverConnection> response) {
            response_proto = std::move(response);
          }));

  std::string response_json =
      base::StrCat({"{", kConnectionIdPair, kConnectionDetailsPair, "}"});
  std::optional<base::Value> response_value = base::JSONReader::Read(
      response_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  request.OnSuccess(
      std::make_unique<base::Value>(std::move(response_value.value())));

  EXPECT_TRUE(response_proto.has_value());
  EXPECT_FALSE(response_proto->has_connection_details());
  EXPECT_TRUE(response_proto->connection_id().empty());
}

TEST(GetReceiverConnectionInfoRequestTest, EmptyResponse) {
  bool response_callback_called = false;
  std::optional<::boca::KioskReceiverConnection> response_proto;
  GetReceiverConnectionInfoRequest request(
      kReceiverId,
      base::BindLambdaForTesting(
          [&response_proto, &response_callback_called](
              std::optional<::boca::KioskReceiverConnection> response) {
            response_callback_called = true;
            response_proto = std::move(response);
          }));
  request.OnSuccess(std::make_unique<base::Value>());

  EXPECT_TRUE(response_proto.has_value());
  EXPECT_FALSE(response_proto->has_connection_details());
  EXPECT_TRUE(response_proto->connection_id().empty());
  EXPECT_TRUE(response_callback_called);
}

TEST(GetReceiverConnectionInfoRequestTest, OnError) {
  bool response_callback_called = false;
  std::optional<::boca::KioskReceiverConnection> response_proto;
  GetReceiverConnectionInfoRequest request(
      kReceiverId,
      base::BindLambdaForTesting(
          [&response_proto, &response_callback_called](
              std::optional<::boca::KioskReceiverConnection> response) {
            response_callback_called = true;
            response_proto = std::move(response);
          }));
  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_FALSE(response_proto.has_value());
  EXPECT_TRUE(response_callback_called);
}

}  // namespace
}  // namespace ash::boca_receiver
