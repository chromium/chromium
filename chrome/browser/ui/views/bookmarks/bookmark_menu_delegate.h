// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_

#include <map>
#include <optional>
#include <set>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/view.h"

class BookmarkMergedSurfaceService;
class BrowserWindowInterface;
class Profile;


namespace ui {
class OSExchangeData;
}  // namespace ui

namespace views {
class MenuItemView;
class Widget;
}  // namespace views

// BookmarkMenuDelegate acts as the (informal) views::MenuDelegate for showing
// bookmarks in a MenuItemView. BookmarkMenuDelegate informally implements
// MenuDelegate as its assumed another class is going to forward the appropriate
// methods to this class. Doing so allows this class to be used for both menus
// on the bookmark bar and the bookmarks in the app menu.
// BookmarkMenuDelegate is a bookmark merged surface, it combines local and
// account bookmark nodes (see `BookmarkParentFolder`). This class must use
// `BookmarkMergedSurfaceService` to retrieve bookmarks and their indexes.
// TODO(crbug.com/382749219): This class has some unnecessary complexity
// stemming from the fact that it's trying to handle distinct requirements from
// various clients. This client-specific logic should be split out.
class BookmarkMenuDelegate : public BookmarkMergedSurfaceServiceObserver,
                             public BookmarkContextMenuObserver {
 public:
  BookmarkMenuDelegate(BrowserWindowInterface* browser,
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

  // Makes the menu for `folder` the active menu. `start_index` is the index of
  // the first child of `folder` to show in the menu.
  void SetActiveMenu(const BookmarkParentFolder& folder, size_t start_index);

  // Updates the start index of the given `folder` and updates its menu
  // accordingly.
  void SetMenuStartIndex(const BookmarkParentFolder& folder,
                         size_t start_index);

  // Returns the id given to the next menu.
  int next_menu_id() const { return next_menu_id_; }

  BookmarkMergedSurfaceService* GetBookmarkMergedSurfaceService() {
    return const_cast<BookmarkMergedSurfaceService*>(
        const_cast<const BookmarkMenuDelegate*>(this)
            ->GetBookmarkMergedSurfaceService());
  }
  const BookmarkMergedSurfaceService* GetBookmarkMergedSurfaceService() const;

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
  bool IsTriggerableEvent(views::MenuItemView* menu, const ui::Event& e);
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
  void RunContextMenuAt(std::vector<int64_t> node_ids,
                        const gfx::Point& p,
                        ui::mojom::MenuSourceType source_type,
                        bool close_on_remove,
                        bool can_paste);
  bool CanDrag(views::MenuItemView* menu);
  void WriteDragData(views::MenuItemView* sender, ui::OSExchangeData* data);
  int GetDragOperations(views::MenuItemView* sender);
  int GetMaxWidthForMenu(views::MenuItemView* menu);
  void WillShowMenu(views::MenuItemView* menu);

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override {}
  void BookmarkMergedSurfaceServiceBeingDeleted() override {}
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override {}
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override {}
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override {}
  void BookmarkAllUserNodesRemoved() override {}

  // BookmarkContextMenuObserver methods.
  void WillRemoveBookmarks(
      const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                VectorExperimental>>& bookmarks) override;
  void DidRemoveBookmarks() override;
  void OnContextMenuClosed() override;

 protected:
  class BookmarkFolderOrURL {
   public:
    explicit BookmarkFolderOrURL(const bookmarks::BookmarkNode* node);

    explicit BookmarkFolderOrURL(const BookmarkParentFolder& folder);

    ~BookmarkFolderOrURL();

    BookmarkFolderOrURL(const BookmarkFolderOrURL& other);
    BookmarkFolderOrURL& operator=(const BookmarkFolderOrURL& other);

    friend bool operator==(const BookmarkFolderOrURL&,
                           const BookmarkFolderOrURL&) = default;

    friend auto operator<=>(const BookmarkFolderOrURL&,
                            const BookmarkFolderOrURL&) = default;

    const BookmarkParentFolder* GetIfBookmarkFolder() const;

    const bookmarks::BookmarkNode* GetIfBookmarkURL() const;

    const bookmarks::BookmarkNode* GetIfNonPermanentNode() const;

    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
    GetUnderlyingNodes(
        const BookmarkMergedSurfaceService* bookmark_merged_service) const;

   private:
    static std::variant<BookmarkParentFolder,
                        raw_ptr<const bookmarks::BookmarkNode>>
    GetFromNode(const bookmarks::BookmarkNode* node);

    std::variant<BookmarkParentFolder, raw_ptr<const bookmarks::BookmarkNode>>
        folder_or_url_;
  };

  // Returns whether the menu should close if 'delete' is selected.
  virtual bool ShouldCloseOnRemove(const BookmarkFolderOrURL& node) const;

  // Maps an index in the bookmark model onto the index at which the
  // corresponding menu item should be inserted. The two differ because a menu
  // may show only a slice of `folder` (bookmarks that overflow the bookmark
  // bar), and because a menu may contain non-bookmark items that precede the
  // bookmarks (e.g. the app menu's "Bookmarks" title, or the managed bookmarks
  // folder).
  //
  // `folder` is the model parent the node is being added to. `parent_menu` is
  // the menu item that will receive the new child item. `new_index` is the
  // node's index among `folder`'s children *in the model*, not in the menu.
  //
  // Returns the index within `parent_menu`, or nullopt if the node falls
  // before the range `parent_menu` displays, in which case no menu item should
  // be added.
  virtual std::optional<size_t> AdjustInsertionIndex(
      const BookmarkParentFolder& folder,
      views::MenuItemView* parent_menu,
      size_t new_index);

  // Brings the non-bookmark menu items this class owns back in sync with the
  // model after bookmarks have been added or removed. E.g., removes the
  // separator in the "other" bookmarks folder once it has no more children.
  //
  // Subclasses that own additional non-bookmark items should override this,
  // call the base implementation first, and append their own updated menus to
  // the result. Overrides must not call `ChildrenChanged()` themselves:
  // returning a menu here hands that responsibility to the caller, which
  // batches the notifications and knows when it is safe to issue them (it is
  // not safe while a drag loop is running).
  //
  // Returns every menu whose children changed.
  virtual std::vector<raw_ref<views::MenuItemView>>
  GetAndUpdateStaleMenuArtifacts();

 private:
  friend class BookmarkMenuDelegateTest;

  using MenuIDToNodeMap = std::map<int, BookmarkFolderOrURL>;
  using NodeToMenuMap =
      std::map<BookmarkFolderOrURL, raw_ptr<views::MenuItemView>>;

  // Creates a menu. This uses BuildMenu() to recursively populate the menu.
  views::MenuItemView* CreateMenu(const BookmarkParentFolder& parent,
                                  size_t start_child_index);

  // Returns true if `folder` has child nodes.
  bool ShouldBuildPermanentNode(const BookmarkParentFolder& folder) const;

  // Builds menus for the 'other' and 'mobile' nodes if they're not empty,
  // adding them to `parent_menu_item_`.
  void BuildMenusForPermanentNodes();

  // Builds a submenu item for the provided bookmark folder, adding it to
  // `parent_menu`.
  void BuildMenuForFolder(const BookmarkParentFolder& folder,
                          const ui::ImageModel& icon,
                          views::MenuItemView* parent_menu);

  // Creates an entry in menu for each child node of `folder` starting at
  // `start_child_index`.
  void BuildMenu(const BookmarkParentFolder& folder,
                 size_t start_child_index,
                 views::MenuItemView* menu);

  // Registers the necessary mappings for |menu| and |node|.
  void AddMenuToMaps(views::MenuItemView* menu,
                     const BookmarkFolderOrURL& node);

  // Returns |next_menu_id_| and increments it by
  // `AppMenuModel::kNumUnboundedMenuTypes`. This allows for interleaved command
  // IDs across unbounded menu types in the app menu.
  int GetAndIncrementNextMenuID();

  // Removes `node` and its `menu`'s view. All descendants of the removed node
  // are also removed.
  void RemoveBookmarkNode(const bookmarks::BookmarkNode* node,
                          views::MenuItemView* menu);

  // Builds a menu for `node`, inserting it into `new_parent_menu`.
  // `new_index` is `node`'s position relative to other bookmarks in its parent
  // folder, and is used to determine where the new menu should be inserted.
  // This also considers other non-bookmark menu items (e.g. "Bookmarks" title)
  // when determining where to insert the menu.
  void AddBookmarkNode(const bookmarks::BookmarkNode* node,
                       views::MenuItemView* new_parent_menu,
                       size_t new_index);

  // Adds or removes the bookmarks title + separator as necessary.
  // Returns the updated menu if there were changes; otherwise, returns null.
  views::MenuItemView* UpdateBookmarksTitle();
  bool ShouldHaveBookmarksTitle();
  void BuildBookmarksTitle(size_t index);
  void RemoveBookmarksTitle();

  void BuildMenuForFolderAt(const BookmarkParentFolder& folder,
                            const ui::ImageModel& icon,
                            views::MenuItemView* parent_menu,
                            size_t index);

  // Builds a menu item for the provided bookmark url, adding it to
  // `parent_menu`.
  void BuildMenuForURLAt(const bookmarks::BookmarkNode* node,
                         views::MenuItemView* parent_menu,
                         size_t index);

  // Build a menu item for the providied bookmark folder or url, adding it to
  // `parent_menu`.
  void BuildNodeMenuItem(const bookmarks::BookmarkNode* node,
                         views::MenuItemView* parent_menu);
  void BuildNodeMenuItemAt(const bookmarks::BookmarkNode* node,
                           views::MenuItemView* parent_menu,
                           size_t index);

  // Escapes ampersands within |title| if necessary, depending on
  // |menu_uses_mnemonics_|.
  std::u16string MaybeEscapeLabel(const std::u16string& title);

  // Adds or removes the separator of the "other" bookmarks folder as necessary.
  // Returns the updated menu if there were changes; otherwise, returns null.
  views::MenuItemView* UpdateOtherNodeSeparator();
  void BuildOtherNodeMenuHeader(views::MenuItemView* menu);

  bool IsDropValid(const BookmarkFolderOrURL* target,
                   const views::MenuDelegate::DropPosition* position);

  struct DropParams {
    BookmarkParentFolder drop_parent;
    size_t index_to_drop_at = 0;
  };

  // Computes the parent and the index at which the dragged/copied node will
  // be dropped. Returns `std::nullopt` if the drop is not valid.
  std::optional<DropParams> GetDropParams(
      views::MenuItemView* menu,
      views::MenuDelegate::DropPosition* position);

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<Profile> profile_;

  // Parent of menus.
  raw_ptr<views::Widget> parent_;

  // Maps from menu id to BookmarkNode.
  MenuIDToNodeMap menu_id_to_node_map_;

  // Current menu.
  raw_ptr<views::MenuItemView> menu_;

  // If non-NULL this is the |parent| passed to BuildFullMenu and is NOT owned
  // by us.
  raw_ptr<views::MenuItemView> parent_menu_item_;

  // Views built by this delegate, but not tracked by the maps.
  // These are all owned by `parent_menu_item_`, if not null.
  raw_ptr<views::View> bookmarks_title_;
  raw_ptr<views::View> bookmarks_title_separator_;
  raw_ptr<views::View> permanent_nodes_separator_;

  // The separator within the "other" bookmarks menu.
  raw_ptr<views::View> other_node_menu_separator_;

  // Maps from node to menu.
  NodeToMenuMap node_to_menu_map_;

  // For root menu items created by `CreateMenu`, stores the `start_child_idx`
  // used when building the menu.
  std::map<BookmarkParentFolder, size_t> node_start_child_idx_map_;

  // Nodes whose submenus have been built (i.e. `BuildMenu` was called on them).
  std::set<BookmarkParentFolder> built_nodes_;

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

  // Data for the drop.
  bookmarks::BookmarkNodeData drop_data_;

  // Used when a context menu is shown.
  std::unique_ptr<BookmarkContextMenu> context_menu_;

  base::ScopedObservation<BookmarkContextMenu, BookmarkContextMenuObserver>
      bookmark_context_menu_observation_{this};

  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      bookmark_merged_service_observation_{this};

  base::WeakPtrFactory<BookmarkMenuDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_DELEGATE_H_
