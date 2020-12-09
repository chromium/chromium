// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

extern const base::Feature kSyncResetPollIntervalOnStart;
extern const base::Feature kSyncUseScryptForNewCustomPassphrases;
extern const base::Feature kSyncSupportTrustedVaultPassphrase;
extern const base::Feature kSyncTriggerFullKeystoreMigration;
extern const base::Feature kIgnoreSyncEncryptionKeysLongMissing;

}  // namespace switches

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_SWITCHES_H_
