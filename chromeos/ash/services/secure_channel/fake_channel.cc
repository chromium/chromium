// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_channel.h"

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::secure_channel {

namespace {

const char kDisconnectionDescription[] = "Remote device disconnected.";

}  // namespace

FakeChannel::FakeChannel() = default;

FakeChannel::~FakeChannel() = default;

mojo::PendingRemote<mojom::Channel> FakeChannel::GenerateRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeChannel::DisconnectGeneratedRemote() {
  receiver_.ResetWithReason(mojom::Channel::kConnectionDroppedReason,
                            kDisconnectionDescription);
}

void FakeChannel::SendMessage(const std::string& message,
                              SendMessageCallback callback) {
  sent_messages_.push_back(std::make_pair(message, std::move(callback)));
}

void FakeChannel::RegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    mojo::PendingRemote<mojom::FilePayloadListener> listener,
    RegisterPayloadFileCallback callback) {
  file_payload_listeners_.emplace(payload_id, std::move(listener));
  std::move(callback).Run(/*success=*/true);
}

void FakeChannel::SendFileTransferUpdate(int64_t payload_id,
                                         mojom::FileTransferStatus status,
                                         uint64_t total_bytes,
                                         uint64_t bytes_transferred) {
  file_payload_listeners_.at(payload_id)
      ->OnFileTransferUpdate(mojom::FileTransferUpdate::New(
          payload_id, status, total_bytes, bytes_transferred));
  file_payload_listeners_.at(payload_id).FlushForTesting();
}

void FakeChannel::GetConnectionMetadata(
    GetConnectionMetadataCallback callback) {
  std::move(callback).Run(std::move(connection_metadata_for_next_call_));
}

}  // namespace ash::secure_channel
