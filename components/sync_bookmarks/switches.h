// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// TODO(crbug.com/1066962): remove this code when most of bookmarks are
// reuploaded.
extern const base::Feature kSyncReuploadBookmarkFullTitles;
extern const base::Feature kSyncUseClientTagForBookmarkCommits;

// TODO(crbug.com/1177798): remove this code when most of bookmarks are
// reuploaded. This feature toggle will work only when
// SyncReuploadBookmarkFullTitles is enabled.
extern const base::Feature kSyncReuploadBookmarksUponMatchingData;

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
