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

// Uses the IpProtectionAuthClient to make IPC calls to Service implementing IP
// Protection.
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
      base::expected<
          std::unique_ptr<
              ip_protection::android::IpProtectionAuthClientInterface>,
          std::string> ip_protection_auth_client);

  void OnGetInitialDataComplete(
      quiche::BlindSignMessageCallback callback,
      base::expected<privacy::ppn::GetInitialDataResponse, std::string>
          response);

  void OnAuthAndSignComplete(
      quiche::BlindSignMessageCallback callback,
      base::expected<privacy::ppn::AuthAndSignResponse, std::string> response);

  void OnSendRequestComplete(quiche::BlindSignMessageCallback callback,
                             std::string response);

  // Set the `ip_protection_auth_client_` for testing.
  void SetIpProtectionAuthClientForTesting(
      std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
          ip_protection_auth_client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ip_protection_auth_client_ = std::move(ip_protection_auth_client);
  }

  void SkipCreateConnectedInstanceForTesting() {
    skip_create_connected_instance_for_testing_ = true;
  }

  base::queue<std::tuple<quiche::BlindSignMessageRequestType,
                         std::string,
                         quiche::BlindSignMessageCallback>>&
  GetPendingRequestsForTesting() {
    return pending_requests_;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<ip_protection::android::IpProtectionAuthClientInterface>
      ip_protection_auth_client_ GUARDED_BY_CONTEXT(sequence_checker_) =
          nullptr;

  // Queue of incoming requests waiting for `ip_protection_auth_client_` to
  // connect to the Android IP Protection service. Once an instance is
  // connected, the queue should be empty.
  base::queue<std::tuple<quiche::BlindSignMessageRequestType,
                         std::string,
                         quiche::BlindSignMessageCallback>>
      pending_requests_;

  bool skip_create_connected_instance_for_testing_ = false;

  base::WeakPtrFactory<BlindSignMessageAndroidImpl> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_IP_PROTECTION_BLIND_SIGN_MESSAGE_ANDROID_IMPL_H_
