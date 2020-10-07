// Copyright 2020 The Chromium Authors. All rights reserved.
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

void RemoteInterfaces::Init() {
  waiting_receiver_ = remote_provider_.BindNewPipeAndPassReceiver();
  remote_provider_.set_disconnect_handler(
      base::BindOnce(&RemoteInterfaces::Init, base::Unretained(this)));
}

}  // namespace chromecast
