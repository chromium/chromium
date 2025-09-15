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

namespace base {
class Value;
}  // namespace base

namespace ash::boca_receiver {

class GetKioskReceiverRequest : public boca::BocaRequest::Delegate {
 public:
  using ResponseCallback =
      base::OnceCallback<void(std::optional<::boca::KioskReceiver>)>;

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
