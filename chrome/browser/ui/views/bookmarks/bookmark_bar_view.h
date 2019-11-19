// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_

#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/drag_controller.h"

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

namespace views {
class Button;
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
                        public BookmarkMenuControllerObserver,
                        public bookmarks::BookmarkBubbleObserver {
 public:
  // TODO(pbos): Get rid of these proxy classes by unifying a single
  // ButtonPressed to handle all buttons. This class only exists to forward
  // events into ::OnButtonPressed.
  class ButtonListener : public views::ButtonListener {
   public:
    explicit ButtonListener(BookmarkBarView* parent);
    void ButtonPressed(views::Button* source, const ui::Event& event) override;

   private:
    BookmarkBarView* const parent_;
  };

  // TODO(pbos): Get rid of these proxy classes by unifying a single
  // ButtonPressed to handle all buttons. This class only exists to forward
  // events into ::OnMenuButtonPressed.
  class MenuButtonListener : public views::ButtonListener {
   public:
    explicit MenuButtonListener(BookmarkBarView* parent);
    void ButtonPressed(views::Button* source, const ui::Event& event) override;

   private:
    BookmarkBarView* const parent_;
  };

  // The internal view class name.
  static const char kViewClassName[];

  // |browser_view| can be NULL during tests.
  BookmarkBarView(Browser* browser, BrowserView* browser_view);
  ~BookmarkBarView() override;

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

  // Returns the button responsible for showing bookmarks in the
  // "Other Bookmarks" folder.
  views::MenuButton* other_bookmarks_button() const {
    return other_bookmarks_button_;
  }

  // Returns the button used when not all the items on the bookmark bar fit.
  views::MenuButton* overflow_button() const { return overflow_button_; }

  const gfx::Animation& size_animation() { return size_animation_; }

  // Returns the active MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetMenu();

  // Returns the context menu, or null if one isn't showing.
  views::MenuItemView* GetContextMenu();

  // Returns the drop MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetDropMenu();

  // If a button is currently throbbing, it is stopped. If immediate is true
  // the throb stops immediately, otherwise it stops after a couple more
  // throbs.
  void StopThrobbing(bool immediate);

  // Returns the tooltip text for the specified url and title. The returned
  // text is clipped to fit |max_tooltip_width|.
  //
  // Note that we adjust the direction of both the URL and the title based on
  // the locale so that pure LTR strings are displayed properly in RTL locales.
  static base::string16 CreateToolTipForURLAndTitle(
      int max_tooltip_width,
      const gfx::FontList& font_list,
      const GURL& url,
      const base::string16& title);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
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
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void OnThemeChanged() override;
  const char* GetClassName() const override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // AccessiblePaneView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // BookmarkMenuControllerObserver:
  void BookmarkMenuControllerDeleted(
      BookmarkMenuController* controller) override;

  // bookmarks::BookmarkBubbleObserver:
  void OnBookmarkBubbleShown(const bookmarks::BookmarkNode* node) override;
  void OnBookmarkBubbleHidden() override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;

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

 private:
  class ButtonSeparatorView;
  struct DropInfo;
  struct DropLocation;

  friend class BookmarkBarViewTestHelper;
  friend class BookmarkBarViewEventTestBase;

  // Used to identify what the user is dropping onto.
  enum DropButtonType { DROP_BOOKMARK, DROP_OTHER_FOLDER, DROP_OVERFLOW };

  // Creates recent bookmark button and when visible button as well as
  // calculating the preferred height.
  void Init();

  void OnButtonPressed(views::Button* sender, const ui::Event& event);
  void OnMenuButtonPressed(views::Button* sender, const ui::Event& event);

  // NOTE: unless otherwise stated all methods that take an index are in terms
  // of the bookmark bar view. Typically the view index and model index are the
  // same, but they may differ during animations or drag and drop.
  //
  // It's easy to get the mapping wrong. For this reason all these methods are
  // private.

  // Returns the index of the first hidden bookmark button. If all buttons are
  // visible, this returns GetBookmarkButtonCount().
  size_t GetFirstHiddenNodeIndex();

  // Creates the button showing the "Other Bookmarks" folder.
  views::MenuButton* CreateOtherBookmarksButton();

  // Creates the button showing the "Managed Bookmarks" folder.
  views::MenuButton* CreateManagedBookmarksButton();

  // Creates the button used when not all bookmark buttons fit.
  views::MenuButton* CreateOverflowButton();

  // Creates the button for rendering the specified bookmark node.
  views::View* CreateBookmarkButton(const bookmarks::BookmarkNode* node);

  // Creates the button for rendering the apps page shortcut.
  views::LabelButton* CreateAppsPageShortcutButton();

  // Configures the button from the specified node. This sets the text,
  // and icon.
  void ConfigureButton(const bookmarks::BookmarkNode* node,
                       views::LabelButton* button);

  // Implementation for BookmarkNodeAddedImpl. Returns true if LayoutAndPaint()
  // is required.
  bool BookmarkNodeAddedImpl(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             size_t index);

  // Implementation for BookmarkNodeRemoved. Returns true if LayoutAndPaint() is
  // required.
  bool BookmarkNodeRemovedImpl(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* parent,
                               size_t index);

  // If the node is a child of the root node, the button is updated
  // appropriately.
  void BookmarkNodeChangedImpl(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node);

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

  // Returns the node corresponding to |sender|, which is one of the
  // |bookmark_buttons_|.
  const bookmarks::BookmarkNode* GetNodeForSender(View* sender) const;

  // Writes a BookmarkNodeData for node to data.
  void WriteBookmarkDragData(const bookmarks::BookmarkNode* node,
                             ui::OSExchangeData* data);

  // This determines which view should throb and starts it
  // throbbing (e.g when the bookmark bubble is showing).
  // If |overflow_only| is true, start throbbing only if |node| is hidden in
  // the overflow menu.
  void StartThrobbing(const bookmarks::BookmarkNode* node, bool overflow_only);

  // Returns the view to throb when a node is removed. |parent| is the parent of
  // the node that was removed, and |old_index| the index of the node that was
  // removed.
  views::Button* DetermineViewToThrobFromRemove(
      const bookmarks::BookmarkNode* parent,
      size_t old_index);

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

  void OnShowManagedBookmarksPrefChanged();

  void LayoutAndPaint() {
    Layout();
    SchedulePaint();
  }

  // Inserts |button| in logical position |index| in the bar, maintaining
  // correct focus traversal order.
  void InsertBookmarkButtonAtIndex(views::View* button, size_t index);

  // Returns the model index for the bookmark associated with |button|,
  // or size_t{-1} if |button| is not a bookmark button from this bar.
  size_t GetIndexForButton(views::View* button);

  // These forward button callbacks into ::On{Menu}ButtonPressed.
  ButtonListener button_listener_;
  MenuButtonListener menu_button_listener_;

  // Needed to react to kShowAppsShortcutInBookmarkBar changes.
  PrefChangeRegistrar profile_pref_registrar_;

  // Used for opening urls.
  content::PageNavigator* page_navigator_;

  // BookmarkModel that owns the entries and folders that are shown in this
  // view. This is owned by the Profile.
  bookmarks::BookmarkModel* model_;

  // ManagedBookmarkService. This is owned by the Profile.
  bookmarks::ManagedBookmarkService* managed_;

  // Used to manage showing a Menu, either for the most recently bookmarked
  // entries, or for the starred folder.
  BookmarkMenuController* bookmark_menu_;

  // Used when showing a menu for drag and drop. That is, if the user drags
  // over a folder this becomes non-null and manages the menu showing the
  // contents of the node.
  BookmarkMenuController* bookmark_drop_menu_;

  // If non-NULL we're showing a context menu for one of the items on the
  // bookmark bar.
  std::unique_ptr<BookmarkContextMenu> context_menu_;

  // Shows the "Other Bookmarks" folder button.
  views::MenuButton* other_bookmarks_button_;

  // Shows the managed bookmarks entries.
  views::MenuButton* managed_bookmarks_button_;

  // Shows the Apps page shortcut.
  views::LabelButton* apps_page_shortcut_;

  // Used to track drops on the bookmark bar view.
  std::unique_ptr<DropInfo> drop_info_;

  // Visible if not all the bookmark buttons fit.
  views::MenuButton* overflow_button_;

  // The individual bookmark buttons.
  std::vector<views::LabelButton*> bookmark_buttons_;

  ButtonSeparatorView* bookmarks_separator_view_;

  Browser* const browser_;
  BrowserView* browser_view_;

  // True if the owning browser is showing an infobar.
  bool infobar_visible_;

  // Animation controlling showing and hiding of the bar.
  gfx::SlideAnimation size_animation_;

  // If the bookmark bubble is showing, this is the visible ancestor of the URL.
  // The visible ancestor is either the |other_bookmarks_button_|,
  // |overflow_button_| or a button on the bar.
  views::Button* throbbing_view_;

  BookmarkBar::State bookmark_bar_state_;

  base::ObserverList<BookmarkBarViewObserver>::Unchecked observers_;

  // Factory used to delay showing of the drop menu.
  base::WeakPtrFactory<BookmarkBarView> show_folder_method_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
