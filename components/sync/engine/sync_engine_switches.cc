// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_engine_switches.h"

namespace switches {

// TODO(crbug.com/657130): Sync integration tests depend on the precommit get
// updates because invalidations aren't working for them. Therefore, they pass
// the command line switch to enable this feature. Once sync integrations test
// support invalidation, this should be removed.
// Enables feature to perform GetUpdate requests before every commit.
const char kSyncEnableGetUpdatesBeforeCommit[] =
    "sync-enable-get-update-before-commits";

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
    "SyncSupportTrustedVaultPassphrase", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
