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
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/receiver/kiosk_receiver_parser.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca_receiver {
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
  return base::ReplaceStringPlaceholders(kRelativeUrlTemplate,
                                         {receiver_id_, connection_id_},
                                         /*offsets=*/nullptr);
}

std::optional<std::string> UpdateKioskReceiverStateRequest::GetRequestBody() {
  base::Value::Dict request_body;
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
