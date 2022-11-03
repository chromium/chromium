// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_

#include <memory>
#include <vector>

#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer_impl.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

namespace device_sync {

class CryptAuthClientFactory;
class CryptAuthKey;

class FakeCryptAuthMetadataSyncer : public CryptAuthMetadataSyncer {
 public:
  FakeCryptAuthMetadataSyncer();

  FakeCryptAuthMetadataSyncer(const FakeCryptAuthMetadataSyncer&) = delete;
  FakeCryptAuthMetadataSyncer& operator=(const FakeCryptAuthMetadataSyncer&) =
      delete;

  ~FakeCryptAuthMetadataSyncer() override;

  // The RequestContext passed to SyncMetadata(). Returns null if
  // SyncMetadata() has not been called yet.
  const absl::optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The local device's BetterTogetherDeviceMetadata passed to SyncMetadata().
  // Returns null if SyncMetadata() has not been called yet.
  const absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>&
  local_device_metadata() const {
    return local_device_metadata_;
  }

  // The initial group key passed to SyncMetadata(). Returns null if
  // SyncMetadata() has not been called yet.
  const absl::optional<const CryptAuthKey*>& initial_group_key() const {
    return initial_group_key_;
  }

  void FinishAttempt(
      const IdToDeviceMetadataPacketMap& id_to_device_metadata_packet_map,
      std::unique_ptr<CryptAuthKey> new_group_key,
      const absl::optional<cryptauthv2::EncryptedGroupPrivateKey>&
          encrypted_group_private_key,
      const absl::optional<cryptauthv2::ClientDirective>& new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  // CryptAuthMetadataSyncer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key) override;

  absl::optional<cryptauthv2::RequestContext> request_context_;
  absl::optional<cryptauthv2::BetterTogetherDeviceMetadata>
      local_device_metadata_;
  absl::optional<const CryptAuthKey*> initial_group_key_;
};

class FakeCryptAuthMetadataSyncerFactory
    : public CryptAuthMetadataSyncerImpl::Factory {
 public:
  FakeCryptAuthMetadataSyncerFactory();

  FakeCryptAuthMetadataSyncerFactory(
      const FakeCryptAuthMetadataSyncerFactory&) = delete;
  FakeCryptAuthMetadataSyncerFactory& operator=(
      const FakeCryptAuthMetadataSyncerFactory&) = delete;

  ~FakeCryptAuthMetadataSyncerFactory() override;

  // Returns a vector of all FakeCryptAuthMetadataSyncer instances created
  // by CreateInstance().
  const std::vector<FakeCryptAuthMetadataSyncer*>& instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into CreateInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

  // Returns the most recent PrefService input into CreateInstance().
  const PrefService* last_pref_service() const { return last_pref_service_; }

 private:
  // CryptAuthMetadataSyncerImpl::Factory:
  std::unique_ptr<CryptAuthMetadataSyncer> CreateInstance(
      CryptAuthClientFactory* client_factory,
      PrefService* pref_service,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<FakeCryptAuthMetadataSyncer*> instances_;
  CryptAuthClientFactory* last_client_factory_ = nullptr;
  PrefService* last_pref_service_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_METADATA_SYNCER_H_
