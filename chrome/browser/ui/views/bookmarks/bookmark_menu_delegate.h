// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/view.h"

class Browser;
class Profile;

namespace bookmarks {
class ManagedBookmarkService;
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
  BookmarkMenuDelegate(Browser* browser,
                       views::Widget* parent,
                       views::MenuDelegate* real_delegate,
                       BookmarkLaunchLocation location);

  BookmarkMenuDelegate(const BookmarkMenuDelegate&) = delete;
  BookmarkMenuDelegate& operator=(const BookmarkMenuDelegate&) = delete;

  ~BookmarkMenuDelegate() override;

  // Extends the `parent` menu by adding items for all relevant bookmark nodes,
  // including:
  // - a folder for managed nodes, if any
  // - each bookmark bar node
  // - a folder for 'other' nodes, if any
  // - a folder for mobile nodes, if any
  void BuildFullMenu(views::MenuItemView* parent);

  // Makes the menu for |node| the active menu. |start_index| is the index of
  // the first child of |node| to show in the menu.
  void SetActiveMenu(const bookmarks::BookmarkNode* node, size_t start_index);

  // Returns the id given to the next menu.
  int next_menu_id() const { return next_menu_id_; }

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
    return context_menu_ ? context_menu_->menu() : nullptr;
  }

  views::Widget* parent() { return parent_; }
  const views::Widget* parent() const { return parent_; }

  // Returns true if we're in the process of mutating the model. This happens
  // when the user deletes menu items using the context menu.
  bool is_mutating_model() const { return is_mutating_model_; }

  // MenuDelegate like methods (see class description for details).
  std::u16string GetTooltipText(int id, const gfx::Point& p) const;
  bool IsTriggerableEvent(views::MenuItemView* menu,
                          const ui::Event& e);
  void ExecuteCommand(int id, int mouse_event_flags);
  bool ShouldExecuteCommandWithoutClosingMenu(int id, const ui::Event& e);
  bool GetDropFormats(views::MenuItemView* menu,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types);
  bool AreDropTypesRequired(views::MenuItemView* menu);
  bool CanDrop(views::MenuItemView* menu, const ui::OSExchangeData& data);
  ui::mojom::DragOperation GetDropOperation(
      views::MenuItemView* item,
      const ui::DropTargetEvent& event,
      views::MenuDelegate::DropPosition* position);
  views::View::DropCallback GetDropCallback(
      views::MenuItemView* menu,
      views::MenuDelegate::DropPosition position,
      const ui::DropTargetEvent& event);
  bool ShowContextMenu(views::MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::mojom::MenuSourceType source_type);
  bool CanDrag(views::MenuItemView* menu);
  void WriteDragData(views::MenuItemView* sender, ui::OSExchangeData* data);
  int GetDragOperations(views::MenuItemView* sender);
  int GetMaxWidthForMenu(views::MenuItemView* menu);
  void WillShowMenu(views::MenuItemView* menu);

  // BookmarkModelObserver methods.
  void BookmarkModelChanged() override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;

  // BookmarkContextMenuObserver methods.
  void WillRemoveBookmarks(
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& bookmarks) override;
  void DidRemoveBookmarks() override;
  void OnContextMenuClosed() override;

 private:
  friend class BookmarkMenuDelegateTest;
  class BookmarkFolderOrURL {
   public:
    explicit BookmarkFolderOrURL(const bookmarks::BookmarkNode* node);
    ~BookmarkFolderOrURL();

    const BookmarkParentFolder* GetIfBookmarkFolder() const;

    const bookmarks::BookmarkNode* GetIfBookmarkURL() const;

    const bookmarks::BookmarkNode* GetIfNonPermanentNode() const;

    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
    GetUnderlyingNodes(
        BookmarkMergedSurfaceService* bookmark_merged_service) const;

   private:
    static std::variant<BookmarkParentFolder,
                        raw_ptr<const bookmarks::BookmarkNode>>
    GetFromNode(const bookmarks::BookmarkNode* node);

    const std::variant<BookmarkParentFolder,
                       raw_ptr<const bookmarks::BookmarkNode>>
        folder_or_url_;
  };

  typedef std::map<int, raw_ptr<const bookmarks::BookmarkNode, CtnExperimental>>
      MenuIDToNodeMap;
  typedef std::map<const bookmarks::BookmarkNode*, raw_ptr<views::MenuItemView>>
      NodeToMenuMap;

  struct DropParams {
    BookmarkParentFolder drop_parent;
    size_t index_to_drop_at = 0;
  };

  bool IsDropValid(const BookmarkFolderOrURL* target,
                   const views::MenuDelegate::DropPosition* position);

  // Computes the parent and the index at which the dragged/copied node will
  // be dropped. Returns `std::nullopt` if the drop is not valid.
  std::optional<DropParams> GetDropParams(
      views::MenuItemView* menu,
      views::MenuDelegate::DropPosition* position);

  // Returns whether the menu should close id 'delete' is selected.
  bool ShouldCloseOnRemove(const BookmarkFolderOrURL* node) const;

  // Creates a menu. This uses BuildMenu() to recursively populate the menu.
  views::MenuItemView* CreateMenu(const bookmarks::BookmarkNode* parent,
                                  size_t start_child_index);

  // Builds menus for the 'other' and 'mobile' nodes if they're not empty,
  // adding them to `parent_menu_item_`.
  void BuildMenusForPermanentNodes();

  // Builds a submenu item for the provided bookmark folder node, adding it to
  // `parent_menu`.
  void BuildMenuForFolder(const bookmarks::BookmarkNode* node,
                          const ui::ImageModel& icon,
                          views::MenuItemView* parent_menu);

  // Builds a menu item for the provided bookmark url, adding it to
  // `parent_menu`.
  void BuildMenuForURL(const bookmarks::BookmarkNode* node,
                       views::MenuItemView* parent_menu);

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
  std::u16string MaybeEscapeLabel(const std::u16string& title);

  // Returns |next_menu_id_| and increments it by 2. This allows for 'sharing'
  // command ids with the recent tabs menu, which also uses every other int as
  // an id.
  int GetAndIncrementNextMenuID();

  const raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;

  // Parent of menus.
  raw_ptr<views::Widget> parent_;

  // Maps from menu id to BookmarkNode.
  MenuIDToNodeMap menu_id_to_node_map_;

  // Current menu.
  raw_ptr<views::MenuItemView> menu_;

  // Data for the drop.
  bookmarks::BookmarkNodeData drop_data_;

  // Used when a context menu is shown.
  std::unique_ptr<BookmarkContextMenu> context_menu_;

  // If non-NULL this is the |parent| passed to BuildFullMenu and is NOT owned
  // by us.
  raw_ptr<views::MenuItemView> parent_menu_item_;

  // Maps from node to menu.
  NodeToMenuMap node_to_menu_map_;

  // ID of the next menu item.
  int next_menu_id_;

  raw_ptr<views::MenuDelegate> real_delegate_;

  // Is the model being changed?
  bool is_mutating_model_;

  // The location where this bookmark menu will be displayed (for UMA).
  BookmarkLaunchLocation location_;

  // Whether the involved menu uses mnemonics or not. If it does, ampersands
  // inside bookmark titles need to be escaped. In cases where the
  // BookmarkMenuDelegate will be the root, client code does not currently
  // enable mnemonics.
  bool menu_uses_mnemonics_ = false;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      bookmark_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
