// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_pb {
class BookmarkSpecifics;
class EntitySpecifics;
}  // namespace sync_pb

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    bool force_favicon_load);

// Creates a bookmark node under the given parent node from the given specifics.
// Returns the newly created node. Callers must verify that
// |specifics| passes the IsValidBookmarkSpecifics().
const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    int index,
    bool is_folder,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service);

// Updates the bookmark node |node| with the data in |specifics|. Callers must
// verify that |specifics| passes the IsValidBookmarkSpecifics().
void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service);

// Checks if a bookmark specifics represents a valid bookmark. |is_folder| is
// whether this specifics is for a folder. Valid specifics must not be empty,
// non-folders must contains a valid url, and all keys in the meta_info must be
// unique.
bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics,
                              bool is_folder);

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SPECIFICS_CONVERSIONS_H_
