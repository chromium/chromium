// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_group_private_key_sharer.h"

#include <utility>

#include "chromeos/services/device_sync/cryptauth_key.h"

namespace chromeos {

namespace device_sync {

CryptAuthGroupPrivateKeySharer::CryptAuthGroupPrivateKeySharer() = default;

CryptAuthGroupPrivateKeySharer::~CryptAuthGroupPrivateKeySharer() = default;

void CryptAuthGroupPrivateKeySharer::ShareGroupPrivateKey(
    const cryptauthv2::RequestContext& request_context,
    const CryptAuthKey& group_key,
    const IdToEncryptingKeyMap& id_to_encrypting_key_map,
    ShareGroupPrivateKeyAttemptFinishedCallback callback) {
  // Enforce that ShareGroupPrivateKey() can only be called once.
  DCHECK(!was_share_group_private_key_called_);
  was_share_group_private_key_called_ = true;

  DCHECK(!group_key.private_key().empty());
  DCHECK(!id_to_encrypting_key_map.empty());

  callback_ = std::move(callback);

  OnAttemptStarted(request_context, group_key, id_to_encrypting_key_map);
}

void CryptAuthGroupPrivateKeySharer::OnAttemptFinished(
    CryptAuthDeviceSyncResult::ResultCode device_sync_result_code) {
  DCHECK(callback_);
  std::move(callback_).Run(device_sync_result_code);
}

}  // namespace device_sync

}  // namespace chromeos
