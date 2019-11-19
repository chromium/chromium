// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/channel_impl.h"

#include "base/bind.h"

namespace chromeos {

namespace secure_channel {

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
}

void ChannelImpl::SendMessage(const std::string& message,
                              SendMessageCallback callback) {
  delegate_->OnSendMessageRequested(message, std::move(callback));
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

}  // namespace secure_channel

}  // namespace chromeos
