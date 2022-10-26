// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/register_payload_file_request.h"

#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

RegisterPayloadFileRequest::RegisterPayloadFileRequest(
    int64_t payload_id,
    FileTransferUpdateCallback file_transfer_update_callback)
    : payload_id(payload_id),
      file_transfer_update_callback(std::move(file_transfer_update_callback)) {}

RegisterPayloadFileRequest::RegisterPayloadFileRequest(
    RegisterPayloadFileRequest&&) = default;

RegisterPayloadFileRequest& RegisterPayloadFileRequest::operator=(
    RegisterPayloadFileRequest&&) = default;

RegisterPayloadFileRequest::~RegisterPayloadFileRequest() = default;

}  // namespace ash::secure_channel
