// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_file_payload_listener.h"

#include <vector>

#include "base/functional/bind.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::secure_channel {

FakeFilePayloadListener::FakeFilePayloadListener() = default;

FakeFilePayloadListener::~FakeFilePayloadListener() = default;

mojo::PendingRemote<mojom::FilePayloadListener>
FakeFilePayloadListener::GenerateRemote() {
  mojo::PendingRemote<mojom::FilePayloadListener> pending_remote =
      receiver_.BindNewPipeAndPassRemote();
  is_connected_ = true;
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeFilePayloadListener::OnDisconnect, base::Unretained(this)));
  return pending_remote;
}

void FakeFilePayloadListener::OnDisconnect() {
  is_connected_ = false;
}

void FakeFilePayloadListener::OnFileTransferUpdate(
    mojom::FileTransferUpdatePtr update) {
  received_updates_.push_back(std::move(update));
}

}  // namespace ash::secure_channel
