// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_syncer.h"

#include <utility>

namespace chromeos {

namespace device_sync {

CryptAuthDeviceSyncer::CryptAuthDeviceSyncer() = default;

CryptAuthDeviceSyncer::~CryptAuthDeviceSyncer() = default;

void CryptAuthDeviceSyncer::Sync(
    const cryptauthv2::ClientMetadata& client_metadata,
    const cryptauthv2::ClientAppMetadata& client_app_metadata,
    DeviceSyncAttemptFinishedCallback callback) {
  // Enforce that Sync() can only be called once.
  DCHECK(!was_sync_called_);
  was_sync_called_ = true;

  callback_ = std::move(callback);

  OnAttemptStarted(client_metadata, client_app_metadata);
}

void CryptAuthDeviceSyncer::OnAttemptFinished(
    const CryptAuthDeviceSyncResult& device_sync_result) {
  DCHECK(callback_);
  std::move(callback_).Run(device_sync_result);
}

}  // namespace device_sync

}  // namespace chromeos
