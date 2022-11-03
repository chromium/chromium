// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/fake_attestation_certificates_syncer.h"

namespace ash {
namespace device_sync {

FakeAttestationCertificatesSyncer::FakeAttestationCertificatesSyncer() =
    default;

FakeAttestationCertificatesSyncer::~FakeAttestationCertificatesSyncer() =
    default;

void FakeAttestationCertificatesSyncer::SetLastSyncTimestamp() {
  number_of_set_last_sync_timestamp_calls_++;
}

void FakeAttestationCertificatesSyncer::UpdateCerts(
    NotifyCallback callback,
    const std::string& user_key) {
  std::move(callback).Run(std::vector<std::string>{kFakeCert}, /*valid=*/true);
}

bool FakeAttestationCertificatesSyncer::IsUpdateRequired() {
  return is_update_required_;
}

void FakeAttestationCertificatesSyncer::SetIsUpdateRequired(
    bool is_update_required) {
  is_update_required_ = is_update_required;
}

void FakeAttestationCertificatesSyncer::ScheduleSyncForTest() {
  // Not supported.
}

FakeAttestationCertificatesSyncerFactory::
    FakeAttestationCertificatesSyncerFactory() = default;

FakeAttestationCertificatesSyncerFactory::
    ~FakeAttestationCertificatesSyncerFactory() = default;

std::unique_ptr<AttestationCertificatesSyncer>
FakeAttestationCertificatesSyncerFactory::CreateInstance(
    CryptAuthScheduler* cryptauth_scheduler,
    PrefService* pref_service,
    AttestationCertificatesSyncer::GetAttestationCertificatesFunction
        get_attestation_certificates_function) {
  auto instance = std::make_unique<FakeAttestationCertificatesSyncer>();
  last_created_ = instance.get();
  return instance;
}

}  // namespace device_sync
}  // namespace ash
