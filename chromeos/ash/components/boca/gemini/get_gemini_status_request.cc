// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/gemini/get_gemini_status_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca {

GetGeminiStatusRequest::GetGeminiStatusRequest(std::string gaia_id,
                                               ResponseCallback callback)
    : gaia_id_(std::move(gaia_id)), callback_(std::move(callback)) {}

GetGeminiStatusRequest::~GetGeminiStatusRequest() = default;

std::string GetGeminiStatusRequest::GetRelativeUrl() {
  return base::ReplaceStringPlaceholders(kGetGeminiStatusUrlTemplate,
                                         {gaia_id_}, nullptr);
}

std::optional<std::string> GetGeminiStatusRequest::GetRequestBody() {
  return std::nullopt;
}

void GetGeminiStatusRequest::OnSuccess(std::unique_ptr<base::Value> response) {
  CHECK(callback_);
  if (!response) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  const base::DictValue* response_dict = response->GetIfDict();
  if (!response_dict) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  const auto* status_ptr = response_dict->FindString(kGeminiStatus);
  if (status_ptr &&
      (*status_ptr == kGeminiEnabled || *status_ptr == kGeminiDisabled)) {
    std::move(callback_).Run(*status_ptr == kGeminiEnabled);
    return;
  }
  std::move(callback_).Run(std::nullopt);
}

void GetGeminiStatusRequest::OnError(google_apis::ApiErrorCode error) {
  CHECK(callback_);
  std::move(callback_).Run(std::nullopt);
}

google_apis::HttpRequestMethod GetGeminiStatusRequest::GetRequestType() const {
  return google_apis::HttpRequestMethod::kGet;
}

}  // namespace ash::boca
