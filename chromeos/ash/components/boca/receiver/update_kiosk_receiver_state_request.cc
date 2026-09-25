// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"
#include "google_apis/common/api_error_codes.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::boca_receiver {

// static
const net::NetworkTrafficAnnotationTag
    UpdateKioskReceiverStateRequest::kTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "ash_boca_receiver_update_kiosk_receiver_state_request",
            R"(
        semantics {
          sender: "School Tools"
          description: "Update the connection state of a kiosk receiver."
          trigger: "Connection state changes at kiosk receiver side or teacher"
                  " requests stopping the connection"
          data: "Device OAuth token for verification, receiver id, "
                "connection id and the new state."
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
          last_reviewed: "2025-09-15"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings, but will only "
                    "be sent if the device set to kiosk mode with the School "
                    "Tools receiver URL set or if the teacher requests to "
                    "stop the connection."
          policy_exception_justification: "Not implemented."
        })");

namespace {

constexpr char kStateKey[] = "state";

}  // namespace

UpdateKioskReceiverStateRequest::UpdateKioskReceiverStateRequest(
    std::string receiver_id,
    std::string connection_id,
    ::boca::ReceiverConnectionState connection_state,
    ResponseCallback callback)
    : receiver_id_(std::move(receiver_id)),
      connection_id_(std::move(connection_id)),
      connection_state_(connection_state),
      callback_(std::move(callback)) {}

UpdateKioskReceiverStateRequest::~UpdateKioskReceiverStateRequest() = default;

std::string UpdateKioskReceiverStateRequest::GetRelativeUrl() {
  return base::ReplaceStringPlaceholders(
      kRelativeUrlTemplate,
      {base::EscapeAllExceptUnreserved(receiver_id_), connection_id_},
      /*offsets=*/nullptr);
}

std::optional<std::string> UpdateKioskReceiverStateRequest::GetRequestBody() {
  base::DictValue request_body;
  request_body.Set(kStateKey,
                   ReceiverConnectionStateStringFromProto(connection_state_));
  return base::WriteJson(request_body);
}

void UpdateKioskReceiverStateRequest::OnSuccess(
    std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  if (response->is_dict() && response->GetDict().FindString(kStateKey)) {
    const std::string* state = response->GetDict().FindString(kStateKey);
    std::move(callback_).Run(ReceiverConnectionStateProtoFromJson(*state));
    return;
  }
  std::move(callback_).Run(std::nullopt);
}

void UpdateKioskReceiverStateRequest::OnError(google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod UpdateKioskReceiverStateRequest::GetRequestType()
    const {
  return google_apis::HttpRequestMethod::kPatch;
}

}  // namespace ash::boca_receiver
