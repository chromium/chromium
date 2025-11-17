// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_KIOSK_RECEIVER_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_KIOSK_RECEIVER_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class Value;
}  // namespace base

namespace ash::boca_receiver {

class GetKioskReceiverRequest : public boca::BocaRequest::Delegate {
 public:
  using ResponseCallback =
      base::OnceCallback<void(std::optional<::boca::KioskReceiver>)>;

  static constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
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

  GetKioskReceiverRequest(std::string receiver_id,
                          std::optional<std::string> connection_id,
                          ResponseCallback callback);
  GetKioskReceiverRequest(const GetKioskReceiverRequest&) = delete;
  GetKioskReceiverRequest& operator=(const GetKioskReceiverRequest&) = delete;
  ~GetKioskReceiverRequest() override;

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

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_GET_KIOSK_RECEIVER_REQUEST_H_
