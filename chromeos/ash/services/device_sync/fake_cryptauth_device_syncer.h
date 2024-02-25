// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

class PrefService;

namespace ash {

namespace device_sync {

class CryptAuthDeviceSyncResult;

// Implementation of CryptAuthDeviceSyncer for use in tests.
class FakeCryptAuthDeviceSyncer : public CryptAuthDeviceSyncer {
 public:
  FakeCryptAuthDeviceSyncer();

  FakeCryptAuthDeviceSyncer(const FakeCryptAuthDeviceSyncer&) = delete;
  FakeCryptAuthDeviceSyncer& operator=(const FakeCryptAuthDeviceSyncer&) =
      delete;

  ~FakeCryptAuthDeviceSyncer() override;

  const std::optional<cryptauthv2::ClientMetadata>& client_metadata() const {
    return client_metadata_;
  }

  const std::optional<cryptauthv2::ClientAppMetadata>& client_app_metadata()
      const {
    return client_app_metadata_;
  }

  void FinishAttempt(const CryptAuthDeviceSyncResult& device_sync_result);

 private:
  // CryptAuthDeviceSyncer:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata) override;

  std::optional<cryptauthv2::ClientMetadata> client_metadata_;
  std::optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;
};

class FakeCryptAuthDeviceSyncerFactory
    : public CryptAuthDeviceSyncerImpl::Factory {
 public:
  FakeCryptAuthDeviceSyncerFactory();

  FakeCryptAuthDeviceSyncerFactory(const FakeCryptAuthDeviceSyncerFactory&) =
      delete;
  FakeCryptAuthDeviceSyncerFactory& operator=(
      const FakeCryptAuthDeviceSyncerFactory&) = delete;

  ~FakeCryptAuthDeviceSyncerFactory() override;

  const std::vector<raw_ptr<FakeCryptAuthDeviceSyncer, VectorExperimental>>&
  instances() const {
    return instances_;
  }

  const CryptAuthDeviceRegistry* last_device_registry() const {
    return last_device_registry_;
  }

  const CryptAuthKeyRegistry* last_key_registry() const {
    return last_key_registry_;
  }

  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

  const PrefService* last_pref_service() const { return last_pref_service_; }

 private:
  // CryptAuthDeviceSyncerImpl::Factory:
  std::unique_ptr<CryptAuthDeviceSyncer> CreateInstance(
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      SyncedBluetoothAddressTracker* synced_bluetooth_address_tracker,
      AttestationCertificatesSyncer* attestation_certificates_syncer,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<raw_ptr<FakeCryptAuthDeviceSyncer, VectorExperimental>>
      instances_;
  raw_ptr<CryptAuthDeviceRegistry> last_device_registry_ = nullptr;
  raw_ptr<CryptAuthKeyRegistry> last_key_registry_ = nullptr;
  raw_ptr<CryptAuthClientFactory> last_client_factory_ = nullptr;
  raw_ptr<SyncedBluetoothAddressTracker, DanglingUntriaged>
      last_synced_bluetooth_address_tracker_ = nullptr;
  raw_ptr<PrefService> last_pref_service_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
