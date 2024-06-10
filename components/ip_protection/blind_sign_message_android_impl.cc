// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/blind_sign_message_android_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_message_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

BlindSignMessageAndroidImpl::BlindSignMessageAndroidImpl() = default;

BlindSignMessageAndroidImpl::~BlindSignMessageAndroidImpl() = default;

void BlindSignMessageAndroidImpl::DoRequest(
    quiche::BlindSignMessageRequestType request_type,
    std::optional<std::string_view> authorization_header,
    const std::string& body,
    quiche::BlindSignMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (authorization_header) {
    std::move(callback)(
        absl::InternalError("Failed Request to Android IP Protection Service. "
                            "Authorization header must be empty."));
    return;
  }

  if (ip_protection_auth_client_ != nullptr) {
    CHECK(pending_requests_.empty());
    SendRequest(request_type, body, std::move(callback));
    return;
  }

  pending_requests_.emplace(request_type, body, std::move(callback));
  // If `ip_protection_auth_client_` is not yet set, try
  // to create a connected instance.
  if (pending_requests_.size() == 1u) {
    CreateIpProtectionAuthClient();
  }
}

void BlindSignMessageAndroidImpl::CreateIpProtectionAuthClient() {
  if (skip_create_connected_instance_for_testing_) {
    return;
  }
  ip_protection::android::IpProtectionAuthClient::CreateConnectedInstance(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &BlindSignMessageAndroidImpl::OnCreateIpProtectionAuthClientComplete,
          weak_ptr_factory_.GetWeakPtr())));
}

// TODO(b/328780742): Add support for error handling when service connection
// fails.
void BlindSignMessageAndroidImpl::OnCreateIpProtectionAuthClientComplete(
    base::expected<std::unique_ptr<
                       ip_protection::android::IpProtectionAuthClientInterface>,
                   std::string> ip_protection_auth_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ip_protection_auth_client.has_value()) {
    CHECK(!ip_protection_auth_client_);
    ip_protection_auth_client_ = std::move(ip_protection_auth_client.value());
  }
  while (!pending_requests_.empty()) {
    auto [request_type, body, callback] = std::move(pending_requests_.front());
    if (ip_protection_auth_client_ != nullptr) {
      SendRequest(request_type, body, std::move(callback));
    } else {
      std::move(callback)(absl::InternalError(
          "Failed request to bind to the GmsCore IP Protection service."));
    }
    pending_requests_.pop();
  }
}

void BlindSignMessageAndroidImpl::SendRequest(
    quiche::BlindSignMessageRequestType request_type,
    const std::string& body,
    quiche::BlindSignMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (request_type) {
    case quiche::BlindSignMessageRequestType::kGetInitialData: {
      privacy::ppn::GetInitialDataRequest get_initial_data_request_proto;
      get_initial_data_request_proto.ParseFromString(body);
      ip_protection_auth_client_->GetInitialData(
          get_initial_data_request_proto,
          base::BindOnce(&BlindSignMessageAndroidImpl::OnGetInitialDataComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    }
    case quiche::BlindSignMessageRequestType::kAuthAndSign: {
      privacy::ppn::AuthAndSignRequest auth_and_sign_request_proto;
      auth_and_sign_request_proto.ParseFromString(body);
      ip_protection_auth_client_->AuthAndSign(
          auth_and_sign_request_proto,
          base::BindOnce(&BlindSignMessageAndroidImpl::OnAuthAndSignComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    }
    case quiche::BlindSignMessageRequestType::kUnknown:
      NOTREACHED_NORETURN();
  }
}

// TODO(b/328780742): Add support for persistent and transient error handling.
void BlindSignMessageAndroidImpl::OnGetInitialDataComplete(
    quiche::BlindSignMessageCallback callback,
    base::expected<privacy::ppn::GetInitialDataResponse, std::string>
        response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response.has_value()) {
    std::move(callback)(absl::InternalError(
        "Failed call to Android IP Protection Service for GetInitialData."));
    return;
  }

  OnSendRequestComplete(std::move(callback), response->SerializeAsString());
}

// TODO(b/328780742): Add support for persistent and transient error handling.
void BlindSignMessageAndroidImpl::OnAuthAndSignComplete(
    quiche::BlindSignMessageCallback callback,
    base::expected<privacy::ppn::AuthAndSignResponse, std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response.has_value()) {
    std::move(callback)(absl::InternalError(
        "Failed call to Android IP Protection Service for AuthAndSign."));
    return;
  }

  OnSendRequestComplete(std::move(callback), response->SerializeAsString());
}

// TODO(b/328780742): Implement response code mappings for error handling in
// GMSCore.
void BlindSignMessageAndroidImpl::OnSendRequestComplete(
    quiche::BlindSignMessageCallback callback,
    std::string response_body) {
  quiche::BlindSignMessageResponse bsa_response(absl::StatusCode::kOk,
                                                std::move(response_body));

  std::move(callback)(std::move(bsa_response));
}
