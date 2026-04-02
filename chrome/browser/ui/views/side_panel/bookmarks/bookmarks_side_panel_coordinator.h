// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

// BookmarksSidePanelCoordinator handles the creation and registration of the
// bookmarks SidePanelEntry.
class BookmarksSidePanelCoordinator {
 public:
  explicit BookmarksSidePanelCoordinator(
      BrowserWindowInterface& browser_window_interface);
  ~BookmarksSidePanelCoordinator();

  static BookmarksSidePanelCoordinator* From(BrowserWindowInterface* browser);

  DECLARE_USER_DATA(BookmarksSidePanelCoordinator);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  std::unique_ptr<views::View> CreateBookmarksWebView(
      SidePanelEntryScope& scope);

  ui::ScopedUnownedUserData<BookmarksSidePanelCoordinator>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_BOOKMARKS_BOOKMARKS_SIDE_PANEL_COORDINATOR_H_
