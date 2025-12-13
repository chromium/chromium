// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace ash::boca_receiver {
namespace {

constexpr std::string_view kFcmTokenKey = "token";
constexpr std::string_view kReceiverIdKey = "receiverId";

}  // namespace

RegisterReceiverRequest::RegisterReceiverRequest(std::string_view fcm_token,
                                                 ResponseCallback callback)
    : fcm_token_(fcm_token), callback_(std::move(callback)) {}

RegisterReceiverRequest::~RegisterReceiverRequest() = default;

std::string RegisterReceiverRequest::GetRelativeUrl() {
  return std::string(kUrl);
}

std::optional<std::string> RegisterReceiverRequest::GetRequestBody() {
  base::Value::Dict request;
  request.Set(kFcmTokenKey, fcm_token_);
  return base::WriteJson(request).value_or("");
}

void RegisterReceiverRequest::OnSuccess(std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  if (response->is_dict() &&
      response->GetIfDict()->FindString(kReceiverIdKey)) {
    std::string receiver_id =
        *response->GetIfDict()->FindString(kReceiverIdKey);
    std::move(callback_).Run(std::move(receiver_id));
    return;
  }
  std::move(callback_).Run(std::nullopt);
}

void RegisterReceiverRequest::OnError(google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod RegisterReceiverRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kPost;
}

}  // namespace ash::boca_receiver
