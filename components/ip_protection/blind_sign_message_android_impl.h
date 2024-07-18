// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_
#define COMPONENTS_IP_PROTECTION_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_message_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace ip_protection {

// Uses the IpProtectionAuthClient to make IPC calls to service implementing IP
// Protection for async requests in BlindSignAuth. DoRequest makes an IPC
// request for either GetInitialData or AuthAndSign and if successful, receives
// a response body which is returned in a BlindSignMessageResponse along with a
// status_code of `absl::StatusCode::kOk`. An AuthRequestError is returned if
// otherwise and is mapped to an `absl::Status`.
//
// AuthRequestError will either be transient, persistent, or other (some failure
// not explicitly communicated by the service). AuthRequestError::kTransient
// maps to absl::Unavailable given that the client can retry the failing call.
// AuthRequestError::kPersistent maps to absl::FailedPreconditionError
// indicating that the request cannot be retried. AuthRequestError::kOther
// is for all other errors that are unexpected and therefore maps to
// absl::Unavailable so the request can be retried with backoff.
//
// See go/canonical-codes for more information on error codes.
class BlindSignMessageAndroidImpl : public quiche::BlindSignMessageInterface {
 public:
  BlindSignMessageAndroidImpl();

  ~BlindSignMessageAndroidImpl() override;

  // quiche::BlindSignMessageInterface implementation:
  void DoRequest(quiche::BlindSignMessageRequestType request_type,
                 std::optional<std::string_view> authorization_header,
                 const std::string& body,
                 quiche::BlindSignMessageCallback callback) override;

 private:
  friend class BlindSignMessageAndroidImplTest;

  FRIEND_TEST_ALL_PREFIXES(BlindSignMessageAndroidImplTest, SetUp);
  FRIEND_TEST_ALL_PREFIXES(BlindSignMessageAndroidImplTest,
                           RequestsAreQueuedUntilConnectedInstance);
  FRIEND_TEST_ALL_PREFIXES(
      BlindSignMessageAndroidImplTest,
      DoRequestReturnsInternalErrorIfFailureToBindToService);
  FRIEND_TEST_ALL_PREFIXES(
      BlindSignMessageAndroidImplTest,
      RetryCreateConnectedInstanceOnNextRequestfServiceDisconnected);
  FRIEND_TEST_ALL_PREFIXES(BlindSignMessageAndroidImplTest,
                           DoRequestHandlesOtherErrors);

  using IpProtectionAuthClientInterface =
      ip_protection::android::IpProtectionAuthClientInterface;
  using PendingRequest = std::tuple<quiche::BlindSignMessageRequestType,
                                    std::string,
                                    quiche::BlindSignMessageCallback>;

  // Request to bind to the Android IP Protection service by creating a
  // connected instance of `ip_protection_auth_client_`.
  void CreateIpProtectionAuthClient();

  // Makes either a GetInitialDataRequest or AuthAndSignRequest to the signing
  // server using `ip_protection_auth_client_`.
  void SendRequest(quiche::BlindSignMessageRequestType request_type,
                   const std::string& body,
                   quiche::BlindSignMessageCallback callback);

  // Processes queued requests once `ip_protection_auth_client_` becomes
  // available.
  void ProcessPendingRequests();

  void OnCreateIpProtectionAuthClientComplete(
      base::expected<std::unique_ptr<IpProtectionAuthClientInterface>,
                     std::string> ip_protection_auth_client);

  void OnGetInitialDataComplete(
      quiche::BlindSignMessageCallback callback,
      base::expected<privacy::ppn::GetInitialDataResponse,
                     ip_protection::android::AuthRequestError> response);

  void OnAuthAndSignComplete(
      quiche::BlindSignMessageCallback callback,
      base::expected<privacy::ppn::AuthAndSignResponse,
                     ip_protection::android::AuthRequestError> response);

  template <typename ResponseType>
  void OnSendRequestComplete(
      base::WeakPtr<IpProtectionAuthClientInterface>
          requesting_ip_protection_auth_client,
      quiche::BlindSignMessageCallback callback,
      base::expected<ResponseType, ip_protection::android::AuthRequestError>
          response);

  // Set the `ip_protection_auth_client_` for testing.
  void SetIpProtectionAuthClientForTesting(
      std::unique_ptr<IpProtectionAuthClientInterface>
          ip_protection_auth_client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ip_protection_auth_client_ = std::move(ip_protection_auth_client);
  }

  IpProtectionAuthClientInterface* GetIpProtectionAuthClientForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ip_protection_auth_client_.get();
  }

  void SkipCreateConnectedInstanceForTesting() {
    skip_create_connected_instance_for_testing_ = true;
  }

  base::queue<PendingRequest>& GetPendingRequestsForTesting() {
    return pending_requests_;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<IpProtectionAuthClientInterface> ip_protection_auth_client_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Queue of incoming requests waiting for `ip_protection_auth_client_` to
  // connect to the Android IP Protection service. Once an instance is
  // connected, the queue should be empty.
  base::queue<PendingRequest> pending_requests_;

  bool skip_create_connected_instance_for_testing_ = false;

  base::WeakPtrFactory<BlindSignMessageAndroidImpl> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_
