// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/switches.h"

#include "base/feature_list.h"

namespace switches {

// TODO(crbug.com/40780588): remove the feature toggle once most of bookmarks
// have been reuploaded.
BASE_FEATURE(kSyncReuploadBookmarks, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncMigrateBookmarksWithoutClientTagHash,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled by default, intended as a kill switch.
BASE_FEATURE(kSyncBookmarksBatchUploadSelectedItems,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches
