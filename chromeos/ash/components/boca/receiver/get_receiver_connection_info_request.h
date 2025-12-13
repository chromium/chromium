// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_RECEIVER_CONNECTION_INFO_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_RECEIVER_CONNECTION_INFO_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "google_apis/common/api_error_codes.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace boca {
class KioskReceiverConnection;
}  // namespace boca

namespace base {
class Value;
}  // namespace base

namespace ash::boca_receiver {

class GetReceiverConnectionInfoRequest : public boca::BocaRequest::Delegate {
 public:
  using ResponseCallback =
      base::OnceCallback<void(std::optional<::boca::KioskReceiverConnection>)>;

  static constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "ash_boca_receiver_get_receiver_connection_info_request",
          R"(
        semantics {
          sender: "School Tools"
          description: "Get the recent connection info to the receiver."
          trigger: "Device starts in School Tools kiosk receiver mode."
          data: "Device OAuth token for verification and receiver id."
          destination: GOOGLE_OWNED_SERVICE
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

  static constexpr std::string_view kRelativeUrlTemplate =
      "/v1/receivers/$1/kioskReceiver:getConnectionInfo";
  static constexpr std::string_view kConnectionIdQueryParam = "connectionId=$1";

  GetReceiverConnectionInfoRequest(std::string_view receiver_id,
                                   ResponseCallback callback);

  GetReceiverConnectionInfoRequest(std::string_view receiver_id,
                                   std::optional<std::string> connection_id,
                                   ResponseCallback callback);

  GetReceiverConnectionInfoRequest(const GetReceiverConnectionInfoRequest&) =
      delete;
  GetReceiverConnectionInfoRequest& operator=(
      const GetReceiverConnectionInfoRequest&) = delete;

  ~GetReceiverConnectionInfoRequest() override;

  // boca::BocaRequest::Delegate:
  std::string GetRelativeUrl() override;
  std::optional<std::string> GetRequestBody() override;
  void OnSuccess(std::unique_ptr<base::Value> response) override;
  void OnError(google_apis::ApiErrorCode error) override;
  google_apis::HttpRequestMethod GetRequestType() const override;

 private:
  std::string receiver_id_;
  std::optional<std::string> connection_id_;
  ResponseCallback callback_;
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_RECEIVER_CONNECTION_INFO_REQUEST_H_
