// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android/blind_sign_message_android_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/containers/queue.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_message_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

namespace {

template <typename ResponseType>
void EmitRequestTimingHistogram(base::TimeDelta time_delta);

template <>
void EmitRequestTimingHistogram<privacy::ppn::GetInitialDataResponse>(
    base::TimeDelta time_delta) {
  Telemetry().AndroidAuthClientGetInitialDataTime(time_delta);
}

template <>
void EmitRequestTimingHistogram<privacy::ppn::AuthAndSignResponse>(
    base::TimeDelta time_delta) {
  Telemetry().AndroidAuthClientAuthAndSignTime(time_delta);
}

}  // namespace

BlindSignMessageAndroidImpl::BlindSignMessageAndroidImpl()
    : client_factory_(
          base::BindRepeating(&ip_protection::android::IpProtectionAuthClient::
                                  CreateConnectedInstance)) {}

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
  // If `ip_protection_auth_client_` is not yet set, try to create a new
  // connected instance.
  if (pending_requests_.size() == 1u) {
    CreateIpProtectionAuthClient();
  }
}

void BlindSignMessageAndroidImpl::CreateIpProtectionAuthClient() {
  TRACE_EVENT0("toplevel",
               "BlindSignMessageAndroidImpl::CreateIpProtectionAuthClient");
  client_factory_.Run(base::BindPostTaskToCurrentDefault(base::BindOnce(
      &BlindSignMessageAndroidImpl::OnCreateIpProtectionAuthClientComplete,
      weak_ptr_factory_.GetWeakPtr(), /*start_time=*/base::TimeTicks::Now())));
}

void BlindSignMessageAndroidImpl::OnCreateIpProtectionAuthClientComplete(
    base::TimeTicks start_time,
    base::expected<std::unique_ptr<IpProtectionAuthClientInterface>,
                   std::string> ip_protection_auth_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ip_protection_auth_client.has_value()) {
    CHECK(!ip_protection_auth_client_);
    ip_protection_auth_client_ = std::move(ip_protection_auth_client.value());
    Telemetry().AndroidAuthClientCreationTime(base::TimeTicks::Now() -
                                              start_time);
  }
  while (!pending_requests_.empty()) {
    auto [request_type, body, callback] = std::move(pending_requests_.front());
    if (ip_protection_auth_client_ != nullptr) {
      SendRequest(request_type, body, std::move(callback));
    } else {
      std::move(callback)(absl::InternalError(
          "Failed request to bind to the Android IP Protection service."));
    }
    pending_requests_.pop();
  }
}

void BlindSignMessageAndroidImpl::SendRequest(
    quiche::BlindSignMessageRequestType request_type,
    const std::string& body,
    quiche::BlindSignMessageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("toplevel", "BlindSignMessageAndroidImpl::SendRequest");
  switch (request_type) {
    case quiche::BlindSignMessageRequestType::kGetInitialData: {
      privacy::ppn::GetInitialDataRequest get_initial_data_request_proto;
      get_initial_data_request_proto.ParseFromString(body);
      ip_protection_auth_client_->GetInitialData(
          get_initial_data_request_proto,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &BlindSignMessageAndroidImpl::OnSendRequestComplete<
                  privacy::ppn::GetInitialDataResponse>,
              weak_ptr_factory_.GetWeakPtr(),
              ip_protection_auth_client_->GetWeakPtr(), std::move(callback),
              /*start_time=*/base::TimeTicks::Now())));
      break;
    }
    case quiche::BlindSignMessageRequestType::kAuthAndSign: {
      privacy::ppn::AuthAndSignRequest auth_and_sign_request_proto;
      auth_and_sign_request_proto.ParseFromString(body);
      ip_protection_auth_client_->AuthAndSign(
          auth_and_sign_request_proto,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &BlindSignMessageAndroidImpl::OnSendRequestComplete<
                  privacy::ppn::AuthAndSignResponse>,
              weak_ptr_factory_.GetWeakPtr(),
              ip_protection_auth_client_->GetWeakPtr(), std::move(callback),
              /*start_time=*/base::TimeTicks::Now())));
      break;
    }
    case quiche::BlindSignMessageRequestType::kUnknown:
      NOTREACHED();
  }
}

template <typename ResponseType>
void BlindSignMessageAndroidImpl::OnSendRequestComplete(
    base::WeakPtr<IpProtectionAuthClientInterface>
        requesting_ip_protection_auth_client,
    quiche::BlindSignMessageCallback callback,
    base::TimeTicks start_time,
    base::expected<ResponseType, ip_protection::android::AuthRequestError>
        response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response.has_value()) {
    EmitRequestTimingHistogram<ResponseType>(base::TimeTicks::Now() -
                                             start_time);
    quiche::BlindSignMessageResponse bsa_response(
        absl::StatusCode::kOk, response->SerializeAsString());
    std::move(callback)(std::move(bsa_response));
  } else {
    switch (response.error()) {
      case ip_protection::android::AuthRequestError::kPersistent: {
        std::move(callback)(absl::FailedPreconditionError(
            "Persistent error when making request to the service implementing "
            "IP Protection."));
        break;
      }
      case ip_protection::android::AuthRequestError::kTransient: {
        std::move(callback)(
            absl::UnavailableError("Transient error when making request to the "
                                   "service implementing IP Protection"));
        break;
      }
      case ip_protection::android::AuthRequestError::kOther:
        // `kOther` error may indicate that the service became disconnected
        // during the request. Because binding succeeded previously, reset the
        // `ip_protection_auth_client_` only if the current client is
        // responsible for the request.
        if (requesting_ip_protection_auth_client) {
          CHECK(requesting_ip_protection_auth_client.get() ==
                ip_protection_auth_client_.get());
          CHECK(pending_requests_.empty());
          ip_protection_auth_client_.reset();
        }
        std::move(callback)(absl::InternalError(
            "An internal error where there is no longer a connection to the "
            "Android IP Protection service during a request."));
        break;
    }
  }
}

}  // namespace ip_protection
