// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/actions/actions.h"

namespace bookmarks {
class BookmarkNode;
}

class BookmarkMergedSurfaceService;

class BookmarksDynamicMenu {
 public:
  explicit BookmarksDynamicMenu(BrowserWindowInterface* browser);
  ~BookmarksDynamicMenu();

  base::WeakPtr<BookmarksDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildBookmarksActions(actions::BaseAction* parent_item);

 private:
  void AddBookmarkNodeAction(actions::BaseAction* parent_item,
                             const bookmarks::BookmarkNode* node,
                             BookmarkMergedSurfaceService* service);

  void AddBookmarkFolderAction(actions::BaseAction* parent_item,
                               const BookmarkParentFolder& folder,
                               BookmarkMergedSurfaceService* service);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  base::WeakPtrFactory<BookmarksDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_BOOKMARKS_DYNAMIC_MENU_H_
