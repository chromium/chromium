// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_

#include <memory>
#include <set>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

class BookmarkBarViewObserver;
class BookmarkBarViewTestHelper;
class BookmarkContextMenu;
class Browser;
class BrowserView;
class Profile;

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace content {
class PageNavigator;
}

namespace gfx {
class FontList;
}

namespace tab_groups {
class SavedTabGroupBar;
}

namespace views {
class MenuButton;
class MenuItemView;
class LabelButton;
}  // namespace views

// BookmarkBarView renders the BookmarkModel.  Each starred entry on the
// BookmarkBar is rendered as a MenuButton. An additional MenuButton aligned to
// the right allows the user to quickly see recently starred entries.
//
// BookmarkBarView shows the bookmarks from a specific Profile. BookmarkBarView
// waits until the HistoryService for the profile has been loaded before
// creating the BookmarkModel.
class BookmarkBarView : public views::AccessiblePaneView,
                        public bookmarks::BookmarkModelObserver,
                        public views::ContextMenuController,
                        public views::DragController,
                        public views::AnimationDelegateViews,
                        public BookmarkMenuControllerObserver {
  METADATA_HEADER(BookmarkBarView, views::AccessiblePaneView)

 public:
  class ButtonSeparatorView;

  // |browser_view| can be NULL during tests.
  BookmarkBarView(Browser* browser, BrowserView* browser_view);
  BookmarkBarView(const BookmarkBarView&) = delete;
  BookmarkBarView& operator=(const BookmarkBarView&) = delete;
  ~BookmarkBarView() override;

  static bool GetAnimationsEnabled();

  static void DisableAnimationsForTesting(bool disabled);

  // Returns the current browser.
  Browser* browser() const { return browser_; }

  void AddObserver(BookmarkBarViewObserver* observer);
  void RemoveObserver(BookmarkBarViewObserver* observer);

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(content::PageNavigator* navigator);

  // Sets whether the containing browser is showing an infobar.  This affects
  // layout during animation.
  void SetInfoBarVisible(bool infobar_visible);
  bool GetInfoBarVisible() const;

  // Changes the state of the bookmark bar.
  void SetBookmarkBarState(BookmarkBar::State state,
                           BookmarkBar::AnimateChangeType animate_type);

  // If |loc| is over a bookmark button the node is returned corresponding to
  // the button and |model_start_index| is set to 0. If a overflow button is
  // showing and |loc| is over the overflow button, the bookmark bar node is
  // returned and |model_start_index| is set to the index of the first node
  // contained in the overflow menu.
  const bookmarks::BookmarkNode* GetNodeForButtonAtModelIndex(
      const gfx::Point& loc,
      size_t* model_start_index);

  // Returns the MenuButton for node.
  views::MenuButton* GetMenuButtonForNode(const bookmarks::BookmarkNode* node);

  // Returns the position to anchor the menu for |button| at.
  void GetAnchorPositionForButton(views::MenuButton* button,
                                  views::MenuAnchorPosition* anchor);

  // Returns the size of the leading margin of the bookmarks bar.
  int GetLeadingMargin() const;

  // Returns the button responsible for showing bookmarks in the
  // "Other Bookmarks" folder.
  views::MenuButton* all_bookmarks_button() const {
    return all_bookmarks_button_;
  }

  const tab_groups::SavedTabGroupBar* saved_tab_group_bar() const {
    return saved_tab_group_bar_;
  }

  // Returns the button used when not all the items on the bookmark bar fit.
  views::MenuButton* overflow_button() const { return overflow_button_; }

  const gfx::Animation& size_animation() { return size_animation_; }

  // Returns the active MenuItemView, or null if a menu isn't showing.
  const views::MenuItemView* GetMenu() const;
  views::MenuItemView* GetMenu() {
    return const_cast<views::MenuItemView*>(std::as_const(*this).GetMenu());
  }

  // Returns the context menu, or null if one isn't showing.
  views::MenuItemView* GetContextMenu();

  // Returns the drop MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetDropMenu();

  // Returns the tooltip text for the specified url and title. The returned
  // text is clipped to fit |max_tooltip_width|.
  //
  // Note that we adjust the direction of both the URL and the title based on
  // the locale so that pure LTR strings are displayed properly in RTL locales.
  static std::u16string CreateToolTipForURLAndTitle(
      int max_tooltip_width,
      const gfx::FontList& font_list,
      const GURL& url,
      const std::u16string& title);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void PaintChildren(const views::PaintInfo& paint_info) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;
  void OnThemeChanged() override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void AddedToWidget() override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // BookmarkMenuControllerObserver:
  void BookmarkMenuControllerDeleted(
      BookmarkMenuController* controller) override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;

  // views::DragController:
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& p) override;
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Calculate the available width for the saved tab group bar.
  // This is used in Tab Group v2 UI to allocate space for both saved tab groups
  // and bookmark buttons.
  static int GetAvailableWidthForSavedTabGroupsBar(
      int saved_tab_group_bar_width,
      int bookmark_buttons_width,
      int available_width);

 private:
  struct DropInfo;
  struct DropLocation;

  friend class BookmarkBarViewTestHelper;
  friend class BookmarkBarViewEventTestBase;

  // Used to identify what the user is dropping onto.
  enum DropButtonType {
    DROP_BOOKMARK,
    DROP_ALL_BOOKMARKS_FOLDER,
    DROP_OVERFLOW
  };

  // Creates recent bookmark button and when visible button as well as
  // calculating the preferred height.
  void Init();

  void AppsPageShortcutPressed(const ui::Event& event);
  void OnButtonPressed(const bookmarks::BookmarkNode* node,
                       const ui::Event& event);
  void OnMenuButtonPressed(const bookmarks::BookmarkNode* node,
                           const ui::Event& event);

  // NOTE: unless otherwise stated all methods that take an index are in terms
  // of the bookmark bar view. Typically the view index and model index are the
  // same, but they may differ during animations or drag and drop.
  //
  // It's easy to get the mapping wrong. For this reason all these methods are
  // private.

  // Returns the index of the first hidden bookmark button. If all buttons are
  // visible, this returns GetBookmarkButtonCount().
  size_t GetFirstHiddenNodeIndex() const;

  // Creates the button showing the "All Bookmarks" folder.
  std::unique_ptr<views::MenuButton> CreateAllBookmarksButton();

  // Creates the button showing the "Managed Bookmarks" folder.
  std::unique_ptr<views::MenuButton> CreateManagedBookmarksButton();

  // Creates the button used when not all bookmark buttons fit.
  std::unique_ptr<views::MenuButton> CreateOverflowButton();

  // Creates the button for rendering the specified bookmark node.
  std::unique_ptr<views::View> CreateBookmarkButton(
      const bookmarks::BookmarkNode* node);

  // Creates the button for rendering the apps page shortcut.
  std::unique_ptr<views::LabelButton> CreateAppsPageShortcutButton();

  // Configures the button from the specified node. This sets the text,
  // and icon.
  void ConfigureButton(const bookmarks::BookmarkNode* node,
                       views::LabelButton* button);

  // Implementation for BookmarkNodeAddedImpl. Returns true if LayoutAndPaint()
  // is required.
  bool BookmarkNodeAddedImpl(const bookmarks::BookmarkNode* parent,
                             size_t index);

  // Implementation for BookmarkNodeRemoved. Returns true if LayoutAndPaint() is
  // required.
  bool BookmarkNodeRemovedImpl(const bookmarks::BookmarkNode* parent,
                               size_t index);

  // If the node is a child of the root node, the button is updated
  // appropriately.
  void BookmarkNodeChangedImpl(const bookmarks::BookmarkNode* node);

  // Shows the menu used during drag and drop for the specified node.
  void ShowDropFolderForNode(const bookmarks::BookmarkNode* node);

  // Cancels the timer used to show a drop menu.
  void StopShowFolderDropMenuTimer();

  // Stars the timer used to show a drop menu for node.
  void StartShowFolderDropMenuTimer(const bookmarks::BookmarkNode* node);

  // Calculates the location for the drop in |location|.
  void CalculateDropLocation(const ui::DropTargetEvent& event,
                             const bookmarks::BookmarkNodeData& data,
                             DropLocation* location);

  // Marks the current drop as invalid and cancels the menu. Used when the
  // model is mutated and a drop is in progress.
  void InvalidateDrop();

  // Returns the node corresponding to |sender|, which is one of the
  // |bookmark_buttons_|.
  const bookmarks::BookmarkNode* GetNodeForSender(views::View* sender) const;

  // Writes a BookmarkNodeData for node to data.
  void WriteBookmarkDragData(const bookmarks::BookmarkNode* node,
                             ui::OSExchangeData* data);

  // Sets/updates the colors and icons for all the child objects in the
  // bookmarks bar.
  void UpdateAppearanceForTheme();

  // Updates the visibility of |other_bookmarks_button_| and
  // |managed_bookmarks_button_|. Also shows or hides the separator if required.
  // Returns true if something changed and a LayoutAndPaint() is needed.
  bool UpdateOtherAndManagedButtonsVisibility();

  // Updates the visibility of |bookmarks_separator_view_|.
  void UpdateBookmarksSeparatorVisibility();

  // Updates the visibility of the apps shortcut based on the pref value.
  void OnAppsPageShortcutVisibilityPrefChanged();

  // Updates the visibility of the tab groups based on the pref value.
  void OnTabGroupsVisibilityPrefChanged();

  void OnShowManagedBookmarksPrefChanged();

  // Updates the look and feel of the bookmarks bar based on the pref value.
  void OnCompactModeChanged();

  void LayoutAndPaint() {
    InvalidateLayout();
    SchedulePaint();
  }

  // Inserts |bookmark_button| in logical position |index| in the bar,
  // maintaining correct focus traversal order.
  void InsertBookmarkButtonAtIndex(std::unique_ptr<views::View> bookmark_button,
                                   size_t index);

  // Returns the model index for the bookmark associated with |button|,
  // or size_t{-1} if |button| is not a bookmark button from this bar.
  size_t GetIndexForButton(views::View* button);

  // Returns the target drop BookmarkNode parent pointer and updates `index`
  // with the right value.
  const bookmarks::BookmarkNode* GetParentNodeAndIndexForDrop(size_t& index);

  // Drops Bookmark `data` and updates `output_drag_op` accordingly.
  void PerformDrop(const bookmarks::BookmarkNodeData data,
                   const bookmarks::BookmarkNode* parent_node,
                   const size_t index,
                   const bool copy,
                   const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  int GetDropLocationModelIndexForTesting() const;
  const views::View* GetSavedTabGroupsSeparatorViewForTesting() const;

  void MaybeShowSavedTabGroupsIntroPromo() const;

  // Needed to react to bookmark bar pref changes.
  PrefChangeRegistrar profile_pref_registrar_;

  // When true denotes if the bookmarks bar view should use the compact mode
  // layout. Otherwise, layout normally.
  bool is_compact_mode_ = false;

  // Used for opening urls.
  raw_ptr<content::PageNavigator, AcrossTasksDanglingUntriaged>
      page_navigator_ = nullptr;

  // BookmarkModel that owns the entries and folders that are shown in this
  // view. This is owned by the Profile.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;

  // ManagedBookmarkService. This is owned by the Profile.
  raw_ptr<bookmarks::ManagedBookmarkService> managed_ = nullptr;

  // Used to manage showing a Menu, either for the most recently bookmarked
  // entries, or for the starred folder.
  raw_ptr<BookmarkMenuController> bookmark_menu_ = nullptr;

  // Used when showing a menu for drag and drop. That is, if the user drags
  // over a folder this becomes non-null and manages the menu showing the
  // contents of the node.
  raw_ptr<BookmarkMenuController> bookmark_drop_menu_ = nullptr;

  // If non-NULL we're showing a context menu for one of the items on the
  // bookmark bar.
  std::unique_ptr<BookmarkContextMenu> context_menu_;

  // Saved Tab Group section
  raw_ptr<tab_groups::SavedTabGroupBar> saved_tab_group_bar_ = nullptr;

  // Shows the "Other Bookmarks" folder button.
  raw_ptr<views::MenuButton> all_bookmarks_button_ = nullptr;

  // Shows the managed bookmarks entries.
  raw_ptr<views::MenuButton> managed_bookmarks_button_ = nullptr;

  // Shows the Apps page shortcut.
  raw_ptr<views::LabelButton> apps_page_shortcut_ = nullptr;

  // Used to track drops on the bookmark bar view.
  std::unique_ptr<DropInfo> drop_info_;

  // Visible if not all the bookmark buttons fit.
  raw_ptr<views::MenuButton> overflow_button_ = nullptr;

  // The individual bookmark buttons.
  std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
      bookmark_buttons_;

  raw_ptr<ButtonSeparatorView> bookmarks_separator_view_ = nullptr;
  raw_ptr<ButtonSeparatorView> saved_tab_groups_separator_view_ = nullptr;

  const raw_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_;

  // True if the owning browser is showing an infobar.
  bool infobar_visible_ = false;

  // Animation controlling showing and hiding of the bar.
  gfx::SlideAnimation size_animation_{this};

  BookmarkBar::State bookmark_bar_state_ = BookmarkBar::SHOW;

  base::ObserverList<BookmarkBarViewObserver>::Unchecked observers_;

  // Factory used to delay showing of the drop menu.
  base::WeakPtrFactory<BookmarkBarView> show_folder_method_factory_{this};

  // Returns WeakPtrs used in GetPageNavigatorGetter(). Used to ensure
  // safety if BookmarkBarView is deleted after getting the callback.
  base::WeakPtrFactory<BookmarkBarView> weak_ptr_factory_{this};

  // Returns WeakPtrs used in GetDropCallback(). Used to ensure
  // safety if `model_` is mutated after getting the callback.
  base::WeakPtrFactory<BookmarkBarView> drop_weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
