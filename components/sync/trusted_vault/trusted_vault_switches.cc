// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_switches.h"

#include "components/sync/driver/sync_driver_switches.h"

namespace switches {

// Allows device registration within trusted vault server without having trusted
// vault key. Effectively disabled if kSyncTrustedVaultPassphraseRecovery
// is disabled.
const base::Feature kAllowSilentTrustedVaultDeviceRegistration{
    "AllowSilentTrustedVaultDeviceRegistration",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Specifies how long requests to vault service shouldn't be retried after
// encountering transient error.
const base::FeatureParam<base::TimeDelta>
    kTrustedVaultServiceThrottlingDuration{
        &kSyncTrustedVaultPassphraseRecovery,
        "TrustedVaultServiceThrottlingDuration", base::Days(1)};

}  // namespace switches
