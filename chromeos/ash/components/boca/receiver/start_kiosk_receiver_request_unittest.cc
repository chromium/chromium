// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"

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
constexpr std::string_view kSessionId = "session_id";
constexpr std::string_view kConnectionCode = "connection_code";
constexpr std::string_view kInitiatorDeviceId = "init_device_id";
constexpr std::string_view kInitiatorGaiaId = "init_gaia";
constexpr std::string_view kInitiatorEmail = "init_email@google.com";
constexpr std::string_view kInitiatorFullName = "init_full_name";
constexpr std::string_view kInitiatorPhotoUrl = "init_photo_url";
constexpr std::string_view kPresenterDeviceId = "pres_device_id";
constexpr std::string_view kPresenterGaiaId = "pres_gaia";
constexpr std::string_view kPresenterEmail = "pres_email@google.com";
constexpr std::string_view kPresenterFullName = "pres_full_name";
constexpr std::string_view kPresenterPhotoUrl = "pres_photo_url";

TEST(StartKioskReceiverRequestTest, RelativeUrl) {
  ::boca::UserIdentity initiator;
  ::boca::UserIdentity presenter;
  StartKioskReceiverRequest request(
      std::string(kReceiverId), initiator, presenter,
      std::string(kInitiatorDeviceId), std::string(kPresenterDeviceId),
      std::string(kConnectionCode), std::string(kSessionId), base::DoNothing());
  EXPECT_EQ(request.GetRelativeUrl(), "/v1/receivers/receiver_id:start");
}

TEST(StartKioskReceiverRequestTest, GetRequestBody) {
  std::optional<std::string> response_body;
  ::boca::ConnectionParameter connection_param;
  connection_param.set_connection_code(kConnectionCode);

  ::boca::UserIdentity initiator;
  initiator.set_gaia_id(kInitiatorGaiaId);
  initiator.set_email(kInitiatorEmail);
  initiator.set_full_name(kInitiatorFullName);
  initiator.set_photo_url(kInitiatorPhotoUrl);

  ::boca::UserIdentity presenter;
  presenter.set_gaia_id(kPresenterGaiaId);
  presenter.set_email(kPresenterEmail);
  presenter.set_full_name(kPresenterFullName);
  presenter.set_photo_url(kPresenterPhotoUrl);

  StartKioskReceiverRequest request(
      std::string(kReceiverId), initiator, presenter,
      std::string(kInitiatorDeviceId), std::string(kPresenterDeviceId),
      std::string(kConnectionCode), std::string(kSessionId), base::DoNothing());

  std::optional<std::string> request_body = request.GetRequestBody();
  ASSERT_TRUE(request_body.has_value());
  std::optional<base::Value::Dict> request_dict = base::JSONReader::ReadDict(
      request_body.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(request_dict.has_value());
  EXPECT_EQ(*request_dict.value().FindString(boca::kSessionId), kSessionId);

  base::DictValue* connection_details_dict =
      request_dict.value().FindDict(boca::kConnection);
  base::DictValue* connection_code =
      connection_details_dict->FindDict(boca::kConnectionCode);
  EXPECT_EQ(*connection_code->FindString(boca::kConnectionCode),
            kConnectionCode);

  base::DictValue* initiator_dict =
      connection_details_dict->FindDict(boca::kInitiator);
  EXPECT_EQ(*initiator_dict->FindDict(boca::kUser)->FindString(boca::kGaiaId),
            kInitiatorGaiaId);
  EXPECT_EQ(*initiator_dict->FindDict(boca::kUser)->FindString(boca::kEmail),
            kInitiatorEmail);
  EXPECT_EQ(*initiator_dict->FindDict(boca::kUser)->FindString(boca::kFullName),
            kInitiatorFullName);
  EXPECT_EQ(*initiator_dict->FindDict(boca::kUser)->FindString(boca::kPhotoUrl),
            kInitiatorPhotoUrl);
  EXPECT_EQ(
      *initiator_dict->FindDict(boca::kDevice)->FindString(boca::kDeviceId),
      kInitiatorDeviceId);

  base::DictValue* presenter_dict =
      connection_details_dict->FindDict(boca::kPresenter);
  EXPECT_EQ(*presenter_dict->FindDict(boca::kUser)->FindString(boca::kGaiaId),
            kPresenterGaiaId);
  EXPECT_EQ(*presenter_dict->FindDict(boca::kUser)->FindString(boca::kEmail),
            kPresenterEmail);
  EXPECT_EQ(*presenter_dict->FindDict(boca::kUser)->FindString(boca::kFullName),
            kPresenterFullName);
  EXPECT_EQ(*presenter_dict->FindDict(boca::kUser)->FindString(boca::kPhotoUrl),
            kPresenterPhotoUrl);
  EXPECT_EQ(
      *presenter_dict->FindDict(boca::kDevice)->FindString(boca::kDeviceId),
      kPresenterDeviceId);
}

TEST(StartKioskReceiverRequestTest, OnSuccess) {
  std::optional<std::string> response_body;
  ::boca::UserIdentity initiator;
  ::boca::UserIdentity presenter;
  StartKioskReceiverRequest request(
      std::string(kReceiverId), initiator, presenter,
      std::string(kInitiatorDeviceId), std::string(kPresenterDeviceId),
      std::string(kConnectionCode), std::string(kSessionId),
      base::BindLambdaForTesting(
          [&response_body](std::optional<std::string> response) {
            response_body = std::move(response);
          }));
  base::Value::Dict response_dict;
  response_dict.Set(boca::kConnectionId, "connection_id");
  request.OnSuccess(std::make_unique<base::Value>(std::move(response_dict)));

  ASSERT_TRUE(response_body.has_value());
  EXPECT_EQ(response_body.value(), "connection_id");
}

TEST(StartKioskReceiverRequestTest, OnError) {
  bool called = false;
  std::optional<std::string> response_body;
  ::boca::UserIdentity initiator;
  ::boca::UserIdentity presenter;
  StartKioskReceiverRequest request(
      std::string(kReceiverId), initiator, presenter,
      std::string(kInitiatorDeviceId), std::string(kPresenterDeviceId),
      std::string(kConnectionCode), std::string(kSessionId),
      base::BindLambdaForTesting(
          [&response_body, &called](std::optional<std::string> response) {
            called = true;
            response_body = std::move(response);
          }));

  request.OnError(google_apis::ApiErrorCode::HTTP_NOT_FOUND);

  EXPECT_TRUE(called);
  EXPECT_FALSE(response_body.has_value());
}
}  // namespace ash::boca_receiver
