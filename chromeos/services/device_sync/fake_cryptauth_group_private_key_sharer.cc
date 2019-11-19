// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/fake_cryptauth_group_private_key_sharer.h"

#include "chromeos/services/device_sync/cryptauth_key.h"

namespace chromeos {

namespace device_sync {

FakeCryptAuthGroupPrivateKeySharer::FakeCryptAuthGroupPrivateKeySharer() =
    default;

FakeCryptAuthGroupPrivateKeySharer::~FakeCryptAuthGroupPrivateKeySharer() =
    default;

void FakeCryptAuthGroupPrivateKeySharer::FinishAttempt(
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(request_context_);
  DCHECK(group_key_);
  DCHECK(id_to_encrypting_key_map_);

  OnAttemptFinished(device_sync_result_code);
}

void FakeCryptAuthGroupPrivateKeySharer::OnAttemptStarted(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const IdToEncryptingKeyMap& id_to_encrypting_key_map) {
  request_context_ = request_context;
  group_key_ = std::make_unique<CryptAuthKey>(group_key);
  id_to_encrypting_key_map_ = id_to_encrypting_key_map;
}

FakeCryptAuthGroupPrivateKeySharerFactory::
    FakeCryptAuthGroupPrivateKeySharerFactory() = default;

FakeCryptAuthGroupPrivateKeySharerFactory::
    ~FakeCryptAuthGroupPrivateKeySharerFactory() = default;

std::unique_ptr<CryptAuthGroupPrivateKeySharer>
FakeCryptAuthGroupPrivateKeySharerFactory::BuildInstance(
    CryptAuthClientFactory* client_factory,
    std::unique_ptr<base::OneShotTimer> timer) {
  last_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeCryptAuthGroupPrivateKeySharer>();
  instances_.push_back(instance.get());

  return instance;
}

}  // namespace device_sync

}  // namespace chromeos
