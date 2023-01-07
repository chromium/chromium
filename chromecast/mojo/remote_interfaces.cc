// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/mojo/remote_interfaces.h"

#include "base/check.h"

namespace chromecast {

RemoteInterfaces::RemoteInterfaces() {
  Init();
  DCHECK(remote_provider_.is_bound());
}

RemoteInterfaces::RemoteInterfaces(
    mojo::PendingRemote<mojom::RemoteInterfaces> provider) {
  Init();
  DCHECK(remote_provider_.is_bound());
  SetProvider(std::move(provider));
}

RemoteInterfaces::~RemoteInterfaces() = default;

void RemoteInterfaces::SetProvider(
    mojo::PendingRemote<mojom::RemoteInterfaces> provider) {
  DCHECK(waiting_receiver_);
  mojo::FusePipes(std::move(waiting_receiver_), std::move(provider));
}

mojo::PendingRemote<mojom::RemoteInterfaces> RemoteInterfaces::Forward() {
  mojo::PendingRemote<mojom::RemoteInterfaces> pending_remote;
  remote_provider_->AddClient(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

mojo::PendingReceiver<mojom::RemoteInterfaces> RemoteInterfaces::GetReceiver() {
  DCHECK(waiting_receiver_);
  return std::move(waiting_receiver_);
}

void RemoteInterfaces::Bind(mojo::GenericPendingReceiver receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Important: We need to copy the interface name before sending it.
  std::string interface_name = *receiver.interface_name();
  remote_provider_->BindInterface(interface_name, receiver.PassPipe());
}

void RemoteInterfaces::Init() {
  if (remote_provider_.is_bound()) {
    LOG(WARNING) << "Remote provider disconnected, reseting...";
    remote_provider_.reset();
  }
  waiting_receiver_ = remote_provider_.BindNewPipeAndPassReceiver();
  remote_provider_.set_disconnect_handler(
      base::BindOnce(&RemoteInterfaces::Init, base::Unretained(this)));
}

}  // namespace chromecast
