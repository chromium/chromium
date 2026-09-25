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
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash::boca {

// static
const net::NetworkTrafficAnnotationTag
    GetGeminiStatusRequest::kTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "ash_boca_gemini_get_gemini_status_request",
            R"(
        semantics {
          sender: "School Tools"
          description: "Get the Gemini status for the given user."
          trigger: "User opens School Tools app."
          data: "Gaia ID to fetch status for."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: ACCESS_TOKEN
            type: GAIA_ID
          }
          internal {
            contacts {
              email: "cros-edu-eng@google.com"
            }
          }
          last_reviewed: "2026-05-06"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be stopped in settings."
          policy_exception_justification: "Not implemented."
        })");

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

  const auto* status_ptr = response_dict->FindString(kGeminiEnablementState);
  if (status_ptr && (*status_ptr == kGeminiStateEnabled ||
                     *status_ptr == kGeminiStateDisabled)) {
    std::move(callback_).Run(*status_ptr == kGeminiStateEnabled);
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
