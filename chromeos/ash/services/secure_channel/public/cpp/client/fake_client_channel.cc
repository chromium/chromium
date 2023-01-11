// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_client_channel.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

FakeClientChannel::FakeClientChannel() = default;

FakeClientChannel::~FakeClientChannel() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run();
}

void FakeClientChannel::InvokePendingGetConnectionMetadataCallback(
    mojom::ConnectionMetadataPtr connection_metadata) {
  std::move(get_connection_metadata_callback_queue_.front())
      .Run(std::move(connection_metadata));
  get_connection_metadata_callback_queue_.pop();
}

void FakeClientChannel::PerformGetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  get_connection_metadata_callback_queue_.push(std::move(callback));
}

void FakeClientChannel::PerformSendMessage(const std::string& payload,
                                           base::OnceClosure on_sent_callback) {
  sent_messages_.push_back(
      std::make_pair(payload, std::move(on_sent_callback)));
}

void FakeClientChannel::PerformRegisterPayloadFile(
    int64_t payload_id,
    mojom::PayloadFilesPtr payload_files,
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>
        file_transfer_update_callback,
    base::OnceCallback<void(bool)> registration_result_callback) {
  registered_file_payloads_.push_back(payload_id);
  std::move(registration_result_callback).Run(/*success=*/true);
}

}  // namespace ash::secure_channel
