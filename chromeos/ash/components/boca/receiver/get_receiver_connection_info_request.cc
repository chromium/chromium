// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/get_receiver_connection_info_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"
#include "chromeos/ash/components/boca/session_api/json_proto_converters.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kUserIdentityKey = "user";
constexpr std::string_view kDeviceInfoKey = "deviceInfo";
constexpr std::string_view kDeviceIdKey = "deviceId";
constexpr std::string_view kConnectionCodeKey = "connectionCode";
constexpr std::string_view kConnectionIdKey = "connectionId";
constexpr std::string_view kReceiverConnectionStateKey =
    "receiverConnectionState";
constexpr std::string_view kPresenterKey = "presenter";
constexpr std::string_view kInitiatorKey = "initiator";
constexpr std::string_view kConnectionDetailsKey = "connectionDetails";

void ConvertUserDeviceInfo(const base::Value::Dict* dict,
                           ::boca::UserDeviceInfo* user_device_info) {
  if (!dict) {
    return;
  }

  if (const base::Value::Dict* user_identity_dict =
          dict->FindDict(kUserIdentityKey)) {
    *user_device_info->mutable_user_identity() =
        boca::ConvertUserIdentityJsonToProto(user_identity_dict);
  }

  if (dict->FindDict(kDeviceInfoKey) &&
      dict->FindDict(kDeviceInfoKey)->FindString(kDeviceIdKey)) {
    user_device_info->mutable_device_info()->set_device_id(
        *dict->FindDict(kDeviceInfoKey)->FindString(kDeviceIdKey));
  }
}

void ConvertConnectionDetails(const base::Value::Dict* dict,
                              ::boca::ConnectionDetails* connection_details) {
  if (!dict) {
    return;
  }

  ConvertUserDeviceInfo(dict->FindDict(kPresenterKey),
                        connection_details->mutable_presenter());
  ConvertUserDeviceInfo(dict->FindDict(kInitiatorKey),
                        connection_details->mutable_initiator());

  if (dict->FindDict(kConnectionCodeKey) &&
      dict->FindDict(kConnectionCodeKey)->FindString(kConnectionCodeKey)) {
    connection_details->mutable_connection_code()->set_connection_code(
        *dict->FindDict(kConnectionCodeKey)->FindString(kConnectionCodeKey));
  }
}

::boca::KioskReceiverConnection ConvertKioskReceiverConnection(
    const base::Value& value) {
  ::boca::KioskReceiverConnection connection;
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict || !dict->FindString(kConnectionIdKey) ||
      !dict->FindString(kReceiverConnectionStateKey)) {
    return connection;
  }
  connection.set_connection_id(*dict->FindString(kConnectionIdKey));
  connection.set_receiver_connection_state(ReceiverConnectionStateProtoFromJson(
      *dict->FindString(kReceiverConnectionStateKey)));
  ConvertConnectionDetails(dict->FindDict(kConnectionDetailsKey),
                           connection.mutable_connection_details());
  return connection;
}

}  // namespace

GetReceiverConnectionInfoRequest::GetReceiverConnectionInfoRequest(
    std::string_view receiver_id,
    ResponseCallback callback)
    : GetReceiverConnectionInfoRequest(receiver_id,
                                       /*connection_id=*/std::nullopt,
                                       std::move(callback)) {}

GetReceiverConnectionInfoRequest::GetReceiverConnectionInfoRequest(
    std::string_view receiver_id,
    std::optional<std::string> connection_id,
    ResponseCallback callback)
    : receiver_id_(receiver_id),
      connection_id_(std::move(connection_id)),
      callback_(std::move(callback)) {}

GetReceiverConnectionInfoRequest::~GetReceiverConnectionInfoRequest() = default;

std::string GetReceiverConnectionInfoRequest::GetRelativeUrl() {
  constexpr std::string_view kQueryStringPrefix = "?";
  const std::string url = base::ReplaceStringPlaceholders(
      kRelativeUrlTemplate, {receiver_id_}, /*offsets=*/nullptr);
  if (!connection_id_.has_value()) {
    return url;
  }
  const std::string connection_id_query_param = base::ReplaceStringPlaceholders(
      kConnectionIdQueryParam, {connection_id_.value()},
      /*offsets=*/nullptr);
  return base::StrCat({url, kQueryStringPrefix, connection_id_query_param});
}

std::optional<std::string> GetReceiverConnectionInfoRequest::GetRequestBody() {
  return std::nullopt;
}

void GetReceiverConnectionInfoRequest::OnSuccess(
    std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  CHECK(response);
  std::move(callback_).Run(ConvertKioskReceiverConnection(*response));
}

void GetReceiverConnectionInfoRequest::OnError(
    google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod
GetReceiverConnectionInfoRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kGet;
}

}  // namespace ash::boca_receiver
