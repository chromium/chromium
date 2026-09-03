// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_H_

#include <stdint.h>

#include "chrome/browser/bookmarks/bookmark_node_types.h"

enum class WindowOpenDisposition;

class BookmarkBarUIClient;

class BookmarkBarUIController {
 public:
  virtual ~BookmarkBarUIController() = default;

  // --- Registration / Lifecycle ---
  virtual void Bind(BookmarkBarUIClient* client) = 0;

  // --- Actions ---
  virtual void OpenAppsPage(WindowOpenDisposition disposition) = 0;
  virtual void OpenBookmark(int64_t node_id,
                            WindowOpenDisposition disposition) = 0;
  virtual void OpenFolder(const bookmarks::BookmarkNodeId& folder,
                          WindowOpenDisposition disposition) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_H_
