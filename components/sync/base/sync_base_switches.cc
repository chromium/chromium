// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_base_switches.h"

#include "base/metrics/field_trial_params.h"

namespace switches {

// Overrides the default server used for profile sync.
const char kSyncServiceURL[] = "sync-url";

// Specifies the vault server used for trusted vault passphrase.
const char kTrustedVaultServiceURL[] = "trusted-vault-service-url";

const base::Feature kSyncNigoriRemoveMetadataOnCacheGuidMismatch{
    "SyncNigoriRemoveMetadataOnCacheGuidMismatch",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Force disables scrypt key derivation for custom passphrase. If this feature
// is enabled, scrypt will be considered as an unsupported method, and Chrome
// will not be able to access data encrypted using scrypt-derived keys (valid
// passphrases will be rejected).
const base::Feature kSyncForceDisableScryptForCustomPassphrase{
    "SyncForceDisableScryptForCustomPassphrase",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncE2ELatencyMeasurement = {
    "SyncE2ELatencyMeasurement", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
