// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser_user_data.h"

class Browser;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

// BookmarksSidePanelCoordinator handles the creation and registration of the
// bookmarks SidePanelEntry.
class BookmarksSidePanelCoordinator
    : public BrowserUserData<BookmarksSidePanelCoordinator> {
 public:
  explicit BookmarksSidePanelCoordinator(Browser* browser);
  ~BookmarksSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  friend class BrowserUserData<BookmarksSidePanelCoordinator>;

  std::unique_ptr<views::View> CreateBookmarksWebView();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_
