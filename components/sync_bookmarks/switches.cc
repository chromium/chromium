// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/switches.h"

namespace switches {

const base::Feature kSyncDoNotCommitBookmarksWithoutFavicon = {
    "SyncDoNotCommitBookmarksWithoutFavicon", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncReuploadBookmarkFullTitles{
    "SyncReuploadBookmarkFullTitles", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncDeduplicateAllBookmarksWithSameGUID{
    "SyncDeduplicateAllBookmarksWithSameGUID",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncIgnoreChangesInTouchIcons{
    "SyncIgnoreChangesInTouchIcons", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches
