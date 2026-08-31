// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_

#include <stdint.h>

enum class WindowOpenDisposition;

class BookmarkBarActionAdapter {
 public:
  virtual ~BookmarkBarActionAdapter() = default;

  virtual void OpenAppsPage(WindowOpenDisposition disposition) = 0;
  virtual void OpenBookmark(int64_t node_id,
                            WindowOpenDisposition disposition) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_ACTION_ADAPTER_H_
