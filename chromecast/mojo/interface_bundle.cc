// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/mojo/interface_bundle.h"

#include <utility>

namespace chromecast {

InterfaceBundle::InterfaceBundle() = default;

InterfaceBundle::~InterfaceBundle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InterfaceBundle::Close() {
  client_receivers_.Clear();
}

mojo::PendingRemote<mojom::RemoteInterfaces> InterfaceBundle::CreateRemote() {
  mojo::PendingRemote<mojom::RemoteInterfaces> pending_remote;
  AddClient(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void InterfaceBundle::BindInterface(const std::string& interface_name,
                                    mojo::ScopedMessagePipeHandle handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_interfaces_.HasInterface(interface_name)) {
    local_interfaces_.Bind(interface_name, std::move(handle));
    return;
  }
  LOG(WARNING) << "Interface '" << interface_name << "' is not exposed by this "
               << "bundle, but a consumer tried to bind it.";
}

void InterfaceBundle::AddClient(
    mojo::PendingReceiver<mojom::RemoteInterfaces> receiver) {
  client_receivers_.Add(this, std::move(receiver));
}

}  // namespace chromecast
