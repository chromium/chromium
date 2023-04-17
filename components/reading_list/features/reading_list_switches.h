// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
#define COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace reading_list {
namespace switches {

// Feature flag used for enabling the reading list backend migration.
// When enabled, reading list data will also be stored in the Bookmarks backend.
// This allows each platform to migrate their reading list front end to point at
// the new reading list data stored in the bookmarks backend without
// interruption to cross device sync if some syncing devices are on versions
// with the migration behavior while others aren't. See crbug/1234426 for more
// details.
BASE_DECLARE_FEATURE(kReadLaterBackendMigration);

#if BUILDFLAG(IS_ANDROID)
// Feature flag used for enabling read later reminder notification.
BASE_DECLARE_FEATURE(kReadLaterReminderNotification);
#endif

// Feature flag that controls a technical rollout of a new codepath that doesn't
// itself cause user-facing changes but sets the foundation for later rollouts
// namely, `kReadingListEnableSyncTransportModeUponSignIn` below).
BASE_DECLARE_FEATURE(kReadingListEnableDualReadingListModel);

// Feature flag used for enabling sync (transport mode) for signed-in users that
// haven't turned on full sync.
BASE_DECLARE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn);

// Returns whether reading list storage related UI can be enabled, by testing
// `kReadingListEnableSyncTransportModeUponSignIn` and
// `kReadingListEnableDualReadingListModel`.
bool IsReadingListAccountStorageUIEnabled();

}  // namespace switches
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
