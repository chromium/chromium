// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_UPDATE_KIOSK_RECEIVER_STATE_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_UPDATE_KIOSK_RECEIVER_STATE_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class Value;
}  // namespace base

namespace ash::boca_receiver {

class UpdateKioskReceiverStateRequest : public boca::BocaRequest::Delegate {
 public:
  using ResponseCallback =
      base::OnceCallback<void(std::optional<::boca::ReceiverConnectionState>)>;

  static constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "ash_boca_receiver_get_receiver_connection_info_request",
          R"(
        semantics {
          sender: "School Tools"
          description: "Update the connection state of a kiosk receiver."
          trigger: "Connection state changes at kiosk receiver side or teacher"
                  " requests stopping the connection"
          data: "Device OAuth token for verification, receiver id, "
                "connection id and the new state."
          destination: GOOGLE_OWNED_SERVICE
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

  static constexpr std::string_view kRelativeUrlTemplate =
      "/v1/receivers/$1/connections/$2:updateState";

  UpdateKioskReceiverStateRequest(
      std::string receiver_id,
      std::string connection_id,
      ::boca::ReceiverConnectionState connection_state,
      ResponseCallback callback);

  UpdateKioskReceiverStateRequest(const UpdateKioskReceiverStateRequest&) =
      delete;
  UpdateKioskReceiverStateRequest& operator=(
      const UpdateKioskReceiverStateRequest&) = delete;

  ~UpdateKioskReceiverStateRequest() override;

  // boca::BocaRequest::Delegate:
  std::string GetRelativeUrl() override;
  std::optional<std::string> GetRequestBody() override;
  void OnSuccess(std::unique_ptr<base::Value> response) override;
  void OnError(google_apis::ApiErrorCode error) override;
  google_apis::HttpRequestMethod GetRequestType() const override;

 private:
  const std::string receiver_id_;
  const std::string connection_id_;
  const ::boca::ReceiverConnectionState connection_state_;
  ResponseCallback callback_;
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_UPDATE_KIOSK_RECEIVER_STATE_REQUEST_H_
