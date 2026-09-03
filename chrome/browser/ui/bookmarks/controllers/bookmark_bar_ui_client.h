// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CLIENT_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CLIENT_H_

#include "chrome/browser/bookmarks/bookmark_node_types.h"

class BookmarkBarUIClient {
 public:
  virtual ~BookmarkBarUIClient() = default;

  virtual void SetAppsPageShortcutVisibility(bool visible) = 0;
  virtual void SetSavedTabGroupsVisibility(bool visible) = 0;
  virtual void SetManagedBookmarksFolderVisibility(bool visible) = 0;
  virtual void ShowFolderMenu(const bookmarks::BookmarkNodeId& folder) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CLIENT_H_
