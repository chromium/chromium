// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/features.h"

#include "base/feature_list.h"

namespace trusted_vault {

BASE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
             "SyncTrustedVaultDegradedRecoverabilityHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration,
             "SyncTrustedVaultVerifyDeviceRegistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace trusted_vault
