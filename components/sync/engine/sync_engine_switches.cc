// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_engine_switches.h"

namespace switches {

const base::Feature kSyncResetPollIntervalOnStart{
    "SyncResetPollIntervalOnStart", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether encryption keys should be derived using scrypt when a new custom
// passphrase is set. If disabled, the old PBKDF2 key derivation method will be
// used instead. Note that disabling this feature does not disable deriving keys
// via scrypt when we receive a remote Nigori node that specifies it as the key
// derivation method.
const base::Feature kSyncUseScryptForNewCustomPassphrases{
    "SyncUseScryptForNewCustomPassphrases", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncSupportTrustedVaultPassphrase{
    "SyncSupportTrustedVaultPassphrase", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncTriggerFullKeystoreMigration{
    "SyncTriggerFullKeystoreMigration", base::FEATURE_DISABLED_BY_DEFAULT};

// Causes Sync to ignore updates encrypted with keys that have been missing for
// too long from this client; Sync will proceed normally as if those updates
// didn't exist.
const base::Feature kIgnoreSyncEncryptionKeysLongMissing{
    "IgnoreSyncEncryptionKeysLongMissing", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
