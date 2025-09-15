// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca_receiver {

namespace {
base::Value::Dict getUserDeviceInfoDict(::boca::UserIdentity user,
                                        std::string device_id) {
  base::Value::Dict user_info;
  user_info.Set(boca::kGaiaId, user.gaia_id());
  user_info.Set(boca::kEmail, user.email());
  user_info.Set(boca::kFullName, user.full_name());
  user_info.Set(boca::kPhotoUrl, user.photo_url());

  base::Value::Dict device_info;
  device_info.Set(boca::kDeviceId, device_id);

  base::Value::Dict user_device_info;
  user_device_info.Set(boca::kUser, std::move(user_info));
  user_device_info.Set(boca::kDevice, std::move(device_info));

  return user_device_info;
}
}  // namespace

StartKioskReceiverRequest::StartKioskReceiverRequest(
    std::string receiver_id,
    ::boca::UserIdentity initiator,
    ::boca::UserIdentity presenter,
    std::string initiator_device_id,
    std::string presenter_device_id,
    std::optional<std::string> connection_code,
    std::optional<std::string> session_id,
    ResponseCallback callback)
    : receiver_id_(std::move(receiver_id)),
      initiator_(std::move(initiator)),
      presenter_(std::move(presenter)),
      initiator_device_id_(std::move(initiator_device_id)),
      presenter_device_id_(std::move(presenter_device_id)),
      connection_code_(std::move(connection_code)),
      session_id_(std::move(session_id)),
      callback_(std::move(callback)) {}

StartKioskReceiverRequest::~StartKioskReceiverRequest() = default;

std::string StartKioskReceiverRequest::GetRelativeUrl() {
  return base::ReplaceStringPlaceholders(boca::kStartKioskReceiverUrlTemplate,
                                         {receiver_id_}, nullptr);
}

std::optional<std::string> StartKioskReceiverRequest::GetRequestBody() {
  std::string request_body;
  base::Value::Dict request;
  base::Value::Dict connection_details;

  if (connection_code_) {
    base::Value::Dict connection_code;
    connection_code.Set(boca::kConnectionCode, connection_code_.value());
    connection_details.Set(boca::kConnectionCode, std::move(connection_code));
  }

  base::Value::Dict initiator =
      getUserDeviceInfoDict(initiator_, initiator_device_id_);

  base::Value::Dict presenter =
      getUserDeviceInfoDict(presenter_, presenter_device_id_);

  connection_details.Set(boca::kInitiator, std::move(initiator));
  connection_details.Set(boca::kPresenter, std::move(presenter));

  request.Set(boca::kConnection, std::move(connection_details));

  if (session_id_) {
    request.Set(boca::kSessionId, session_id_.value());
  }

  base::JSONWriter::Write(request, &request_body);
  return request_body;
}

void StartKioskReceiverRequest::OnSuccess(
    std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  if (response->is_dict() &&
      response->GetIfDict()->FindString(boca::kConnectionId)) {
    std::string connection_id =
        *response->GetIfDict()->FindString(boca::kConnectionId);
    std::move(callback_).Run(std::move(connection_id));
    return;
  }
  std::move(callback_).Run(std::nullopt);
}

void StartKioskReceiverRequest::OnError(google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod StartKioskReceiverRequest::GetRequestType()
    const {
  return google_apis::HttpRequestMethod::kPost;
}

}  // namespace ash::boca_receiver
