// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_MENU_DELEGATE_H_

#include <cstddef>
#include <map>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

class BrowserWindowInterface;

namespace views {
class MenuItemView;
class Widget;
}  // namespace views

// BookmarkBarMenuDelegate specializes BookmarkMenuDelegate for standalone
// bookmark bar menus, managing root menu creation, start-index offsets for
// overflow menus, and bookmark-bar specific deletion behavior.
class BookmarkBarMenuDelegate : public BookmarkMenuDelegate {
 public:
  BookmarkBarMenuDelegate(BrowserWindowInterface* browser,
                          views::Widget* parent,
                          views::MenuDelegate* real_delegate,
                          BookmarkLaunchLocation location);

  BookmarkBarMenuDelegate(const BookmarkBarMenuDelegate&) = delete;
  BookmarkBarMenuDelegate& operator=(const BookmarkBarMenuDelegate&) = delete;

  ~BookmarkBarMenuDelegate() override;

  // Makes the menu for `folder` the active menu. `start_index` is the index of
  // the first child of `folder` to show in the menu.
  void SetActiveMenu(const BookmarkParentFolder& folder, size_t start_index);

  // Updates the start index of the given `folder` and updates its menu
  // accordingly.
  void SetMenuStartIndex(const BookmarkParentFolder& folder,
                         size_t start_index);

  // Returns the active root menu.
  views::MenuItemView* menu() { return menu_; }

 protected:
  // BookmarkMenuDelegate:
  bool ShouldCloseOnRemove(const BookmarkFolderOrURL& node) const override;
  std::optional<size_t> AdjustInsertionIndex(const BookmarkParentFolder& folder,
                                             views::MenuItemView* parent_menu,
                                             size_t new_index) override;

 private:
  // Creates a standalone root menu item for `parent` starting at
  // `start_child_index`.
  views::MenuItemView* CreateMenu(const BookmarkParentFolder& parent,
                                  size_t start_child_index);

  // Current active root menu item created by this delegate.
  raw_ptr<views::MenuItemView> menu_ = nullptr;

  // For root menu items created by `CreateMenu`, stores the `start_child_idx`
  // used when building the menu.
  std::map<BookmarkParentFolder, size_t> node_start_child_idx_map_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_MENU_DELEGATE_H_
