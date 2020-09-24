// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

extern const base::Feature kSyncDoNotCommitBookmarksWithoutFavicon;
// TODO(crbug.com/1066962): remove this code when most of bookmarks are
// reuploaded.
extern const base::Feature kSyncReuploadBookmarkFullTitles;
// This switch is used to disable removing of bookmark duplicates by GUID.
extern const base::Feature kSyncDeduplicateAllBookmarksWithSameGUID;
// TODO(crbug.com/1075709): remove after launch.
extern const base::Feature kSyncIgnoreChangesInTouchIcons;

}  // namespace switches

#endif  // COMPONENTS_SYNC_BOOKMARKS_SWITCHES_H_
