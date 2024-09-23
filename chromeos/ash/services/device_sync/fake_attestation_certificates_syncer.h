// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_ATTESTATION_CERTIFICATES_SYNCER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_ATTESTATION_CERTIFICATES_SYNCER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer.h"
#include "chromeos/ash/services/device_sync/attestation_certificates_syncer_impl.h"

namespace ash {

namespace device_sync {

class FakeAttestationCertificatesSyncer : public AttestationCertificatesSyncer {
 public:
  FakeAttestationCertificatesSyncer();
  ~FakeAttestationCertificatesSyncer() override;

  static constexpr char kFakeCert[] = "certificate";

  void SetIsUpdateRequired(bool is_update_required);

  int number_of_set_last_sync_timestamp_calls() {
    return number_of_set_last_sync_timestamp_calls_;
  }

 private:
  // AttestationCertificatesSyncer:
  bool IsUpdateRequired() override;
  void SetLastSyncTimestamp() override;
  void UpdateCerts(NotifyCallback callback,
                   const std::string& user_key) override;
  void ScheduleSyncForTest() override;

  int number_of_set_last_sync_timestamp_calls_ = 0;
  bool is_update_required_ = false;
};

class FakeAttestationCertificatesSyncerFactory
    : public AttestationCertificatesSyncerImpl::Factory {
 public:
  FakeAttestationCertificatesSyncerFactory();
  ~FakeAttestationCertificatesSyncerFactory() override;

  AttestationCertificatesSyncer* last_created() { return last_created_; }

 private:
  // AttestationCertificatesSyncer::Factory:
  std::unique_ptr<AttestationCertificatesSyncer> CreateInstance(
      CryptAuthScheduler* cryptauth_scheduler,
      PrefService* pref_service,
      AttestationCertificatesSyncer::GetAttestationCertificatesFunction
          get_attestation_certificates_function) override;

  raw_ptr<AttestationCertificatesSyncer> last_created_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_ATTESTATION_CERTIFICATES_SYNCER_H_
