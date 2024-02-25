// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
#define COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace reading_list::switches {

// Feature flag used for enabling the reading list backend migration.
// When enabled, reading list data will also be stored in the Bookmarks backend.
// This allows each platform to migrate their reading list front end to point at
// the new reading list data stored in the bookmarks backend without
// interruption to cross device sync if some syncing devices are on versions
// with the migration behavior while others aren't. See crbug/1234426 for more
// details.
BASE_DECLARE_FEATURE(kReadLaterBackendMigration);

}  // namespace reading_list::switches

#endif  // COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
