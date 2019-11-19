// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace chromeos {

namespace device_sync {

class CryptAuthClientFactory;
class CryptAuthKey;

class FakeCryptAuthMetadataSyncer : public CryptAuthMetadataSyncer {
 public:
  FakeCryptAuthMetadataSyncer();
  ~FakeCryptAuthMetadataSyncer() override;

  // The RequestContext passed to SyncMetadata(). Returns null if
  // SyncMetadata() has not been called yet.
  const base::Optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The local device's BetterTogetherDeviceMetadata passed to SyncMetadata().
  // Returns null if SyncMetadata() has not been called yet.
  const base::Optional<cryptauthv2::BetterTogetherDeviceMetadata>&
  local_device_metadata() const {
    return local_device_metadata_;
  }

  // The initial group key passed to SyncMetadata(). Returns null if
  // SyncMetadata() has not been called yet.
  const base::Optional<const CryptAuthKey*>& initial_group_key() const {
    return initial_group_key_;
  }

  void FinishAttempt(
      const IdToDeviceMetadataPacketMap& id_to_device_metadata_packet_map,
      std::unique_ptr<CryptAuthKey> new_group_key,
      const base::Optional<cryptauthv2::EncryptedGroupPrivateKey>&
          encrypted_group_private_key,
      const base::Optional<cryptauthv2::ClientDirective>& new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  // CryptAuthMetadataSyncer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key) override;

  base::Optional<cryptauthv2::RequestContext> request_context_;
  base::Optional<cryptauthv2::BetterTogetherDeviceMetadata>
      local_device_metadata_;
  base::Optional<const CryptAuthKey*> initial_group_key_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthMetadataSyncer);
};

class FakeCryptAuthMetadataSyncerFactory
    : public CryptAuthMetadataSyncerImpl::Factory {
 public:
  FakeCryptAuthMetadataSyncerFactory();
  ~FakeCryptAuthMetadataSyncerFactory() override;

  // Returns a vector of all FakeCryptAuthMetadataSyncer instances created
  // by BuildInstance().
  const std::vector<FakeCryptAuthMetadataSyncer*>& instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into BuildInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthMetadataSyncerImpl::Factory:
  std::unique_ptr<CryptAuthMetadataSyncer> BuildInstance(
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthMetadataSyncer*> instances_;
  CryptAuthClientFactory* last_client_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthMetadataSyncerFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_
