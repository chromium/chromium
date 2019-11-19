// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "ui/views/controls/menu/menu_delegate.h"

class Browser;

namespace views {
class MenuRunner;
class Widget;
}

// Observer for the BookmarkContextMenu.
class BookmarkContextMenuObserver {
 public:
  // Invoked before the specified items are removed from the bookmark model.
  virtual void WillRemoveBookmarks(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) = 0;

  // Invoked after the items have been removed from the model.
  virtual void DidRemoveBookmarks() = 0;

  // Invoked when the context menu is closed.
  virtual void OnContextMenuClosed() = 0;

 protected:
  virtual ~BookmarkContextMenuObserver() {}
};

class BookmarkContextMenu : public BookmarkContextMenuControllerDelegate,
                            public views::MenuDelegate {
 public:
  // |browser| is used to open the bookmark manager, and is NULL in tests.
  BookmarkContextMenu(
      views::Widget* parent_widget,
      Browser* browser,
      Profile* profile,
      content::PageNavigator* page_navigator,
      BookmarkLaunchLocation opened_from,
      const bookmarks::BookmarkNode* parent,
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      bool close_on_remove);
  ~BookmarkContextMenu() override;

  // Installs a callback to be run before the context menu is run. The callback
  // runs only once, and only one such callback can be set at any time. Once the
  // installed callback is run, another callback can be installed.
  static void InstallPreRunCallback(base::OnceClosure callback);

  // Shows the context menu at the specified point.
  void RunMenuAt(const gfx::Point& point,
                 ui::MenuSourceType source_type);

  views::MenuItemView* menu() const { return menu_; }

  void set_observer(BookmarkContextMenuObserver* observer) {
    observer_ = observer;
  }

  // Sets the PageNavigator.
  void SetPageNavigator(content::PageNavigator* navigator);

  // Overridden from views::MenuDelegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsItemChecked(int command_id) const override;
  bool IsCommandEnabled(int command_id) const override;
  bool IsCommandVisible(int command_id) const override;
  bool ShouldCloseAllMenusOnExecute(int id) override;
  void OnMenuClosed(views::MenuItemView* menu) override;

  // Overridden from BookmarkContextMenuControllerDelegate:
  void CloseMenu() override;
  void WillExecuteCommand(
      int command_id,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) override;
  void DidExecuteCommand(int command_id) override;

 private:
  std::unique_ptr<BookmarkContextMenuController> controller_;

  // The parent of dialog boxes opened from the context menu.
  views::Widget* parent_widget_;

  // The menu itself. This is owned by |menu_runner_|.
  views::MenuItemView* menu_;

  // Responsible for running the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  BookmarkContextMenuObserver* observer_;

  // Should the menu close when a node is removed.
  bool close_on_remove_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_H_
