// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_REGISTER_RECEIVER_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_REGISTER_RECEIVER_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "google_apis/common/api_error_codes.h"

namespace base {
class Value;
}  // namespace base

namespace ash::boca_receiver {

class RegisterReceiverRequest : public boca::BocaRequest::Delegate {
 public:
  using ResponseCallback = base::OnceCallback<void(std::optional<std::string>)>;

  RegisterReceiverRequest(std::string_view fcm_token,
                          ResponseCallback callback);

  RegisterReceiverRequest(const RegisterReceiverRequest&) = delete;
  RegisterReceiverRequest& operator=(const RegisterReceiverRequest&) = delete;

  ~RegisterReceiverRequest() override;

  // boca::BocaRequest::Delegate:
  std::string GetRelativeUrl() override;
  std::optional<std::string> GetRequestBody() override;
  void OnSuccess(std::unique_ptr<base::Value> response) override;
  void OnError(google_apis::ApiErrorCode error) override;

 private:
  std::string fcm_token_;
  ResponseCallback callback_;
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_REGISTER_RECEIVER_REQUEST_H_
