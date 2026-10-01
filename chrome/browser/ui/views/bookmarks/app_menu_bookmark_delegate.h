// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_APP_MENU_BOOKMARK_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_APP_MENU_BOOKMARK_DELEGATE_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"

class BrowserWindowInterface;

namespace views {
class MenuDelegate;
class MenuItemView;
class View;
class Widget;
}  // namespace views

// AppMenuBookmarkDelegate specializes BookmarkMenuDelegate for the AppMenu
// (three-dots menu), embedding the bookmarks submenu into an existing parent
// menu item with title headers and permanent folders.
class AppMenuBookmarkDelegate : public BookmarkMenuDelegate {
 public:
  AppMenuBookmarkDelegate(
      BrowserWindowInterface* browser,
      views::Widget* parent,
      views::MenuDelegate* real_delegate,
      BookmarkLaunchLocation location = BookmarkLaunchLocation::kAppMenu);

  AppMenuBookmarkDelegate(const AppMenuBookmarkDelegate&) = delete;
  AppMenuBookmarkDelegate& operator=(const AppMenuBookmarkDelegate&) = delete;

  ~AppMenuBookmarkDelegate() override;

  // Populates `parent` with the bookmark bar nodes and permanent node submenus.
  void BuildFullMenu(views::MenuItemView* parent);

 protected:
  // BookmarkMenuDelegate:
  std::optional<size_t> AdjustInsertionIndex(const BookmarkParentFolder& folder,
                                             views::MenuItemView* parent_menu,
                                             size_t new_index) override;
  std::vector<raw_ref<views::MenuItemView>> GetAndUpdateStaleMenuArtifacts()
      override;

 private:
  // Returns true if `folder` has child nodes.
  bool ShouldBuildPermanentNode(const BookmarkParentFolder& folder) const;

  // Builds menus for the 'other' and 'mobile' nodes if they're not empty,
  // adding them to `parent_menu_item_`.
  void BuildMenusForPermanentNodes();

  // Adds or removes the bookmarks title + separator as necessary.
  // Returns the updated menu if there were changes; otherwise, returns null.
  views::MenuItemView* UpdateBookmarksTitle();
  bool ShouldHaveBookmarksTitle();
  void BuildBookmarksTitle(size_t index);
  void RemoveBookmarksTitle();

  // The parent menu item passed to BuildFullMenu; not owned by us.
  raw_ptr<views::MenuItemView> parent_menu_item_ = nullptr;

  // Views built by this delegate, owned by `parent_menu_item_`.
  raw_ptr<views::View> bookmarks_title_ = nullptr;
  raw_ptr<views::View> bookmarks_title_separator_ = nullptr;
  raw_ptr<views::View> permanent_nodes_separator_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_APP_MENU_BOOKMARK_DELEGATE_H_
