// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

namespace content {
class PageNavigator;
}

// An interface implemented by an object that performs actions on the actual
// menu for the controller.
class BookmarkContextMenuControllerDelegate {
 public:
  virtual ~BookmarkContextMenuControllerDelegate() {}

  // Closes the bookmark context menu.
  virtual void CloseMenu() = 0;

  // Sent before any command from the menu is executed.
  virtual void WillExecuteCommand(
      int command_id,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {}

  // Sent after any command from the menu is executed.
  virtual void DidExecuteCommand(int command_id) {}
};

// BookmarkContextMenuController creates and manages state for the context menu
// shown for any bookmark item.
class BookmarkContextMenuController
    : public bookmarks::BaseBookmarkModelObserver,
      public ui::SimpleMenuModel::Delegate {
 public:
  // Creates the bookmark context menu.
  // |browser| is used to open the bookmark manager and is null in tests.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  BookmarkContextMenuController(
      gfx::NativeWindow parent_window,
      BookmarkContextMenuControllerDelegate* delegate,
      Browser* browser,
      Profile* profile,
      content::PageNavigator* navigator,
      BookmarkLaunchLocation opened_from,
      const bookmarks::BookmarkNode* parent,
      const std::vector<const bookmarks::BookmarkNode*>& selection);
  ~BookmarkContextMenuController() override;

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  // ui::SimpleMenuModel::Delegate implementation:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;

  void set_navigator(content::PageNavigator* navigator) {
    navigator_ = navigator;
  }

 private:
  void BuildMenu();

  // Adds a IDC_* style command to the menu with a string16.
  void AddItem(int id, const base::string16 str);
  // Adds a IDC_* style command to the menu with a localized string.
  void AddItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds a checkable item to the menu.
  void AddCheckboxItem(int id, int localization_id);

  // Overridden from bookmarks::BaseBookmarkModelObserver:
  // Any change to the model results in closing the menu.
  void BookmarkModelChanged() override;

  gfx::NativeWindow parent_window_;
  BookmarkContextMenuControllerDelegate* delegate_;
  Browser* const browser_;
  Profile* profile_;
  content::PageNavigator* navigator_;
  const BookmarkLaunchLocation opened_from_;
  const bookmarks::BookmarkNode* parent_;
  std::vector<const bookmarks::BookmarkNode*> selection_;
  bookmarks::BookmarkModel* model_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  // Used to detect deletion of |this| executing a command.
  base::WeakPtrFactory<BookmarkContextMenuController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuController);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
