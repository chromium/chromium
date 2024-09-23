// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_

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
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
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
  // A request that has been queued, waiting for an internal
  // IpProtectionAuthClient to become ready.
  //
  // Public for testing.
  using PendingRequest = std::tuple<quiche::BlindSignMessageRequestType,
                                    std::string,
                                    quiche::BlindSignMessageCallback>;
  // Factory signature for creating IpProtectionAuthClient(Interface)s.
  //
  // Public for testing.
  using ClientFactory = void(
      base::OnceCallback<ip_protection::android::
                             IpProtectionAuthClientInterface::ClientCreated>);

  BlindSignMessageAndroidImpl();

  ~BlindSignMessageAndroidImpl() override;

  // quiche::BlindSignMessageInterface implementation:
  void DoRequest(quiche::BlindSignMessageRequestType request_type,
                 std::optional<std::string_view> authorization_header,
                 const std::string& body,
                 quiche::BlindSignMessageCallback callback) override;

  // Set the auth client factory to be used when an auth client needs to be
  // created.
  void SetIpProtectionAuthClientFactoryForTesting(
      base::RepeatingCallback<ClientFactory> factory) {
    CHECK(factory);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    client_factory_ = std::move(factory);
  }

  ip_protection::android::IpProtectionAuthClientInterface*
  GetIpProtectionAuthClientForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ip_protection_auth_client_.get();
  }

  const base::queue<PendingRequest>& GetPendingRequestsForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pending_requests_;
  }

 private:
  using IpProtectionAuthClientInterface =
      ip_protection::android::IpProtectionAuthClientInterface;

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
      base::TimeTicks start_time,
      base::expected<std::unique_ptr<IpProtectionAuthClientInterface>,
                     std::string> ip_protection_auth_client);

  template <typename ResponseType>
  void OnSendRequestComplete(
      base::WeakPtr<IpProtectionAuthClientInterface>
          requesting_ip_protection_auth_client,
      quiche::BlindSignMessageCallback callback,
      base::TimeTicks start_time,
      base::expected<ResponseType, ip_protection::android::AuthRequestError>
          response);

  SEQUENCE_CHECKER(sequence_checker_);

  // The factory method for creating an IpProtectionAuthClient(Interface). Can
  // be reconfigured by tests (See SetIpProtectionAuthClientFactoryForTesting).
  // Must not be null.
  base::RepeatingCallback<ClientFactory> client_factory_;

  std::unique_ptr<IpProtectionAuthClientInterface> ip_protection_auth_client_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Queue of incoming requests waiting for `ip_protection_auth_client_` to
  // connect to the Android IP Protection service. Once an instance is
  // connected, the queue should be empty.
  base::queue<PendingRequest> pending_requests_;

  base::WeakPtrFactory<BlindSignMessageAndroidImpl> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_
