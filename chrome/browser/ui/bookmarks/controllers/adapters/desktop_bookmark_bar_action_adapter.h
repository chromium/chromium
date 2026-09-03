// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"

class BrowserWindowInterface;

class DesktopBookmarkBarActionAdapter : public BookmarkBarActionAdapter {
 public:
  explicit DesktopBookmarkBarActionAdapter(BrowserWindowInterface* browser);
  ~DesktopBookmarkBarActionAdapter() override;

  // BookmarkBarActionAdapter overrides:
  void OpenAppsPage(WindowOpenDisposition disposition) override;
  void OpenBookmark(int64_t node_id,
                    WindowOpenDisposition disposition) override;
  void NotifyFolderOpened() override;
  void OpenFolderNodes(const bookmarks::BookmarkNodeId& folder,
                       WindowOpenDisposition disposition) override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_DESKTOP_BOOKMARK_BAR_ACTION_ADAPTER_H_
