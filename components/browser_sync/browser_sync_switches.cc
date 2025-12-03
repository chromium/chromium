// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser_sync_switches.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace switches {

BASE_FEATURE(kMigrateSyncingUserToSignedIn,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE_PARAM(base::TimeDelta,
                   kMinDelayToMigrateSyncPaused,
                   &switches::kMigrateSyncingUserToSignedIn,
                   "min_delay_to_migrate_sync_paused",
                   base::Days(7));

BASE_FEATURE(kUndoMigrationOfSyncingUserToSignedIn,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceMigrateSyncingUserToSignedIn,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kMigrateOutOfSyncSetupIncompleteState,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace switches
