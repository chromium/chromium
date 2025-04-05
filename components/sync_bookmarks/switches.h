// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// TODO(crbug.com/40780588): remove the feature toggle once most of bookmarks
// have been reuploaded.
BASE_DECLARE_FEATURE(kSyncReuploadBookmarks);

BASE_DECLARE_FEATURE(kSyncMigrateBookmarksWithoutClientTagHash);

// If enabled, support displaying and uploading individual items (bookmarks or
// folders) in the Batch Upload UI.
//
// Batch Upload of all items is supported regardless of this feature flag.
//
// On Windows/Mac/Linux: this flag only affects behavior if the
// `switches::kSyncEnableBookmarksInTransportMode` feature is also enabled.
//
// On Android: this flag does not affect user-visiable behavior, but does enable
// new code paths.
BASE_DECLARE_FEATURE(kSyncBookmarksBatchUploadSelectedItems);

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
