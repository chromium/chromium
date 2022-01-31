// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// TODO(crbug.com/1232951): remove the feature toggle once most of bookmarks
// have been reuploaded.
constexpr inline base::Feature kSyncReuploadBookmarks{
    "SyncReuploadBookmarks", base::FEATURE_ENABLED_BY_DEFAULT};

constexpr inline base::Feature kSyncUseClientTagForBookmarkCommits{
    "SyncUseClientTagForBookmarkCommits", base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1177798): remove this code after a quick verification that it
// doesn't cause issues.
constexpr inline base::Feature kSyncBookmarksEnforceLateMaxEntriesToCommit{
    "SyncBookmarksEnforceLateMaxEntriesToCommit",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
