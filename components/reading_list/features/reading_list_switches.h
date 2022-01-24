// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
#define COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

#if !defined(OS_IOS)
// Feature flag used for enabling side panel on desktop.
// TODO(crbug.com/1225279): Move this back to chrome/browser/ui/ui_features.h
// after kReadLater is cleaned up (and IsReadingListEnabled() returns true on
// Desktop). This is currently here so that kSidePanel (which doesn't work
// without read later) can imply that the reading list should be enabled.
namespace features {
extern const base::Feature kSidePanel;
}  // namespace features
#endif  // !defined(OS_IOS)

namespace reading_list {
namespace switches {

// Feature flag used for enabling Read later on desktop and Android.
extern const base::Feature kReadLater;

// Whether Reading List is enabled on this device. On iOS this is true if the
// buildflag for Reading List is enabled (no experiment). On Desktop it is also
// true if `kSidePanel` is enabled as it assumes a reading list.
bool IsReadingListEnabled();

// Feature flag used for enabling the reading list backend migration.
// When enabled, reading list data will also be stored in the Bookmarks backend.
// This allows each platform to migrate their reading list front end to point at
// the new reading list data stored in the bookmarks backend without
// interruption to cross device sync if some syncing devices are on versions
// with the migration behavior while others aren't. See crbug/1234426 for more
// details.
extern const base::Feature kReadLaterBackendMigration;

#ifdef OS_ANDROID
// Feature flag used for enabling read later reminder notification.
extern const base::Feature kReadLaterReminderNotification;
#endif

}  // namespace switches
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
