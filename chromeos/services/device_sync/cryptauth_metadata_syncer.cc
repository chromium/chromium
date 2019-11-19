// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_metadata_syncer.h"

#include <utility>

namespace chromeos {

namespace device_sync {

CryptAuthMetadataSyncer::CryptAuthMetadataSyncer() = default;

CryptAuthMetadataSyncer::~CryptAuthMetadataSyncer() = default;

void CryptAuthMetadataSyncer::SyncMetadata(
    const cryptauthv2::RequestContext& request_context,
    const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
    const CryptAuthKey* initial_group_key,
    SyncMetadataAttemptFinishedCallback callback) {
  // Enforce that SyncMetadata() can only be called once.
  DCHECK(!was_sync_metadata_called_);
  was_sync_metadata_called_ = true;

  callback_ = std::move(callback);

  OnAttemptStarted(request_context, local_device_metadata, initial_group_key);
}

void CryptAuthMetadataSyncer::OnAttemptFinished(
    const IdToDeviceMetadataPacketMap& id_to_device_metadata_packet_map,
    std::unique_ptr<CryptAuthKey> new_group_key,
    const base::Optional<cryptauthv2::EncryptedGroupPrivateKey>&
        encrypted_group_private_key,
    const base::Optional<cryptauthv2::ClientDirective>& new_client_directive,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(callback_);
  std::move(callback_).Run(id_to_device_metadata_packet_map,
                           std::move(new_group_key),
                           encrypted_group_private_key, new_client_directive,
                           device_sync_result_code);
}

}  // namespace device_sync

}  // namespace chromeos
