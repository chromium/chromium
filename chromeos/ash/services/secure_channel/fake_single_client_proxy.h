// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_PROXY_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_PROXY_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"
#include "chromeos/ash/services/secure_channel/single_client_proxy.h"

namespace ash::secure_channel {

// Test SingleClientProxy implementation.
class FakeSingleClientProxy : public SingleClientProxy {
 public:
  FakeSingleClientProxy(
      Delegate* delegate,
      base::OnceCallback<void(const base::UnguessableToken&)>
          destructor_callback =
              base::OnceCallback<void(const base::UnguessableToken&)>());

  FakeSingleClientProxy(const FakeSingleClientProxy&) = delete;
  FakeSingleClientProxy& operator=(const FakeSingleClientProxy&) = delete;

  ~FakeSingleClientProxy() override;

  bool was_remote_device_disconnection_handled() {
    return was_remote_device_disconnection_handled_;
  }

  const std::vector<std::pair<std::string, std::string>>& processed_messages() {
    return processed_messages_;
  }

  // SingleClientProxy:
  const base::UnguessableToken& GetProxyId() override;

  // Public for testing.
  using SingleClientProxy::GetConnectionMetadataFromDelegate;
  using SingleClientProxy::NotifyClientDisconnected;
  using SingleClientProxy::NotifySendMessageRequested;
  using SingleClientProxy::RegisterPayloadFileWithDelegate;

 private:
  // SingleClientProxy:
  void HandleReceivedMessage(const std::string& feature,
                             const std::string& payload) override;
  void HandleRemoteDeviceDisconnection() override;
  void HandleNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) override;

  const base::UnguessableToken proxy_id_;
  base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback_;

  std::vector<std::pair<std::string, std::string>> processed_messages_;
  bool was_remote_device_disconnection_handled_ = false;
  mojom::NearbyConnectionStep nearby_connection_step_;
  mojom::NearbyConnectionStepResult nearby_connection_step_result_;
};

// Test SingleClientProxy::Delegate implementation.
class FakeSingleClientProxyDelegate : public SingleClientProxy::Delegate {
 public:
  FakeSingleClientProxyDelegate();

  FakeSingleClientProxyDelegate(const FakeSingleClientProxyDelegate&) = delete;
  FakeSingleClientProxyDelegate& operator=(
      const FakeSingleClientProxyDelegate&) = delete;

  ~FakeSingleClientProxyDelegate() override;

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
  // SingleClientProxy::Delegate:
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

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_SINGLE_CLIENT_PROXY_H_
