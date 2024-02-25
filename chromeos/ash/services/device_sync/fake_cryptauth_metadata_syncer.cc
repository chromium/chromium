// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_metadata_syncer.h"

namespace ash {

namespace device_sync {

FakeCryptAuthMetadataSyncer::FakeCryptAuthMetadataSyncer() = default;

FakeCryptAuthMetadataSyncer::~FakeCryptAuthMetadataSyncer() = default;

void FakeCryptAuthMetadataSyncer::FinishAttempt(
    const IdToDeviceMetadataPacketMap& id_to_device_metadata_packet_map,
    std::unique_ptr<CryptAuthKey> new_group_key,
    const std::optional<cryptauthv2::EncryptedGroupPrivateKey>&
        encrypted_group_private_key,
    const std::optional<cryptauthv2::ClientDirective>& new_client_directive,
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(request_context_);
  DCHECK(local_device_metadata_);
  DCHECK(initial_group_key_);

  OnAttemptFinished(id_to_device_metadata_packet_map, std::move(new_group_key),
                    encrypted_group_private_key, new_client_directive,
                    device_sync_result_code);
}

void FakeCryptAuthMetadataSyncer::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
    const CryptAuthKey* initial_group_key) {
  request_context_ = request_context;
  local_device_metadata_ = local_device_metadata;
  initial_group_key_ = initial_group_key;
}

FakeCryptAuthMetadataSyncerFactory::FakeCryptAuthMetadataSyncerFactory() =
    default;

FakeCryptAuthMetadataSyncerFactory::~FakeCryptAuthMetadataSyncerFactory() =
    default;

std::unique_ptr<CryptAuthMetadataSyncer>
FakeCryptAuthMetadataSyncerFactory::CreateInstance(
    CryptAuthClientFactory* client_factory,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_client_factory_ = client_factory;
  last_pref_service_ = pref_service;

  auto instance = std::make_unique<FakeCryptAuthMetadataSyncer>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace ash
