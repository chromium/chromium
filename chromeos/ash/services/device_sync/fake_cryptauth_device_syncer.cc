// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_cryptauth_device_syncer.h"

#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"

namespace ash {

namespace device_sync {

FakeCryptAuthDeviceSyncer::FakeCryptAuthDeviceSyncer() = default;

FakeCryptAuthDeviceSyncer::~FakeCryptAuthDeviceSyncer() = default;

void FakeCryptAuthDeviceSyncer::FinishAttempt(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  DCHECK(client_metadata_);
  DCHECK(client_app_metadata_);
  OnAttemptFinished(device_sync_result);
}

void FakeCryptAuthDeviceSyncer::OnAttemptStarted(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata) {
  client_metadata_ = client_metadata;
  client_app_metadata_ = client_app_metadata;
}

FakeCryptAuthDeviceSyncerFactory::FakeCryptAuthDeviceSyncerFactory() = default;

FakeCryptAuthDeviceSyncerFactory::~FakeCryptAuthDeviceSyncerFactory() = default;

std::unique_ptr<CryptAuthDeviceSyncer>
FakeCryptAuthDeviceSyncerFactory::CreateInstance(
    CryptAuthDeviceRegistry* device_registry,
    CryptAuthKeyRegistry* key_registry,
    CryptAuthClientFactory* client_factory,
    SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
    AttestationCertificatesSyncer* attestation_certificates_syncer,
    PrefService* pref_service,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_device_registry_ = device_registry;
  last_key_registry_ = key_registry;
  last_client_factory_ = client_factory;
  last_synced_bluetooth_address_tracker_ = synced_bluetooth_address_tracker;
  last_pref_service_ = pref_service;

  auto instance = std::make_unique<FakeCryptAuthDeviceSyncer>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace ash
