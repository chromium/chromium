// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "ui/views/controls/menu/menu_delegate.h"

class Browser;
class Profile;

namespace bookmarks {
class ManagedBookmarkService;
}

namespace content {
class PageNavigator;
}

namespace gfx {
class ImageSkia;
}

namespace ui {
class OSExchangeData;
}

namespace views {
class MenuItemView;
class Widget;
}

// BookmarkMenuDelegate acts as the (informal) views::MenuDelegate for showing
// bookmarks in a MenuItemView. BookmarkMenuDelegate informally implements
// MenuDelegate as its assumed another class is going to forward the appropriate
// methods to this class. Doing so allows this class to be used for both menus
// on the bookmark bar and the bookmarks in the app menu.
class BookmarkMenuDelegate : public bookmarks::BaseBookmarkModelObserver,
                             public BookmarkContextMenuObserver {
 public:
  enum ShowOptions {
    // Indicates a menu should be added containing the permanent folders (other
    // than then bookmark bar folder). This only makes sense when showing the
    // contents of the bookmark bar folder.
    SHOW_PERMANENT_FOLDERS,

    // Don't show any additional folders.
    HIDE_PERMANENT_FOLDERS
  };

  BookmarkMenuDelegate(Browser* browser,
                       content::PageNavigator* navigator,
                       views::Widget* parent);
  ~BookmarkMenuDelegate() override;

  // Creates the menus from the model.
  void Init(views::MenuDelegate* real_delegate,
            views::MenuItemView* parent,
            const bookmarks::BookmarkNode* node,
            size_t start_child_index,
            ShowOptions show_options,
            BookmarkLaunchLocation location);

  // Sets the PageNavigator.
  void SetPageNavigator(content::PageNavigator* navigator);

  // Returns the id given to the next menu.
  int next_menu_id() const { return next_menu_id_; }

  // Makes the menu for |node| the active menu. |start_index| is the index of
  // the first child of |node| to show in the menu.
  void SetActiveMenu(const bookmarks::BookmarkNode* node, size_t start_index);

  bookmarks::BookmarkModel* GetBookmarkModel() {
    return const_cast<bookmarks::BookmarkModel*>(
        const_cast<const BookmarkMenuDelegate*>(this)->GetBookmarkModel());
  }
  const bookmarks::BookmarkModel* GetBookmarkModel() const;
  bookmarks::ManagedBookmarkService* GetManagedBookmarkService();

  // Returns the menu.
  views::MenuItemView* menu() { return menu_; }

  // Returns the context menu, or NULL if the context menu isn't showing.
  views::MenuItemView* context_menu() {
    return context_menu_.get() ? context_menu_->menu() : NULL;
  }

  views::Widget* parent() { return parent_; }
  const views::Widget* parent() const { return parent_; }

  // Returns true if we're in the process of mutating the model. This happens
  // when the user deletes menu items using the context menu.
  bool is_mutating_model() const { return is_mutating_model_; }

  // MenuDelegate like methods (see class description for details).
  base::string16 GetTooltipText(int id, const gfx::Point& p) const;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const ui::Event& e);
  void ExecuteCommand(int id, int mouse_event_flags);
  bool ShouldExecuteCommandWithoutClosingMenu(int id, const ui::Event& e);
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types);
  bool AreDropTypesRequired(views::MenuItemView* menu);
  bool CanDrop(views::MenuItemView* menu, const ui::OSExchangeData& data);
  int GetDropOperation(views::MenuItemView* item,
                       const ui::DropTargetEvent& event,
                       views::MenuDelegate::DropPosition* position);
  int OnPerformDrop(views::MenuItemView* menu,
                    views::MenuDelegate::DropPosition position,
                    const ui::DropTargetEvent& event);
  bool ShowContextMenu(views::MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type);
  bool CanDrag(views::MenuItemView* menu);
  void WriteDragData(views::MenuItemView* sender, ui::OSExchangeData* data);
  int GetDragOperations(views::MenuItemView* sender);
  int GetMaxWidthForMenu(views::MenuItemView* menu);
  void WillShowMenu(views::MenuItemView* menu);

  // BookmarkModelObserver methods.
  void BookmarkModelChanged() override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;

  // BookmarkContextMenuObserver methods.
  void WillRemoveBookmarks(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) override;
  void DidRemoveBookmarks() override;
  void OnContextMenuClosed() override;

 private:
  friend class BookmarkMenuDelegateTest;

  typedef std::map<int, const bookmarks::BookmarkNode*> MenuIDToNodeMap;
  typedef std::map<const bookmarks::BookmarkNode*, views::MenuItemView*>
      NodeToMenuMap;

  // Returns whether the menu should close id 'delete' is selected.
  bool ShouldCloseOnRemove(const bookmarks::BookmarkNode* node) const;

  // Creates a menu. This uses BuildMenu() to recursively populate the menu.
  views::MenuItemView* CreateMenu(const bookmarks::BookmarkNode* parent,
                                  size_t start_child_index,
                                  ShowOptions show_options);

  // Invokes BuildMenuForPermanentNode() for the permanent nodes (excluding
  // 'other bookmarks' folder).
  void BuildMenusForPermanentNodes(views::MenuItemView* menu);

  // If |node| has children a new menu is created and added to |menu| to
  // represent it. If |node| is not empty and |added_separator| is false, a
  // separator is added before the new menu items and |added_separator| is set
  // to true.
  void BuildMenuForPermanentNode(const bookmarks::BookmarkNode* node,
                                 const gfx::ImageSkia& icon,
                                 views::MenuItemView* menu,
                                 bool* added_separator);

  void BuildMenuForManagedNode(views::MenuItemView* menu);

  // Creates an entry in menu for each child node of |parent| starting at
  // |start_child_index|.
  void BuildMenu(const bookmarks::BookmarkNode* parent,
                 size_t start_child_index,
                 views::MenuItemView* menu);

  // Registers the necessary mappings for |menu| and |node|.
  void AddMenuToMaps(views::MenuItemView* menu,
                     const bookmarks::BookmarkNode* node);

  // Escapes ampersands within |title| if necessary, depending on
  // |menu_uses_mnemonics_|.
  base::string16 MaybeEscapeLabel(const base::string16& title);

  Browser* const browser_;
  Profile* profile_;

  content::PageNavigator* page_navigator_;

  // Parent of menus.
  views::Widget* parent_;

  // Maps from menu id to BookmarkNode.
  MenuIDToNodeMap menu_id_to_node_map_;

  // Current menu.
  views::MenuItemView* menu_;

  // Data for the drop.
  bookmarks::BookmarkNodeData drop_data_;

  // Used when a context menu is shown.
  std::unique_ptr<BookmarkContextMenu> context_menu_;

  // If non-NULL this is the |parent| passed to Init and is NOT owned by us.
  views::MenuItemView* parent_menu_item_;

  // Maps from node to menu.
  NodeToMenuMap node_to_menu_map_;

  // ID of the next menu item.
  int next_menu_id_;

  views::MenuDelegate* real_delegate_;

  // Is the model being changed?
  bool is_mutating_model_;

  // The location where this bookmark menu will be displayed (for UMA).
  BookmarkLaunchLocation location_;

  // Whether the involved menu uses mnemonics or not. If it does, ampersands
  // inside bookmark titles need to be escaped.
  bool menu_uses_mnemonics_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
