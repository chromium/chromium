// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_

#include "base/feature_list.h"

namespace base {
class FilePath;
}  // namespace base

class PrefService;

namespace browser_sync {

// If enabled, eligible users (i.e. those for which Sync-the-feature is active)
// are migrated, at browser startup, to the signed-in non-syncing state.
BASE_DECLARE_FEATURE(kMigrateSyncingUserToSignedIn);

// If enabled, users who were migrated from syncing to signed-in via the above
// flag are migrated back into the syncing state.
BASE_DECLARE_FEATURE(kUndoMigrationOfSyncingUserToSignedIn);

void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service);

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
