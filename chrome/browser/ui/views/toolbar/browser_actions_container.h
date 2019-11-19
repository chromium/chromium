// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/drag_controller.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class BubbleDialogDelegateView;
class ResizeArea;
class Separator;
}

// The BrowserActionsContainer is a container view, responsible for drawing the
// toolbar action icons (including extension icons and icons for component
// toolbar actions). It comes in two flavors, a main container (when residing on
// the toolbar) and an overflow container (that resides in the main application
// menu, aka the Chrome menu).
//
// When in 'main' mode, the container supports the full functionality of a
// BrowserActionContainer, but in 'overflow' mode the container is effectively
// just an overflow for the 'main' toolbar (shows only the icons that the main
// toolbar does not) and as such does not have an overflow itself. The overflow
// container also does not support resizing. Since the main container only shows
// icons in the Chrome toolbar, it is limited to a single row of icons. The
// overflow container, however, is allowed to display icons in multiple rows.
//
// The main container is placed flush against the omnibox and hot dog menu,
// whereas the overflow container is placed within the hot dog menu. The
// layout is similar to this:
//   rI_I_Is
// Where the letters are as follows:
//   r: An invisible resize area.  This is
//      GetLayoutConstant(TOOLBAR_STANDARD_SPACING) pixels wide and directly
//      adjacent to the omnibox. Only shown for the main container.
//   I: An icon. This has a width of 28.
//   _: ToolbarActionsBar::PlatformSettings::item_spacing pixels of empty space.
//   s: GetLayoutConstant(TOOLBAR_STANDARD_SPACING) pixels of empty space
//      (before the app menu).
// The reason the container contains the trailing space "s", rather than having
// it be handled by the parent view, is so that when the user starts
// dragging an icon around, we have the space to draw the ultimate drop
// indicator.  (Otherwise, we'd be trying to draw it into the padding beyond our
// right edge, and it wouldn't appear.)
//
// The BrowserActionsContainer in 'main' mode follows a few rules, in terms of
// user experience:
//
// 1) The container can never grow beyond the space needed to show all icons
// (hereby referred to as the max width).
// 2) The container can never shrink below the space needed to show just the
// initial padding (ignoring the case where there are no icons to show, in which
// case the container won't be visible anyway).
// 3) The container snaps into place (to the pixel count that fits the visible
// icons) to make sure there is no wasted space at the edges of the container.
// 4) If the user adds or removes icons (read: installs/uninstalls browser
// actions) we grow and shrink the container as needed - but ONLY if the
// container was at max width to begin with.
// 5) If the container is NOT at max width (has an overflow menu), we respect
// that size when adding and removing icons and DON'T grow/shrink the container.
// This means that new icons (which always appear at the far right) will show up
// in the overflow.
//
// Resizing the BrowserActionsContainer:
//
// The ResizeArea view sends OnResize messages to the BrowserActionsContainer
// class as the user drags it. This modifies the value for |resize_amount_|.
// That indicates to the container that a resize is in progress and is used to
// calculate the size in GetPreferredSize(), though that function never exceeds
// the defined minimum and maximum size of the container.
//
// When the user releases the mouse (ends the resize), we calculate a target
// size for the container (animation_target_size_), clamp that value to the
// containers min and max and then animate from the *current* position (that the
// user has dragged the view to) to the target size.
//
// Animating the BrowserActionsContainer:
//
// Animations are used when snapping the container to a value that fits all
// visible icons. This can be triggered when the user finishes resizing the
// container or when Browser Actions are added/removed.
//
// We always animate from the current width (container_width_) to the target
// size (animation_target_size_), using |resize_amount| to keep track of the
// animation progress.
//
////////////////////////////////////////////////////////////////////////////////
class BrowserActionsContainer : public views::View,
                                public ToolbarActionsBarDelegate,
                                public views::ResizeAreaDelegate,
                                public views::AnimationDelegateViews,
                                public ToolbarActionView::Delegate,
                                public views::WidgetObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns the view of the toolbar actions overflow menu to use as a
    // reference point for a popup when this view isn't visible.
    virtual views::LabelButton* GetOverflowReferenceView() = 0;

    // Returns the maximum width the browser actions container can have. An
    // empty value means there is no maximum.
    virtual base::Optional<int> GetMaxBrowserActionsWidth() const = 0;

    // Whether the container supports showing extensions outside of the menu.
    virtual bool CanShowIconInToolbar() const;

    // Creates a ToolbarActionsBar for the BrowserActionsContainer to use.
    virtual std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
        ToolbarActionsBarDelegate* delegate,
        Browser* browser,
        ToolbarActionsBar* main_bar) const = 0;
  };

  // Constructs a BrowserActionContainer for a particular |browser| object. For
  // documentation of |main_container|, see class comments.
  //
  // |interactive| determines whether the bar can be dragged to resize it or do
  // drag and drop.
  BrowserActionsContainer(Browser* browser,
                          BrowserActionsContainer* main_container,
                          Delegate* delegate,
                          bool interactive = true);
  BrowserActionsContainer(const BrowserActionsContainer&) = delete;
  BrowserActionsContainer& operator=(const BrowserActionsContainer&) = delete;
  ~BrowserActionsContainer() override;

  // Get the number of toolbar actions being displayed.
  size_t num_toolbar_actions() const { return toolbar_action_views_.size(); }

  // Returns the browser this container is associated with.
  Browser* browser() const { return browser_; }

  Delegate* delegate() { return delegate_; }

  ToolbarActionsBar* toolbar_actions_bar() {
    return toolbar_actions_bar_.get();
  }

  // Get a particular toolbar action view.
  ToolbarActionView* GetToolbarActionViewAt(int index) {
    return toolbar_action_views_[index].get();
  }

  // Whether we are performing resize animation on the container.
  bool animating() const {
    return resize_animation_ && resize_animation_->is_animating();
  }

  // Is the view being resized?
  bool resizing() const { return resize_starting_width_.has_value(); }

  // Returns the ID of the action represented by the view at |index|.
  std::string GetIdAt(size_t index) const;

  // Returns the ToolbarActionView* associated with the given |extension|, or
  // nullptr if none exists.
  ToolbarActionView* GetViewForId(const std::string& id);

  // Update the views to reflect the state of the toolbar actions.
  void RefreshToolbarActionViews();

  // Returns how many actions are currently visible. If the intent is to find
  // how many are visible once the container finishes animation, see
  // VisibleBrowserActionsAfterAnimation() below.
  size_t VisibleBrowserActions() const;

  // Returns how many actions will be visible once the container finishes
  // animating to a new size, or (if not animating) the currently visible icons.
  size_t VisibleBrowserActionsAfterAnimation() const;

  // Sets the color for the separator if present, called after construction and
  // on theme changes.
  void SetSeparatorColor(SkColor color);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // Overridden from views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Overridden from views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Overridden from ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override;
  bool ShownInsideMenu() const override;
  bool CanShowIconInToolbar() const override;
  void OnToolbarActionViewDragDone() override;
  views::LabelButton* GetOverflowReferenceView() const override;
  gfx::Size GetToolbarActionSize() override;

  // ToolbarActionsBarDelegate:
  void AddViewForAction(ToolbarActionViewController* action,
                        size_t index) override;
  void RemoveViewForAction(ToolbarActionViewController* action) override;
  void RemoveAllViews() override;
  void Redraw(bool order_changed) override;
  void ResizeAndAnimate(gfx::Tween::Type tween_type,
                        int target_width) override;
  int GetWidth(GetWidthTime get_width_time) const override;
  bool IsAnimating() const override;
  void StopAnimating() override;
  void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) override;
  bool CloseOverflowMenuIfOpen() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  views::BubbleDialogDelegateView* active_bubble() { return active_bubble_; }

  static views::FlexRule GetFlexRule();

 protected:
  // Overridden from views::View:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  friend class BrowserActionsContainerBrowserTest;

  // A struct representing the position at which an action will be dropped.
  struct DropPosition;

  // Clears the |active_bubble_|, and unregisters the container as an observer.
  void ClearActiveBubble(views::Widget* widget);

  // Utility functions for going from/to width and icon counts.
  size_t WidthToIconCount(int width) const;
  int GetWidthForIconCount(size_t num_icons) const;
  int GetWidthWithAllActionsVisible() const;

  // Get index of the drag-drop position.
  size_t GetDropPositionIndex() const;

  // Returns the preferred width given the limit of |max_width|. (Unlike most
  // views, since we don't want to show part of an icon or a large space after
  // the omnibox, this is probably *not* |max_width|).
  int GetWidthForMaxWidth(int max_width) const;

  // Width allocated for the resize handle, |resize_area_|. 0 when it should not
  // be shown.
  int GetResizeAreaWidth() const;

  // Width of the separator and surrounding padding. 0 when the separator should
  // not be shown.
  int GetSeparatorAreaWidth() const;

  // Updates the enabled state of the resize area based on whether a resize can
  // happen with the current browser size and actions bar state.
  void UpdateResizeArea();

  const ToolbarActionsBar::PlatformSettings& platform_settings() const {
    return toolbar_actions_bar_->platform_settings();
  }

  Delegate* const delegate_ = nullptr;

  // The controlling ToolbarActionsBar, which handles most non-view logic.
  std::unique_ptr<ToolbarActionsBar> toolbar_actions_bar_;

  // Child toolbar action buttons.
  std::vector<std::unique_ptr<ToolbarActionView>> toolbar_action_views_;

  // The Browser object the container is associated with.
  Browser* const browser_;

  // The main container we are serving as overflow for, or NULL if this
  // class is the the main container. See class comments for details on
  // the difference between main and overflow.
  BrowserActionsContainer* main_container_;

  // The resize area for the container.
  views::ResizeArea* resize_area_ = nullptr;

  // Separator at the end of browser actions to highlight that these actions are
  // different from built-in toolbar actions.
  views::Separator* separator_ = nullptr;

  // The animation that happens when the container snaps to place.
  std::unique_ptr<gfx::SlideAnimation> resize_animation_;

  // True if the container has been added to the parent view.
  bool added_to_view_ = false;

  // When the container is resizing, this is the width at which it started.
  base::Optional<int> resize_starting_width_;

  // This is used while the user is resizing (and when the animations are in
  // progress) to know how wide the delta is between the current state and what
  // we should draw.
  int resize_amount_ = 0;

  // Keeps track of the absolute pixel width the container should have when we
  // are done animating.
  int animation_target_size_ = 0;

  // The DropPosition for the current drag-and-drop operation, or NULL if there
  // is none.
  std::unique_ptr<DropPosition> drop_position_;

  // Whether the container can be interacted with (e.g drag/drop, resize).
  const bool interactive_ = true;

  // The extension bubble that is actively showing, if any.
  views::BubbleDialogDelegateView* active_bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_
