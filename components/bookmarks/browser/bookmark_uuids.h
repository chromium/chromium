// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UUIDS_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UUIDS_H_

namespace bookmarks {

// TODO(crbug.com/40108138): Make these constants of type base::Uuid once there
// exists a constexpr constructor.
extern const char kRootNodeUuid[];
extern const char kBookmarkBarNodeUuid[];
extern const char kOtherBookmarksNodeUuid[];
extern const char kMobileBookmarksNodeUuid[];
extern const char kManagedNodeUuid[];
extern const char kShoppingCollectionUuid[];

// A bug in sync caused some problematic UUIDs to be produced.
extern const char kBannedUuidDueToPastSyncBug[];

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UUIDS_H_
