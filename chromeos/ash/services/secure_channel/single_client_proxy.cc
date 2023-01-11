// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/single_client_proxy.h"

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

SingleClientProxy::SingleClientProxy(Delegate* delegate)
    : delegate_(delegate) {}

SingleClientProxy::~SingleClientProxy() = default;

void SingleClientProxy::NotifySendMessageRequested(
    const std::string& message_feature,
    const std::string& message_payload,
    base::OnceClosure on_sent_callback) {
  delegate_->OnSendMessageRequested(message_feature, message_payload,
                                    std::move(on_sent_callback));
}

void SingleClientProxy::NotifyClientDisconnected() {
  delegate_->OnClientDisconnected(GetProxyId());
}

void SingleClientProxy::RegisterPayloadFileWithDelegate(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  delegate_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                 std::move(file_transfer_update_callback),
                                 std::move(registration_result_callback));
}

void SingleClientProxy::GetConnectionMetadataFromDelegate(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  delegate_->GetConnectionMetadata(std::move(callback));
}

}  // namespace ash::secure_channel
