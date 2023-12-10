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

BASE_DECLARE_FEATURE(kMigrateSyncingUserToSignedIn);

void MaybeMigrateSyncingUserToSignedIn(const base::FilePath& profile_path,
                                       PrefService* pref_service);

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_TO_SIGNIN_MIGRATION_H_
