// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/channel_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::secure_channel {

namespace {

const char kReasonForDisconnection[] = "Remote device disconnected.";

}  // namespace

ChannelImpl::ChannelImpl(Delegate* delegate) : delegate_(delegate) {}

ChannelImpl::~ChannelImpl() = default;

mojo::PendingRemote<mojom::Channel> ChannelImpl::GenerateRemote() {
  // Only one PendingRemote should be generated from this instance.
  DCHECK(!receiver_.is_bound());

  mojo::PendingRemote<mojom::Channel> interface_remote =
      receiver_.BindNewPipeAndPassRemote();

  receiver_.set_disconnect_handler(base::BindOnce(
      &ChannelImpl::OnBindingDisconnected, base::Unretained(this)));

  return interface_remote;
}

void ChannelImpl::HandleRemoteDeviceDisconnection() {
  DCHECK(receiver_.is_bound());

  // If the RemoteDevice disconnected, alert clients by providing them a
  // reason specific to this event.
  receiver_.ResetWithReason(mojom::Channel::kConnectionDroppedReason,
                            kReasonForDisconnection);

  file_payload_listener_remotes_.Clear();
}

void ChannelImpl::SendMessage(const std::string& message,
                              SendMessageCallback callback) {
  delegate_->OnSendMessageRequested(message, std::move(callback));
}

void ChannelImpl::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    mojo::PendingRemote<mojom::FilePayloadListener> listener,
    RegisterPayloadFileCallback callback) {
  mojo::RemoteSetElementId remote_id =
      file_payload_listener_remotes_.Add(std::move(listener));

  delegate_->RegisterPayloadFile(
      payload_id, std::move(payload_files),
      base::BindRepeating(&ChannelImpl::NotifyFileTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr(), remote_id),
      base::BindOnce(&ChannelImpl::OnRegisterPayloadFileResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     remote_id));
}

void ChannelImpl::OnRegisterPayloadFileResult(
    RegisterPayloadFileCallback callback,
    mojo::RemoteSetElementId listener_remote_id,
    bool success) {
  if (!success) {
    file_payload_listener_remotes_.Remove(listener_remote_id);
  }
  std::move(callback).Run(success);
}

void ChannelImpl::NotifyFileTransferUpdate(
    mojo::RemoteSetElementId listener_remote_id,
    mojom::FileTransferUpdatePtr update) {
  bool is_transfer_complete =
      update->status != mojom::FileTransferStatus::kInProgress;
  file_payload_listener_remotes_.Get(listener_remote_id)
      ->OnFileTransferUpdate(std::move(update));

  if (is_transfer_complete) {
    file_payload_listener_remotes_.Remove(listener_remote_id);
  }
}

void ChannelImpl::GetConnectionMetadata(
    GetConnectionMetadataCallback callback) {
  delegate_->GetConnectionMetadata(
      base::BindOnce(&ChannelImpl::OnConnectionMetadataFetchedFromDelegate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ChannelImpl::OnConnectionMetadataFetchedFromDelegate(
    GetConnectionMetadataCallback callback,
    mojom::ConnectionMetadataPtr connection_metadata_from_delegate) {
  std::move(callback).Run(std::move(connection_metadata_from_delegate));
}

void ChannelImpl::OnBindingDisconnected() {
  delegate_->OnClientDisconnected();
}

}  // namespace ash::secure_channel
