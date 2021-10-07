// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_MESSAGE_PROXY_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_MESSAGE_PROXY_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/services/secure_channel/register_payload_file_request.h"
#include "chromeos/services/secure_channel/single_client_message_proxy.h"

namespace chromeos {

namespace secure_channel {

// Test SingleClientMessageProxy implementation.
class FakeSingleClientMessageProxy : public SingleClientMessageProxy {
 public:
  FakeSingleClientMessageProxy(
      Delegate* delegate,
      base::OnceCallback<void(const base::UnguessableToken&)>
          destructor_callback =
              base::OnceCallback<void(const base::UnguessableToken&)>());

  FakeSingleClientMessageProxy(const FakeSingleClientMessageProxy&) = delete;
  FakeSingleClientMessageProxy& operator=(const FakeSingleClientMessageProxy&) =
      delete;

  ~FakeSingleClientMessageProxy() override;

  bool was_remote_device_disconnection_handled() {
    return was_remote_device_disconnection_handled_;
  }

  const std::vector<std::pair<std::string, std::string>>& processed_messages() {
    return processed_messages_;
  }

  // SingleClientMessageProxy:
  const base::UnguessableToken& GetProxyId() override;

  // Public for testing.
  using SingleClientMessageProxy::GetConnectionMetadataFromDelegate;
  using SingleClientMessageProxy::NotifyClientDisconnected;
  using SingleClientMessageProxy::NotifySendMessageRequested;
  using SingleClientMessageProxy::RegisterPayloadFileWithDelegate;

 private:
  // SingleClientMessageProxy:
  void HandleReceivedMessage(const std::string& feature,
                             const std::string& payload) override;
  void HandleRemoteDeviceDisconnection() override;

  const base::UnguessableToken proxy_id_;
  base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback_;

  std::vector<std::pair<std::string, std::string>> processed_messages_;
  bool was_remote_device_disconnection_handled_ = false;
};

// Test SingleClientMessageProxy::Delegate implementation.
class FakeSingleClientMessageProxyDelegate
    : public SingleClientMessageProxy::Delegate {
 public:
  FakeSingleClientMessageProxyDelegate();

  FakeSingleClientMessageProxyDelegate(
      const FakeSingleClientMessageProxyDelegate&) = delete;
  FakeSingleClientMessageProxyDelegate& operator=(
      const FakeSingleClientMessageProxyDelegate&) = delete;

  ~FakeSingleClientMessageProxyDelegate() override;

  std::vector<std::tuple<std::string, std::string, base::OnceClosure>>&
  send_message_requests() {
    return send_message_requests_;
  }

  const base::flat_map<int64_t, RegisterPayloadFileRequest>&
  register_payload_file_requests() const {
    return register_payload_file_requests_;
  }

  void set_register_payload_file_result(bool register_payload_file_result) {
    register_payload_file_result_ = register_payload_file_result;
  }

  void set_connection_metadata_for_next_call(
      mojom::ConnectionMetadataPtr connection_metadata_for_next_call) {
    connection_metadata_for_next_call_ =
        std::move(connection_metadata_for_next_call);
  }

  void set_on_client_disconnected_closure(
      base::OnceClosure on_client_disconnected_closure) {
    on_client_disconnected_closure_ = std::move(on_client_disconnected_closure);
  }

  const base::UnguessableToken& disconnected_proxy_id() {
    return disconnected_proxy_id_;
  }

 private:
  // SingleClientMessageProxy::Delegate:
  void OnSendMessageRequested(const std::string& message_feaure,
                              const std::string& message_payload,
                              base::OnceClosure on_sent_callback) override;
  void RegisterPayloadFile(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override;
  void GetConnectionMetadata(
      base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) override;
  void OnClientDisconnected(const base::UnguessableToken& proxy_id) override;

  std::vector<std::tuple<std::string, std::string, base::OnceClosure>>
      send_message_requests_;
  base::flat_map<int64_t, RegisterPayloadFileRequest>
      register_payload_file_requests_;
  bool register_payload_file_result_ = true;
  mojom::ConnectionMetadataPtr connection_metadata_for_next_call_;
  base::OnceClosure on_client_disconnected_closure_;
  base::UnguessableToken disconnected_proxy_id_;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_MESSAGE_PROXY_H_
