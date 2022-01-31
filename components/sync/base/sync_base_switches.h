// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_
#define COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// Overrides the default server used for profile sync.
constexpr inline char kSyncServiceURL[] = "sync-url";

// Specifies the vault server used for trusted vault passphrase.
constexpr inline char kTrustedVaultServiceURL[] = "trusted-vault-service-url";

constexpr inline base::Feature kSyncNigoriRemoveMetadataOnCacheGuidMismatch{
    "SyncNigoriRemoveMetadataOnCacheGuidMismatch",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches

#endif  // COMPONENTS_SYNC_BASE_SYNC_BASE_SWITCHES_H_
