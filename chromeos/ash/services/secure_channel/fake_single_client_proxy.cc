// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_single_client_proxy.h"

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

namespace ash::secure_channel {

FakeSingleClientProxy::FakeSingleClientProxy(
    Delegate* delegate,
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : SingleClientProxy(delegate),
      proxy_id_(base::UnguessableToken::Create()),
      destructor_callback_(std::move(destructor_callback)) {}

FakeSingleClientProxy::~FakeSingleClientProxy() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run(GetProxyId());
}

const base::UnguessableToken& FakeSingleClientProxy::GetProxyId() {
  return proxy_id_;
}

void FakeSingleClientProxy::HandleReceivedMessage(const std::string& feature,
                                                  const std::string& payload) {
  processed_messages_.push_back(std::make_pair(feature, payload));
}

void FakeSingleClientProxy::HandleRemoteDeviceDisconnection() {
  was_remote_device_disconnection_handled_ = true;
}

void FakeSingleClientProxy::HandleNearbyConnectionStateChanged(
    mojom::NearbyConnectionStep step,
    mojom::NearbyConnectionStepResult result) {
  nearby_connection_step_ = step;
  nearby_connection_step_result_ = result;
}

FakeSingleClientProxyDelegate::FakeSingleClientProxyDelegate() = default;

FakeSingleClientProxyDelegate::~FakeSingleClientProxyDelegate() = default;

void FakeSingleClientProxyDelegate::OnSendMessageRequested(
    const std::string& message_feaure,
    const std::string& message_payload,
    base::OnceClosure on_sent_callback) {
  send_message_requests_.push_back(std::make_tuple(
      message_feaure, message_payload, std::move(on_sent_callback)));
}

void FakeSingleClientProxyDelegate::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  register_payload_file_requests_.emplace(
      payload_id, RegisterPayloadFileRequest(
                      payload_id, std::move(file_transfer_update_callback)));
  std::move(registration_result_callback).Run(register_payload_file_result_);
}

void FakeSingleClientProxyDelegate::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  return std::move(callback).Run(std::move(connection_metadata_for_next_call_));
}

void FakeSingleClientProxyDelegate::OnClientDisconnected(
    const base::UnguessableToken& proxy_id) {
  disconnected_proxy_id_ = proxy_id;

  if (on_client_disconnected_closure_)
    std::move(on_client_disconnected_closure_).Run();
}

}  // namespace ash::secure_channel
