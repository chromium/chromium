// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer.h"
#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer_impl.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"

namespace chromeos {
namespace device_sync {

class CryptAuthClientFactory;
class CryptAuthKey;

class FakeCryptAuthGroupPrivateKeySharer
    : public CryptAuthGroupPrivateKeySharer {
 public:
  FakeCryptAuthGroupPrivateKeySharer();
  ~FakeCryptAuthGroupPrivateKeySharer() override;

  // The RequestContext passed to ShareGroupPrivateKey(). Returns null if
  // ShareGroupPrivateKey() has not been called yet.
  const base::Optional<cryptauthv2::RequestContext>& request_context() const {
    return request_context_;
  }

  // The group key passed to ShareGroupPrivateKey(). Returns null if
  // ShareGroupPrivateKey() has not been called yet.
  const CryptAuthKey* group_key() const { return group_key_.get(); }

  // The device ID to encrypting key map passed to ShareGroupPrivateKey().
  // Returns null if ShareGroupPrivateKey() has not been called yet.
  const base::Optional<IdToEncryptingKeyMap>& id_to_encrypting_key_map() const {
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

  base::Optional<cryptauthv2::RequestContext> request_context_;
  std::unique_ptr<CryptAuthKey> group_key_;
  base::Optional<IdToEncryptingKeyMap> id_to_encrypting_key_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthGroupPrivateKeySharer);
};

class FakeCryptAuthGroupPrivateKeySharerFactory
    : public CryptAuthGroupPrivateKeySharerImpl::Factory {
 public:
  FakeCryptAuthGroupPrivateKeySharerFactory();
  ~FakeCryptAuthGroupPrivateKeySharerFactory() override;

  // Returns a vector of all FakeCryptAuthGroupPrivateKeySharer instances
  // created by BuildInstance().
  const std::vector<FakeCryptAuthGroupPrivateKeySharer*>& instances() const {
    return instances_;
  }

  // Returns the most recent CryptAuthClientFactory input into BuildInstance().
  const CryptAuthClientFactory* last_client_factory() const {
    return last_client_factory_;
  }

 private:
  // CryptAuthGroupPrivateKeySharerImpl::Factory:
  std::unique_ptr<CryptAuthGroupPrivateKeySharer> BuildInstance(
      CryptAuthClientFactory* client_factory,
      std::unique_ptr<base::OneShotTimer> timer = nullptr) override;

  std::vector<FakeCryptAuthGroupPrivateKeySharer*> instances_;
  CryptAuthClientFactory* last_client_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthGroupPrivateKeySharerFactory);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
