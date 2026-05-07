// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GET_GEMINI_STATUS_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GET_GEMINI_STATUS_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class Value;
}  // namespace base

namespace ash::boca {

class GetGeminiStatusRequest : public BocaRequest::Delegate {
 public:
  using ResponseCallback = base::OnceCallback<void(std::optional<bool>)>;

  static constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "ash_boca_gemini_get_gemini_status_request",
          R"(
        semantics {
          sender: "School Tools"
          description: "Get the Gemini status for the given user."
          trigger: "User opens School Tools app."
          data: "Gaia ID to fetch status for."
          destination: GOOGLE_OWNED_SERVICE
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

  GetGeminiStatusRequest(std::string gaia_id, ResponseCallback callback);

  GetGeminiStatusRequest(const GetGeminiStatusRequest&) = delete;
  GetGeminiStatusRequest& operator=(const GetGeminiStatusRequest&) = delete;

  ~GetGeminiStatusRequest() override;

  // BocaRequest::Delegate:
  std::string GetRelativeUrl() override;
  std::optional<std::string> GetRequestBody() override;
  void OnSuccess(std::unique_ptr<base::Value> response) override;
  void OnError(google_apis::ApiErrorCode error) override;
  google_apis::HttpRequestMethod GetRequestType() const override;

 private:
  std::string gaia_id_;
  ResponseCallback callback_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_GEMINI_GET_GEMINI_STATUS_REQUEST_H_
