// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_DATA_PROVIDER_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_DATA_PROVIDER_H_

namespace bookmarks {
class BookmarkNode;
}

namespace power_bookmarks {

class PowerBookmarkMeta;

class PowerBookmarkDataProvider {
 public:
  PowerBookmarkDataProvider() = default;
  virtual ~PowerBookmarkDataProvider() = default;

  // Allow features the opportunity to add metadata to `meta` when a bookmark
  // `node` is created. The `meta` object is attached to and stored with the
  // bookmark.
  virtual void AttachMetadataForNewBookmark(const bookmarks::BookmarkNode* node,
                                            PowerBookmarkMeta* meta) = 0;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_DATA_PROVIDER_H_