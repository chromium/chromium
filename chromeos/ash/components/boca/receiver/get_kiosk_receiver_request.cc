// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/get_kiosk_receiver_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::boca_receiver {

// static
const net::NetworkTrafficAnnotationTag
    GetKioskReceiverRequest::kTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "ash_boca_receiver_get_kiosk_receiver_request",
            R"(
        semantics {
          sender: "School Tools"
          description: "Get the kiosk receiver information."
          trigger: "Connection state changes at kiosk receiver side or teacher"
                  " requests starting the connection"
          data: "Kiosk receiver information and connection state."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: ACCESS_TOKEN
            type: DEVICE_ID
          }
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2025-09-09"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will only "
                    "be sent if the device set to kiosk mode with the School "
                    "Tools receiver URL set."
          policy_exception_justification: "Not implemented."
        })");

GetKioskReceiverRequest::GetKioskReceiverRequest(
    std::string receiver_id,
    std::optional<std::string> connection_id,
    ResponseCallback callback)
    : receiver_id_(std::move(receiver_id)),
      connection_id_(std::move(connection_id)),
      callback_(std::move(callback)) {}

GetKioskReceiverRequest::~GetKioskReceiverRequest() = default;

std::string GetKioskReceiverRequest::GetRelativeUrl() {
  std::string escaped_receiver_id =
      base::EscapeAllExceptUnreserved(receiver_id_);
  if (!connection_id_) {
    return base::ReplaceStringPlaceholders(
        boca::kGetKioskReceiverWithoutConnectionIdUrlTemplate,
        {escaped_receiver_id}, nullptr);
  }
  return base::ReplaceStringPlaceholders(
      boca::kGetKioskReceiverUrlTemplate,
      {escaped_receiver_id, connection_id_.value()}, nullptr);
}

std::optional<std::string> GetKioskReceiverRequest::GetRequestBody() {
  return std::nullopt;
}

void GetKioskReceiverRequest::OnSuccess(std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  if (!response->is_dict()) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  auto kiosk_receiver_dict = std::move(response->GetIfDict());

  ::boca::KioskReceiver receiver;
  if (auto* robot_email_ptr =
          kiosk_receiver_dict->FindString(boca::kRobotEmail)) {
    receiver.set_robot_email(*robot_email_ptr);
  }
  if (auto* state_ptr =
          kiosk_receiver_dict->FindString(boca::kReceiverConnectionState)) {
    receiver.set_state(ReceiverConnectionStateProtoFromJson(*state_ptr));
  }

  std::move(callback_).Run(std::move(receiver));
}

void GetKioskReceiverRequest::OnError(google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod GetKioskReceiverRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kGet;
}

}  // namespace ash::boca_receiver
