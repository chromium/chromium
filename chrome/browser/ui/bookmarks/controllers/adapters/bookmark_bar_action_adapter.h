// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include <stdint.h>

#include "chrome/browser/bookmarks/bookmark_node_types.h"

enum class WindowOpenDisposition;

class BookmarkBarActionAdapter {
 public:
  virtual ~BookmarkBarActionAdapter() = default;

  // Opens the bookmark app page.
  virtual void OpenAppsPage(WindowOpenDisposition disposition) = 0;

  // Opens the bookmark node with the given id.
  virtual void OpenBookmark(int64_t node_id,
                            WindowOpenDisposition disposition) = 0;

  // Notifies the adapter that a folder menu was opened.
  virtual void NotifyFolderOpened() = 0;

  // Opens all bookmarks in the folder using the given disposition.
  virtual void OpenFolderNodes(const bookmarks::BookmarkNodeId& folder,
                               WindowOpenDisposition disposition) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
