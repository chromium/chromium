// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/switches.h"
#include "base/feature_list.h"

namespace switches {

const base::Feature kSyncReuploadBookmarkFullTitles{
    "SyncReuploadBookmarkFullTitles", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSyncUseClientTagForBookmarkCommits{
    "SyncUseClientTagForBookmarkCommits", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncReuploadBookmarksUponMatchingData{
    "SyncReuploadBookmarksUponMatchingData", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches
