// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/single_client_message_proxy.h"

#include "base/callback.h"
#include "chromeos/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace chromeos {

namespace secure_channel {

SingleClientMessageProxy::SingleClientMessageProxy(Delegate* delegate)
    : delegate_(delegate) {}

SingleClientMessageProxy::~SingleClientMessageProxy() = default;

void SingleClientMessageProxy::NotifySendMessageRequested(
    const std::string& message_feature,
    const std::string& message_payload,
    base::OnceClosure on_sent_callback) {
  delegate_->OnSendMessageRequested(message_feature, message_payload,
                                    std::move(on_sent_callback));
}

void SingleClientMessageProxy::NotifyClientDisconnected() {
  delegate_->OnClientDisconnected(GetProxyId());
}

void SingleClientMessageProxy::RegisterPayloadFileWithDelegate(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    FileTransferUpdateCallback file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  delegate_->RegisterPayloadFile(payload_id, std::move(payload_files),
                                 std::move(file_transfer_update_callback),
                                 std::move(registration_result_callback));
}

void SingleClientMessageProxy::GetConnectionMetadataFromDelegate(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  delegate_->GetConnectionMetadata(std::move(callback));
}

}  // namespace secure_channel

}  // namespace chromeos
