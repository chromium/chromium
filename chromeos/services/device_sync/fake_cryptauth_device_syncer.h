// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_device_syncer.h"
#include "chromeos/services/device_sync/cryptauth_device_syncer_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_client_app_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

class CryptAuthDeviceSyncResult;

// Implementation of CryptAuthDeviceSyncer for use in tests.
class FakeCryptAuthDeviceSyncer : public CryptAuthDeviceSyncer {
 public:
  FakeCryptAuthDeviceSyncer();
  ~FakeCryptAuthDeviceSyncer() override;

  const base::Optional<cryptauthv2::ClientMetadata>& client_metadata() const {
    return client_metadata_;
  }

  const base::Optional<cryptauthv2::ClientAppMetadata>& client_app_metadata()
      const {
    return client_app_metadata_;
  }

  void FinishAttempt(const CryptAuthDeviceSyncResult& device_sync_result);

 private:
  // CryptAuthDeviceSyncer:
  void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata) override;

  base::Optional<cryptauthv2::ClientMetadata> client_metadata_;
  base::Optional<cryptauthv2::ClientAppMetadata> client_app_metadata_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthDeviceSyncer);
};

class FakeCryptAuthDeviceSyncerFactory
    : public CryptAuthDeviceSyncerImpl::Factory {
 public:
  FakeCryptAuthDeviceSyncerFactory();
  ~FakeCryptAuthDeviceSyncerFactory() override;

  const std::vector<FakeCryptAuthDeviceSyncer*>& instances() const {
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

 private:
  // CryptAuthDeviceSyncerImpl::Factory:
  std::unique_ptr<CryptAuthDeviceSyncer> BuildInstance(
      CryptAuthDeviceRegistry* device_registry,
      CryptAuthKeyRegistry* key_registry,
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthDeviceSyncer*> instances_;
  CryptAuthDeviceRegistry* last_device_registry_ = nullptr;
  CryptAuthKeyRegistry* last_key_registry_ = nullptr;
  CryptAuthClientFactory* last_client_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthDeviceSyncerFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_DEVICE_SYNCER_H_
