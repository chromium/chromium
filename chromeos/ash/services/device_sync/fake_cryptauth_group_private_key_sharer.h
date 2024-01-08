// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer.h"
#include "chromeos/ash/services/device_sync/cryptauth_group_private_key_sharer_impl.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace ash {
namespace device_sync {

class CryptAuthClientFactory;
class CryptAuthKey;

class FakeCryptAuthGroupPrivateKeySharer
    : public CryptAuthGroupPrivateKeySharer {
 public:
  FakeCryptAuthGroupPrivateKeySharer();

  FakeCryptAuthGroupPrivateKeySharer(
      const FakeCryptAuthGroupPrivateKeySharer&) = delete;
  FakeCryptAuthGroupPrivateKeySharer& operator=(
      const FakeCryptAuthGroupPrivateKeySharer&) = delete;

  ~FakeCryptAuthGroupPrivateKeySharer() override;

  // The RequestContext passed to ShareGroupPrivateKey(). Returns null if
  // ShareGroupPrivateKey() has not been called yet.
  const std::optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The group key passed to ShareGroupPrivateKey(). Returns null if
  // ShareGroupPrivateKey() has not been called yet.
  const CryptAuthKey* group_key() const { return group_key_.get(); }

  // The device ID to encrypting key map passed to ShareGroupPrivateKey().
  // Returns null if ShareGroupPrivateKey() has not been called yet.
  const std::optional<IdToEncryptingKeyMap>& id_to_encrypting_key_map() const {
    return id_to_encrypting_key_map_;
  }

  void FinishAttempt(
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  // CryptAuthGroupPrivateKeySharer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const CryptAuthKey& group_key,
      const IdToEncryptingKeyMap& id_to_encrypting_key_map) override;

  std::optional<cryptauthv2::RequestContext> request_context_;
  std::unique_ptr<CryptAuthKey> group_key_;
  std::optional<IdToEncryptingKeyMap> id_to_encrypting_key_map_;
};

class FakeCryptAuthGroupPrivateKeySharerFactory
    : public CryptAuthGroupPrivateKeySharerImpl::Factory {
 public:
  FakeCryptAuthGroupPrivateKeySharerFactory();

  FakeCryptAuthGroupPrivateKeySharerFactory(
      const FakeCryptAuthGroupPrivateKeySharerFactory&) = delete;
  FakeCryptAuthGroupPrivateKeySharerFactory& operator=(
      const FakeCryptAuthGroupPrivateKeySharerFactory&) = delete;

  ~FakeCryptAuthGroupPrivateKeySharerFactory() override;

  // Returns a vector of all FakeCryptAuthGroupPrivateKeySharer instances
  // created by CreateInstance().
  const std::vector<
      raw_ptr<FakeCryptAuthGroupPrivateKeySharer, VectorExperimental>>&
  instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into CreateInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthGroupPrivateKeySharerImpl::Factory:
  std::unique_ptr<CryptAuthGroupPrivateKeySharer> CreateInstance(
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer) override;

  std::vector<raw_ptr<FakeCryptAuthGroupPrivateKeySharer, VectorExperimental>>
      instances_;
  raw_ptr<CryptAuthClientFactory> last_client_factory_ = nullptr;
};

}  // namespace device_sync

}  // namespace ash

#endif  //  CHROMEOS_ASH_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
