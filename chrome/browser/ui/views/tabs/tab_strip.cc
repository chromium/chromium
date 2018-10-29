// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_style.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/compositing_recorder.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

using MD = ui::MaterialDesignController;

namespace {

// Max number of stacked tabs.
constexpr int kMaxStackedCount = 4;

// Padding between stacked tabs.
constexpr int kStackedPadding = 6;

// Size of the drop indicator.
int g_drop_indicator_width = 0;
int g_drop_indicator_height = 0;

// Animation delegate used for any automatic tab movement.  Hides the tab if it
// is not fully visible within the tabstrip area, to prevent overflow clipping.
class TabAnimationDelegate : public gfx::AnimationDelegate {
 public:
  TabAnimationDelegate(TabStrip* tab_strip, Tab* tab);
  ~TabAnimationDelegate() override;

  void AnimationProgressed(const gfx::Animation* animation) override;

 protected:
  TabStrip* tab_strip() { return tab_strip_; }
  Tab* tab() { return tab_; }

 private:
  TabStrip* const tab_strip_;
  Tab* const tab_;

  DISALLOW_COPY_AND_ASSIGN(TabAnimationDelegate);
};

TabAnimationDelegate::TabAnimationDelegate(TabStrip* tab_strip, Tab* tab)
    : tab_strip_(tab_strip), tab_(tab) {}

TabAnimationDelegate::~TabAnimationDelegate() {}

void TabAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  tab_->SetVisible(tab_strip_->ShouldTabBeVisible(tab_));
}

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate : public TabAnimationDelegate {
 public:
  ResetDraggingStateDelegate(TabStrip* tab_strip, Tab* tab);
  ~ResetDraggingStateDelegate() override;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResetDraggingStateDelegate);
};

ResetDraggingStateDelegate::ResetDraggingStateDelegate(TabStrip* tab_strip,
                                                       Tab* tab)
    : TabAnimationDelegate(tab_strip, tab) {}

ResetDraggingStateDelegate::~ResetDraggingStateDelegate() {}

void ResetDraggingStateDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  tab()->set_dragging(false);
  AnimationProgressed(animation);  // Forces tab visibility to update.
}

void ResetDraggingStateDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

// If |dest| contains the point |point_in_source| the event handler from |dest|
// is returned. Otherwise returns null.
views::View* ConvertPointToViewAndGetEventHandler(
    views::View* source,
    views::View* dest,
    const gfx::Point& point_in_source) {
  gfx::Point dest_point(point_in_source);
  views::View::ConvertPointToTarget(source, dest, &dest_point);
  return dest->HitTestPoint(dest_point)
             ? dest->GetEventHandlerForPoint(dest_point)
             : nullptr;
}

// Gets a tooltip handler for |point_in_source| from |dest|. Note that |dest|
// should return null if it does not contain the point.
views::View* ConvertPointToViewAndGetTooltipHandler(
    views::View* source,
    views::View* dest,
    const gfx::Point& point_in_source) {
  gfx::Point dest_point(point_in_source);
  views::View::ConvertPointToTarget(source, dest, &dest_point);
  return dest->GetTooltipHandlerForPoint(dest_point);
}

TabDragController::EventSource EventSourceFromEvent(
    const ui::LocatedEvent& event) {
  return event.IsGestureEvent() ? TabDragController::EVENT_SOURCE_TOUCH
                                : TabDragController::EVENT_SOURCE_MOUSE;
}

const TabSizeInfo& GetTabSizeInfo() {
  static TabSizeInfo tab_size_info, touch_tab_size_info;
  TabSizeInfo* info = MD::touch_ui() ? &touch_tab_size_info : &tab_size_info;
  if (info->standard_size.IsEmpty()) {
    info->pinned_tab_width = TabStyle::GetPinnedWidth();
    info->min_active_width = TabStyle::GetMinimumActiveWidth();
    info->min_inactive_width = TabStyle::GetMinimumInactiveWidth();
    info->standard_size =
        gfx::Size(TabStyle::GetStandardWidth(), GetLayoutConstant(TAB_HEIGHT));
    info->tab_overlap = TabStyle::GetTabOverlap();
  }
  return *info;
}

int GetStackableTabWidth() {
  return TabStyle::GetTabOverlap() + (MD::touch_ui() ? 136 : 102);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabStrip::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabStrip::RemoveTabDelegate : public TabAnimationDelegate {
 public:
  RemoveTabDelegate(TabStrip* tab_strip, Tab* tab);

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveTabDelegate);
};

TabStrip::RemoveTabDelegate::RemoveTabDelegate(TabStrip* tab_strip, Tab* tab)
    : TabAnimationDelegate(tab_strip, tab) {}

void TabStrip::RemoveTabDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK(tab()->closing());
  tab_strip()->RemoveAndDeleteTab(tab());

  // Send the Container a message to simulate a mouse moved event at the current
  // mouse position. This tickles the Tab the mouse is currently over to show
  // the "hot" state of the close button.  Note that this is not required (and
  // indeed may crash!) for removes spawned by non-mouse closes and
  // drag-detaches.
  if (!tab_strip()->IsDragSessionActive() &&
      tab_strip()->ShouldHighlightCloseButtonAfterRemove()) {
    // The widget can apparently be null during shutdown.
    views::Widget* widget = tab_strip()->GetWidget();
    if (widget)
      widget->SynthesizeMouseMoveEvent();
  }
}

void TabStrip::RemoveTabDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, public:

TabStrip::TabStrip(std::unique_ptr<TabStripController> controller)
    : controller_(std::move(controller)),
      current_inactive_width_(TabStyle::GetStandardWidth()),
      current_active_width_(TabStyle::GetStandardWidth()) {
  Init();
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  md_observer_.Add(MD::GetInstance());
}

TabStrip::~TabStrip() {
  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  DestroyDragController();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  // The children (tabs) may callback to us from their destructor. Delete them
  // so that if they call back we aren't in a weird state.
  RemoveAllChildViews(true);
}

void TabStrip::AddObserver(TabStripObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStrip::RemoveObserver(TabStripObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabStrip::FrameColorsChanged() {
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->FrameColorsChanged();
  new_tab_button_->FrameColorsChanged();
  UpdateContrastRatioValues();
  SchedulePaint();
}

int TabStrip::GetTabsMaxX() const {
  // There might be no tabs yet during startup.
  return tab_count() ? ideal_bounds(tab_count() - 1).right() : 0;
}

void TabStrip::SetBackgroundOffset(int offset) {
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->set_background_offset(offset);
  new_tab_button_->set_background_offset(offset);
}

bool TabStrip::IsRectInWindowCaption(const gfx::Rect& rect) {
  // If there is no control at this location, the hit is in the caption area.
  const views::View* v = GetEventHandlerForRect(rect);
  if (v == this)
    return true;

  // When the window has a top drag handle, a thin strip at the top of inactive
  // tabs and the new tab button is treated as part of the window drag handle,
  // to increase draggability.  This region starts 1 DIP above the top of the
  // separator.
  const int drag_handle_extension = TabStyle::GetDragHandleExtension(height());

  // Disable drag handle extension when tab shapes are visible.
  bool extend_drag_handle = !controller_->IsFrameCondensed() &&
                            !controller_->EverHasVisibleBackgroundTabShapes();

  // A hit on the tab is not in the caption unless it is in the thin strip
  // mentioned above.
  const int tab_index = tabs_.GetIndexOfView(v);
  if (IsValidModelIndex(tab_index)) {
    Tab* tab = tab_at(tab_index);
    gfx::Rect tab_drag_handle = tab->GetMirroredBounds();
    tab_drag_handle.set_height(drag_handle_extension);
    return extend_drag_handle && !tab->IsActive() &&
           tab_drag_handle.Intersects(rect);
  }

  // Similarly, a hit in the new tab button is considered to be in the caption
  // if it's in this thin strip.
  gfx::Rect new_tab_button_drag_handle = new_tab_button_->GetMirroredBounds();
  new_tab_button_drag_handle.set_height(drag_handle_extension);
  if (extend_drag_handle && new_tab_button_drag_handle.Intersects(rect))
    return true;

  // Check to see if the rect intersects the non-button parts of the new tab
  // button. The button has a non-rectangular shape, so if it's not in the
  // visual portions of the button we treat it as a click to the caption.
  gfx::RectF rect_in_new_tab_coords_f(rect);
  View::ConvertRectToTarget(this, new_tab_button_, &rect_in_new_tab_coords_f);
  gfx::Rect rect_in_new_tab_coords =
      gfx::ToEnclosingRect(rect_in_new_tab_coords_f);
  return new_tab_button_->GetLocalBounds().Intersects(rect_in_new_tab_coords) &&
         !new_tab_button_->HitTestRect(rect_in_new_tab_coords);
}

bool TabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

bool TabStrip::IsTabStripCloseable() const {
  return !IsDragSessionActive();
}

bool TabStrip::IsTabStripEditable() const {
  return !IsDragSessionActive() && !IsActiveDropTarget();
}

bool TabStrip::IsTabCrashed(int tab_index) const {
  return tab_at(tab_index)->data().IsCrashed();
}

bool TabStrip::TabHasNetworkError(int tab_index) const {
  return tab_at(tab_index)->data().network_state == TabNetworkState::kError;
}

TabAlertState TabStrip::GetTabAlertState(int tab_index) const {
  return tab_at(tab_index)->data().alert_state;
}

void TabStrip::UpdateLoadingAnimations() {
  for (int i = 0; i < tab_count(); i++)
    tab_at(i)->StepLoadingAnimation();
}

void TabStrip::SetStackedLayout(bool stacked_layout) {
  if (stacked_layout == stacked_layout_)
    return;

  stacked_layout_ = stacked_layout;
  SetResetToShrinkOnExit(false);
  SwapLayoutIfNecessary();

  // When transitioning to stacked try to keep the active tab from moving.
  const int active_index = controller_->GetActiveIndex();
  if (touch_layout_ && active_index != -1) {
    touch_layout_->SetActiveTabLocation(ideal_bounds(active_index).x());
    AnimateToIdealBounds();
  }

  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->Layout();
}

void TabStrip::StartHighlight(int model_index) {
  tab_at(model_index)->StartPulse();
}

void TabStrip::StopAllHighlighting() {
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->StopPulse();
}

void TabStrip::AddTabAt(int model_index, TabRendererData data, bool is_active) {
  const bool was_single_tab_mode = SingleTabMode();

  Tab* tab = new Tab(this, animation_container_.get());
  AddChildView(tab);
  const bool pinned = data.pinned;
  tab->SetData(std::move(data));
  UpdateTabsClosingMap(model_index, 1);
  tabs_.Add(tab, model_index);
  selected_tabs_.IncrementFrom(model_index);

  // If the new tab button is visually after the tabs, make sure it is logically
  // afterwards as well so that the focus traversal order is correct.
  NewTabButtonPosition position = controller_->GetNewTabButtonPosition();
  if (position == AFTER_TABS || position == TRAILING)
    ReorderChildView(new_tab_button_, -1);

  if (touch_layout_) {
    int add_types = 0;
    if (pinned)
      add_types |= StackedTabStripLayout::kAddTypePinned;
    if (is_active)
      add_types |= StackedTabStripLayout::kAddTypeActive;
    touch_layout_->AddTab(model_index, add_types,
                          GenerateIdealBoundsForPinnedTabs(nullptr));
  }

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (tab_count() > 1 && GetWidget() && GetWidget()->IsVisible())
    StartInsertTabAnimation(model_index);
  else
    DoLayout();

  SwapLayoutIfNecessary();

  if (was_single_tab_mode != SingleTabMode())
    SingleTabModeChanged();

  for (TabStripObserver& observer : observers_)
    observer.OnTabAdded(model_index);

  // Stop dragging when a new tab is added and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the add,
  // but this comes up infrequently enough that it's not worth the complexity.
  //
  // At the start of AddTabAt() the model and tabs are out sync. Any queries to
  // find a tab given a model index can go off the end of |tabs_|. As such, it
  // is important that we complete the drag *after* adding the tab so that the
  // model and tabstrip are in sync.
  if (drag_controller_.get() && !drag_controller_->is_mutating() &&
      drag_controller_->is_dragging_window()) {
    EndDrag(END_DRAG_COMPLETE);
  }
}

void TabStrip::MoveTab(int from_model_index,
                       int to_model_index,
                       TabRendererData data) {
  DCHECK_GT(tabs_.view_size(), 0);
  const Tab* last_tab = GetLastVisibleTab();
  tab_at(from_model_index)->SetData(std::move(data));
  if (touch_layout_) {
    tabs_.MoveViewOnly(from_model_index, to_model_index);
    int pinned_count = 0;
    const int start_x = GenerateIdealBoundsForPinnedTabs(&pinned_count);
    touch_layout_->MoveTab(from_model_index, to_model_index,
                           controller_->GetActiveIndex(), start_x,
                           pinned_count);
  } else {
    tabs_.Move(from_model_index, to_model_index);
  }
  selected_tabs_.Move(from_model_index, to_model_index, /*length=*/1);

  StartMoveTabAnimation();
  if (MayHideNewTabButtonWhileDragging() &&
      TabDragController::IsAttachedTo(this) &&
      (last_tab != GetLastVisibleTab() || last_tab->dragging())) {
    new_tab_button_->SetVisible(false);
  }
  SwapLayoutIfNecessary();

  for (TabStripObserver& observer : observers_)
    observer.OnTabMoved(from_model_index, to_model_index);
}

void TabStrip::RemoveTabAt(content::WebContents* contents,
                           int model_index,
                           bool was_active) {
  const int model_count = GetModelCount();
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (in_tab_close_ && model_count > 0 && model_index != model_count) {
    // The user closed a tab other than the last tab. Set
    // available_width_for_tabs_ so that as the user closes tabs with the mouse
    // a tab continues to fall under the mouse.
    int next_active_index = controller_->GetActiveIndex();
    DCHECK(IsValidModelIndex(next_active_index));
    if (model_index <= next_active_index) {
      // At this point, model's internal state has already been updated.
      // |contents| has been detached from model and the active index has been
      // updated. But the tab for |contents| isn't removed yet. Thus, we need to
      // fix up next_active_index based on it.
      next_active_index++;
    }
    Tab* next_active_tab = tab_at(next_active_index);
    Tab* tab_being_removed = tab_at(model_index);

    int size_delta = tab_being_removed->width();
    if (!tab_being_removed->data().pinned && was_active &&
        current_active_width_ > current_inactive_width_) {
      // When removing an active, non-pinned tab, an inactive tab will be made
      // active and thus given the active width. Thus the width being removed
      // from the strip is really the current width of whichever inactive tab
      // will be made active.
      size_delta = next_active_tab->width();
    }

    available_width_for_tabs_ = ideal_bounds(model_count).right() -
                                TabStartX() - size_delta + tab_overlap;
  }

  if (!touch_layout_)
    PrepareForAnimation();

  Tab* tab = tab_at(model_index);
  tab->SetClosing(true);

  int old_x = tabs_.ideal_bounds(model_index).x();
  RemoveTabFromViewModel(model_index);

  if (touch_layout_) {
    touch_layout_->RemoveTab(model_index,
                             GenerateIdealBoundsForPinnedTabs(nullptr), old_x);
  }

  GenerateIdealBounds();
  AnimateToIdealBounds();

  // TODO(pkasting): When closing multiple tabs, we get repeated RemoveTabAt()
  // calls, each of which closes a new tab and thus generates different ideal
  // bounds.  We should update the animations of any other tabs that are
  // currently being closed to reflect the new ideal bounds, or else change from
  // removing one tab at a time to animating the removal of all tabs at once.

  // Compute the target bounds for animating this tab closed.  The tab's left
  // edge should stay joined to the right edge of the previous tab, if any.
  gfx::Rect tab_bounds = tab->bounds();
  int desired_x = TabStartX();
  if (model_index > 0)
    desired_x = ideal_bounds(model_index - 1).right() - tab_overlap;
  tab_bounds.set_x(desired_x);

  // The tab should animate to the width of the overlap in order to close at the
  // same speed the surrounding tabs are moving, since at this width the
  // subsequent tab is naturally positioned at the same X coordinate.
  tab_bounds.set_width(tab_overlap);

  // Animate the tab closed.
  bounds_animator_.AnimateViewTo(tab, tab_bounds);
  bounds_animator_.SetAnimationDelegate(
      tab, std::make_unique<RemoveTabDelegate>(this, tab));

  // TODO(pkasting): The first part of this conditional doesn't really make
  // sense to me.  Why is each condition justified?
  if ((touch_layout_ || !in_tab_close_ || model_index == GetModelCount()) &&
      TabDragController::IsAttachedTo(this)) {
    // Don't animate the new tab button when dragging tabs. Otherwise it looks
    // like the new tab button magically appears from beyond the end of the tab
    // strip.
    bounds_animator_.StopAnimatingView(new_tab_button_);
    new_tab_button_->SetBoundsRect(new_tab_button_bounds_);
  }

  SwapLayoutIfNecessary();

  for (TabStripObserver& observer : observers_)
    observer.OnTabRemoved(model_index);

  if (SingleTabMode())
    SingleTabModeChanged();

  // Stop dragging when a new tab is removed and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the
  // remove operation, but this comes up infrequently enough that it's not worth
  // the complexity.
  //
  // At the start of RemoveTabAt() the model and tabs are out sync. Any queries
  // to find a tab given a model index can go off the end of |tabs_|. As such,
  // it is important that we complete the drag *after* removing the tab so that
  // the model and tabstrip are in sync.
  if (contents && drag_controller_.get() && !drag_controller_->is_mutating() &&
      drag_controller_->IsDraggingTab(contents)) {
    EndDrag(END_DRAG_COMPLETE);
  }
}

void TabStrip::SetTabData(int model_index, TabRendererData data) {
  const bool was_single_tab_mode = SingleTabMode();

  Tab* tab = tab_at(model_index);
  const bool pinned_state_changed = tab->data().pinned != data.pinned;
  tab->SetData(std::move(data));

  if (pinned_state_changed) {
    if (SingleTabMode() != was_single_tab_mode)
      SingleTabModeChanged();
    if (touch_layout_) {
      int pinned_tab_count = 0;
      int start_x = GenerateIdealBoundsForPinnedTabs(&pinned_tab_count);
      touch_layout_->SetXAndPinnedCount(start_x, pinned_tab_count);
    }
    if (GetWidget() && GetWidget()->IsVisible())
      StartPinnedTabAnimation();
    else
      DoLayout();
  }
  SwapLayoutIfNecessary();
}

bool TabStrip::ShouldTabBeVisible(const Tab* tab) const {
  // Detached tabs should always be invisible (as they close).
  if (tab->detached())
    return false;

  // When stacking tabs, all tabs should always be visible.
  if (stacked_layout_)
    return true;

  // If the tab is currently clipped by the trailing edge of the strip, it
  // shouldn't be visible.
  const int right_edge = tab->bounds().right();
  const int tabstrip_right =
      tab->dragging() ? TabDragAreaEndX() : (TabStartX() + GetTabAreaWidth());
  if (right_edge > tabstrip_right)
    return false;

  // Non-clipped dragging tabs should always be visible.
  if (tab->dragging())
    return true;

  // Let all non-clipped closing tabs be visible.  These will probably finish
  // closing before the user changes the active tab, so there's little reason to
  // try and make the more complex logic below apply.
  if (tab->closing())
    return true;

  // Now we need to check whether the tab isn't currently clipped, but could
  // become clipped if we changed the active tab, widening either this tab or
  // the tabstrip portion before it.

  // Pinned tabs don't change size when activated, so any tab in the pinned tab
  // region is safe.
  if (tab->data().pinned)
    return true;

  // If the active tab is on or before this tab, we're safe.
  if (controller_->GetActiveIndex() <= GetModelIndexOfTab(tab))
    return true;

  // We need to check what would happen if the active tab were to move to this
  // tab or before.
  return (right_edge + current_active_width_ - current_inactive_width_) <=
         tabstrip_right;
}

void TabStrip::SetSelection(const ui::ListSelectionModel& new_selection) {
  if (selected_tabs_.active() != new_selection.active()) {
    if (selected_tabs_.active() >= 0)
      tab_at(selected_tabs_.active())->ActiveStateChanged();
    if (new_selection.active() >= 0)
      tab_at(new_selection.active())->ActiveStateChanged();
  }

  if (touch_layout_) {
    touch_layout_->SetActiveIndex(new_selection.active());
    // Only start an animation if we need to. Otherwise clicking on an
    // unselected tab and dragging won't work because dragging is only allowed
    // if not animating.
    if (!views::ViewModelUtils::IsAtIdealBounds(tabs_))
      AnimateToIdealBounds();
    SchedulePaint();
  } else {
    if (current_inactive_width_ == current_active_width_) {
      // When tabs are wide enough, selecting a new tab cannot change the
      // ideal bounds, so only a repaint is necessary.
      SchedulePaint();
    } else if (IsAnimating()) {
      // The selection change will have modified the ideal bounds of the tabs
      // in |selected_tabs_| and |new_selection|.  We need to recompute.
      // Note: This is safe even if we're in the midst of mouse-based tab
      // closure--we won't expand the tabstrip back to the full window
      // width--because PrepareForCloseAt() will have set
      // |available_width_for_tabs_| already.
      GenerateIdealBounds();
      AnimateToIdealBounds();
    } else {
      // As in the animating case above, the selection change will have
      // affected the desired bounds of the tabs, but since we're not animating
      // we can just snap to the new bounds.
      DoLayout();
    }
  }

  // Use STLSetDifference to get the indices of elements newly selected
  // and no longer selected, since selected_indices() is always sorted.
  ui::ListSelectionModel::SelectedIndices no_longer_selected =
      base::STLSetDifference<ui::ListSelectionModel::SelectedIndices>(
          selected_tabs_.selected_indices(), new_selection.selected_indices());
  ui::ListSelectionModel::SelectedIndices newly_selected =
      base::STLSetDifference<ui::ListSelectionModel::SelectedIndices>(
          new_selection.selected_indices(), selected_tabs_.selected_indices());

  // Fire accessibility events that reflect the changes to selection.
  for (auto tab_index : no_longer_selected) {
    tab_at(tab_index)->NotifyAccessibilityEvent(
        ax::mojom::Event::kSelectionRemove, true);
  }
  for (auto tab_index : newly_selected) {
    tab_at(tab_index)->NotifyAccessibilityEvent(ax::mojom::Event::kSelectionAdd,
                                                true);
  }
  tab_at(new_selection.active())
      ->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  selected_tabs_ = new_selection;

  // Notify all tabs whose selected state changed.
  for (auto tab_index :
       base::STLSetUnion<ui::ListSelectionModel::SelectedIndices>(
           no_longer_selected, newly_selected)) {
    tab_at(tab_index)->SelectedStateChanged();
  }
}

void TabStrip::SetTabNeedsAttention(int model_index, bool attention) {
  tab_at(model_index)->SetTabNeedsAttention(attention);
}

int TabStrip::GetModelIndexOfTab(const Tab* tab) const {
  return tabs_.GetIndexOfView(tab);
}

int TabStrip::GetModelCount() const {
  return controller_->GetCount();
}

bool TabStrip::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

bool TabStrip::IsDragSessionActive() const {
  return drag_controller_ != nullptr;
}

bool TabStrip::IsActiveDropTarget() const {
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    if (tab->dragging())
      return true;
  }
  return false;
}

SkAlpha TabStrip::GetInactiveAlpha(bool for_new_tab_button) const {
#if defined(OS_CHROMEOS)
  static const SkAlpha kInactiveTabAlphaAsh = 230;
  const SkAlpha base_alpha = kInactiveTabAlphaAsh;
#else
  static const SkAlpha kInactiveTabAlphaGlass = 200;
  static const SkAlpha kInactiveTabAlphaOpaque = 255;
  const SkAlpha base_alpha = TitlebarBackgroundIsTransparent()
                                 ? kInactiveTabAlphaGlass
                                 : kInactiveTabAlphaOpaque;
#endif  // OS_CHROMEOS
  static const double kMultiSelectionMultiplier = 0.6;
  return (for_new_tab_button || (GetSelectionModel().size() <= 1))
             ? base_alpha
             : static_cast<SkAlpha>(kMultiSelectionMultiplier * base_alpha);
}

bool TabStrip::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

void TabStrip::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator_.Cancel();

  if (layout)
    DoLayout();
}

const ui::ListSelectionModel& TabStrip::GetSelectionModel() const {
  return controller_->GetSelectionModel();
}

bool TabStrip::SupportsMultipleSelection() {
  // Currently we only allow single selection in touch layout mode.
  return touch_layout_ == nullptr;
}

NewTabButtonPosition TabStrip::GetNewTabButtonPosition() const {
  return controller_->GetNewTabButtonPosition();
}

bool TabStrip::ShouldHideCloseButtonForTab(Tab* tab) const {
  if (tab->IsActive()) {
    // For single-tab mode, the close button looks like it's floating oddly in
    // space for LEADING/TRAILING NTBs, so hide in that case.
    return SingleTabMode() &&
           controller_->GetNewTabButtonPosition() != AFTER_TABS;
  }
  return !!touch_layout_;
}

bool TabStrip::ShouldShowCloseButtonOnHover() {
  return !touch_layout_;
}

bool TabStrip::MaySetClip() {
  // Only touch layout needs to restrict the clip.
  return touch_layout_ || IsStackingDraggedTabs();
}

void TabStrip::SelectTab(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->SelectTab(model_index);
}

void TabStrip::ExtendSelectionTo(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->ExtendSelectionTo(model_index);
}

void TabStrip::ToggleSelected(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->ToggleSelected(model_index);
}

void TabStrip::AddSelectionFromAnchorTo(Tab* tab) {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    controller_->AddSelectionFromAnchorTo(model_index);
}

void TabStrip::CloseTab(Tab* tab, CloseTabSource source) {
  int model_index = GetModelIndexOfTab(tab);
  if (tab->closing()) {
    // If the tab is already closing, close the next tab. We do this so that the
    // user can rapidly close tabs by clicking the close button and not have
    // the animations interfere with that.
    model_index = FindClosingTab(tab).first->first;
  }

  if (!IsValidModelIndex(model_index))
    return;

  if (!in_tab_close_ && IsAnimating()) {
    // Cancel any current animations. We do this as remove uses the current
    // ideal bounds and we need to know ideal bounds is in a good state.
    StopAnimating(true);
  }

  if (GetWidget()) {
    in_tab_close_ = true;
    resize_layout_timer_.Stop();
    if (source == CLOSE_TAB_FROM_TOUCH)
      StartResizeLayoutTabsFromTouchTimer();
    else
      AddMessageLoopObserver();
  }

  controller_->CloseTab(model_index, source);
}

void TabStrip::ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) {
  controller_->ShowContextMenuForTab(tab, p, source_type);
}

bool TabStrip::IsActiveTab(const Tab* tab) const {
  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsActiveTab(model_index);
}

bool TabStrip::IsTabSelected(const Tab* tab) const {
  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsTabSelected(model_index);
}

bool TabStrip::IsTabPinned(const Tab* tab) const {
  if (tab->closing())
    return false;

  int model_index = GetModelIndexOfTab(tab);
  return IsValidModelIndex(model_index) &&
         controller_->IsTabPinned(model_index);
}

bool TabStrip::IsFirstVisibleTab(const Tab* tab) const {
  return GetModelIndexOfTab(tab) == 0;
}

bool TabStrip::IsLastVisibleTab(const Tab* tab) const {
  return GetLastVisibleTab() == tab;
}

bool TabStrip::SingleTabMode() const {
  return controller_->IsSingleTabModeAvailable() && tab_count() == 1 &&
         !tab_at(0)->data().pinned;
}

void TabStrip::MaybeStartDrag(
    Tab* tab,
    const ui::LocatedEvent& event,
    const ui::ListSelectionModel& original_selection) {
  // Don't accidentally start any drag operations during animations if the
  // mouse is down... during an animation tabs are being resized automatically,
  // so the View system can misinterpret this easily if the mouse is down that
  // the user is dragging.
  if (IsAnimating() || tab->closing() ||
      controller_->HasAvailableDragActions() == 0) {
    return;
  }

  int model_index = GetModelIndexOfTab(tab);
  if (!IsValidModelIndex(model_index)) {
    CHECK(false);
    return;
  }
  Tabs tabs;
  int x = tab->GetMirroredXInView(event.x());
  int y = event.y();
  // Build the set of selected tabs to drag and calculate the offset from the
  // first selected tab.
  for (int i = 0; i < tab_count(); ++i) {
    Tab* other_tab = tab_at(i);
    if (IsTabSelected(other_tab)) {
      tabs.push_back(other_tab);
      if (other_tab == tab)
        x += GetSizeNeededForTabs(tabs) - tab->width();
    }
  }
  DCHECK(!tabs.empty());
  DCHECK(base::ContainsValue(tabs, tab));
  ui::ListSelectionModel selection_model;
  if (!original_selection.IsSelected(model_index))
    selection_model = original_selection;
  // Delete the existing DragController before creating a new one. We do this as
  // creating the DragController remembers the WebContents delegates and we need
  // to make sure the existing DragController isn't still a delegate.
  drag_controller_.reset();
  TabDragController::MoveBehavior move_behavior = TabDragController::REORDER;
  // Use MOVE_VISIBLE_TABS in the following conditions:
  // . Mouse event generated from touch and the left button is down (the right
  //   button corresponds to a long press, which we want to reorder).
  // . Gesture tap down and control key isn't down.
  // . Real mouse event and control is down. This is mostly for testing.
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_GESTURE_TAP_DOWN);
  if (touch_layout_ &&
      ((event.type() == ui::ET_MOUSE_PRESSED &&
        (((event.flags() & ui::EF_FROM_TOUCH) &&
          static_cast<const ui::MouseEvent&>(event).IsLeftMouseButton()) ||
         (!(event.flags() & ui::EF_FROM_TOUCH) &&
          static_cast<const ui::MouseEvent&>(event).IsControlDown()))) ||
       (event.type() == ui::ET_GESTURE_TAP_DOWN && !event.IsControlDown()))) {
    move_behavior = TabDragController::MOVE_VISIBLE_TABS;
  }

  drag_controller_.reset(new TabDragController);
  drag_controller_->Init(this, tab, tabs, gfx::Point(x, y), event.x(),
                         std::move(selection_model), move_behavior,
                         EventSourceFromEvent(event));
}

void TabStrip::ContinueDrag(views::View* view, const ui::LocatedEvent& event) {
  if (drag_controller_.get() &&
      drag_controller_->event_source() == EventSourceFromEvent(event)) {
    gfx::Point screen_location(event.location());
    views::View::ConvertPointToScreen(view, &screen_location);
    drag_controller_->Drag(screen_location);
  }
}

bool TabStrip::EndDrag(EndDragReason reason) {
  if (!drag_controller_.get())
    return false;
  bool started_drag = drag_controller_->started_drag();
  drag_controller_->EndDrag(reason);
  return started_drag;
}

Tab* TabStrip::GetTabAt(const gfx::Point& point) {
  views::View* view = GetEventHandlerForPoint(point);
  if (!view)
    return nullptr;  // No tab contains the point.

  // Walk up the view hierarchy until we find a tab, or the TabStrip.
  while (view && view != this && view->id() != VIEW_ID_TAB)
    view = view->parent();

  return view && view->id() == VIEW_ID_TAB ? static_cast<Tab*>(view) : nullptr;
}

const Tab* TabStrip::GetAdjacentTab(const Tab* tab, int offset) {
  int index = GetModelIndexOfTab(tab);
  if (index < 0)
    return nullptr;
  index += offset;
  return IsValidModelIndex(index) ? tab_at(index) : nullptr;
}

void TabStrip::OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) {
  UpdateStackedLayoutFromMouseEvent(source, event);
}

bool TabStrip::ShouldPaintTab(const Tab* tab, float scale, gfx::Path* clip) {
  if (!MaySetClip())
    return true;

  int index = GetModelIndexOfTab(tab);
  if (index == -1)
    return true;  // Tab is closing, paint it all.

  int active_index = IsStackingDraggedTabs() ? controller_->GetActiveIndex()
                                             : touch_layout_->active_index();
  if (active_index == tab_count())
    active_index--;

  const gfx::Rect& current_bounds = tab_at(index)->bounds();
  if (index < active_index) {
    const Tab* next_tab = tab_at(index + 1);
    const gfx::Rect& next_bounds = next_tab->bounds();
    if (current_bounds.x() == next_bounds.x())
      return false;

    if (current_bounds.x() > next_bounds.x())
      return true;  // Can happen during dragging.

    *clip =
        next_tab->tab_style()->GetPath(TabStyle::PathType::kExteriorClip, scale,
                                       false, TabStyle::RenderUnits::kDips);

    clip->offset(SkIntToScalar(next_bounds.x() - current_bounds.x()), 0);
  } else if (index > active_index && index > 0) {
    const Tab* prev_tab = tab_at(index - 1);
    const gfx::Rect& previous_bounds = prev_tab->bounds();
    if (current_bounds.x() == previous_bounds.x())
      return false;

    if (current_bounds.x() < previous_bounds.x())
      return true;  // Can happen during dragging.

    *clip =
        prev_tab->tab_style()->GetPath(TabStyle::PathType::kExteriorClip, scale,
                                       false, TabStyle::RenderUnits::kDips);
    clip->offset(SkIntToScalar(previous_bounds.x() - current_bounds.x()), 0);
  }
  return true;
}

int TabStrip::GetStrokeThickness() const {
  return controller_->ShouldDrawStrokes() ? 1 : 0;
}

bool TabStrip::CanPaintThrobberToLayer() const {
  // Disable layer-painting of throbbers if dragging, if any tab animation is in
  // progress, or if stacked tabs are enabled. Also disable in fullscreen: when
  // "immersive" the tab strip could be sliding in or out; for other modes,
  // there's no tab strip.
  const bool dragging = drag_controller_ && drag_controller_->started_drag();
  const views::Widget* widget = GetWidget();
  return widget && !touch_layout_ && !dragging && !IsAnimating() &&
         !widget->IsFullscreen();
}

bool TabStrip::HasVisibleBackgroundTabShapes() const {
  return controller_->HasVisibleBackgroundTabShapes();
}

SkColor TabStrip::GetToolbarTopSeparatorColor() const {
  return controller_->GetToolbarTopSeparatorColor();
}

SkColor TabStrip::GetTabSeparatorColor() const {
  return separator_color_;
}

SkColor TabStrip::GetTabBackgroundColor(TabState state) const {
  return controller_->GetTabBackgroundColor(state);
}

SkColor TabStrip::GetTabForegroundColor(TabState state) const {
  return controller_->GetTabForegroundColor(state);
}

// Returns the accessible tab name for the tab.
base::string16 TabStrip::GetAccessibleTabName(const Tab* tab) const {
  int model_index = GetModelIndexOfTab(tab);
  if (IsValidModelIndex(model_index))
    return controller_->GetAccessibleTabName(tab);
  return base::string16();
}

int TabStrip::GetBackgroundResourceId(
    bool* has_custom_image,
    BrowserNonClientFrameView::ActiveState active_state) const {
  if (!TitlebarBackgroundIsTransparent()) {
    return controller_->GetTabBackgroundResourceId(active_state,
                                                   has_custom_image);
  }

  constexpr int kBackgroundIdGlass = IDR_THEME_TAB_BACKGROUND_V;
  *has_custom_image = GetThemeProvider()->HasCustomImage(kBackgroundIdGlass);
  return kBackgroundIdGlass;
}

gfx::Rect TabStrip::GetTabAnimationTargetBounds(const Tab* tab) {
  return bounds_animator_.GetTargetBounds(tab);
}

void TabStrip::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
  if (reset_to_shrink_on_exit_) {
    reset_to_shrink_on_exit_ = false;
    SetStackedLayout(false);
    controller_->StackedLayoutMaybeChanged();
  }
}

float TabStrip::GetHoverOpacityForTab(float range_parameter) const {
  return gfx::Tween::FloatValueBetween(range_parameter, hover_opacity_min_,
                                       hover_opacity_max_);
}

float TabStrip::GetHoverOpacityForRadialHighlight() const {
  return radial_highlight_opacity_;
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, views::View overrides:

void TabStrip::Layout() {
  // Only do a layout if our size changed.
  if (last_layout_size_ == size())
    return;
  if (IsDragSessionActive())
    return;
  DoLayout();
}

void TabStrip::PaintChildren(const views::PaintInfo& paint_info) {
  // The view order doesn't match the paint order (tabs_ contains the tab
  // ordering). Additionally we need to paint the tabs that are closing in
  // |tabs_closing_map_|.
  bool is_dragging = false;
  Tab* active_tab = nullptr;
  Tabs tabs_dragging;
  Tabs selected_and_hovered_tabs;

  {
    // Using transparency here normally disables LCD AA on title text in favor
    // of greyscale AA.  In most cases, the tabs will be rendered opaquely on an
    // opaque background before compositing, so it's safe to pass false for
    // |lcd_text_requires_opaque_layer| to allow LCD AA.  If the theme does not
    // fill a background color, however, it must fall back to greyscale AA.
    bool has_background_color = GetThemeProvider()->GetDisplayProperty(
        ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR);
    ui::CompositingRecorder opacity_recorder(
        paint_info.context(), GetInactiveAlpha(false), !has_background_color);

    // When background tab shapes are visible, as for hovered or selected tabs,
    // the paint order must be handled carefully to avoid Z-order errors, so
    // this code defers drawing such tabs until later.
    const auto paint_or_add_to_tabs = [&paint_info,
                                       &selected_and_hovered_tabs](Tab* tab) {
      if (tab->IsSelected() ||
          (tab->mouse_hovered() || tab->hover_controller()->ShouldDraw())) {
        selected_and_hovered_tabs.push_back(tab);
      } else {
        tab->Paint(paint_info);
      }
    };

    const auto paint_closing_tabs = [=](int index) {
      if (tabs_closing_map_.find(index) == tabs_closing_map_.end())
        return;
      for (Tab* tab : base::Reversed(tabs_closing_map_[index]))
        paint_or_add_to_tabs(tab);
    };

    paint_closing_tabs(tab_count());

    int active_tab_index = -1;
    for (int i = tab_count() - 1; i >= 0; --i) {
      Tab* tab = tab_at(i);
      if (tab->dragging() && !stacked_layout_) {
        is_dragging = true;
        if (tab->IsActive()) {
          active_tab = tab;
          active_tab_index = i;
        } else {
          tabs_dragging.push_back(tab);
        }
      } else if (tab->IsActive()) {
        active_tab = tab;
        active_tab_index = i;
      } else if (!stacked_layout_) {
        paint_or_add_to_tabs(tab);
      }
      paint_closing_tabs(i);
    }

    // Draw from the left and then the right if we're in touch mode.
    if (stacked_layout_ && active_tab_index >= 0) {
      for (int i = 0; i < active_tab_index; ++i) {
        Tab* tab = tab_at(i);
        tab->Paint(paint_info);
      }

      for (int i = tab_count() - 1; i > active_tab_index; --i) {
        Tab* tab = tab_at(i);
        tab->Paint(paint_info);
      }
    }
  }

  // This will sort the inactive tabs so that they paint in the following order:
  //
  // o Unselected and hover-animating tabs in ascending animation value order.
  // o The single unselected mouse hovered tab, if present.
  // o Selected tabs in trailing-to-leading order.
  // o Selected and hover animating tabs in ascending animation value order.
  // o The single selected and mouse_hovered tab, if present.
  //
  // This is accomplished by adding a "weight" to the current hover animation
  // value which represents the above groupings.
  //
  // 0.0 == sort_value         Unselected/non hover animating (already painted).
  // 0.0 <  sort_value <= 1.0  Unselected/hover animating.
  // 2.0 <= sort_value <= 3.0  Unselected/mouse hovered tab.
  // 4.0 == sort_value         Selected/non hover animating.
  // 4.0 <  sort_value <= 5.0  Selected/hover animating.
  // 6.0 <= sort_value <= 7.0  Selected/mouse hovered tab.
  //
  auto tab_sort_value = [](Tab* tab) {
    float sort_value = tab->hover_controller()->GetAnimationValue();
    if (tab->IsSelected())
      sort_value += 4.f;
    if (tab->mouse_hovered())
      sort_value += 2.f;
    return sort_value;
  };

  std::stable_sort(selected_and_hovered_tabs.begin(),
                   selected_and_hovered_tabs.end(),
                   [&tab_sort_value](Tab* tab1, Tab* tab2) {
                     return tab_sort_value(tab1) < tab_sort_value(tab2);
                   });
  for (Tab* tab : selected_and_hovered_tabs)
    tab->Paint(paint_info);

  // Always paint the active tab over all the inactive tabs.
  if (active_tab && !is_dragging)
    active_tab->Paint(paint_info);

  // Paint the New Tab button.
  if (new_tab_button_->state() == views::Button::STATE_PRESSED) {
    new_tab_button_->Paint(paint_info);
  } else {
    // Match the inactive tab opacity for non-pressed states.  See comments in
    // NewTabButton::PaintFill() for why we don't do this for the pressed state.
    // This call doesn't need to set |lcd_text_requires_opaque_layer| to false
    // because no text will be drawn.
    ui::CompositingRecorder opacity_recorder(paint_info.context(),
                                             GetInactiveAlpha(true), true);
    new_tab_button_->Paint(paint_info);
  }

  // And the dragged tabs.
  for (size_t i = 0; i < tabs_dragging.size(); ++i)
    tabs_dragging[i]->Paint(paint_info);

  // If the active tab is being dragged, it goes last.
  if (active_tab && is_dragging)
    active_tab->Paint(paint_info);
}

void TabStrip::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Paint a separator before a trailing new tab button.
  if (GetNewTabButtonPosition() == TRAILING) {
    const gfx::Size separator_size = TabStyle::GetSeparatorSize();
    const int width = separator_size.width();
    const float separator_height = separator_size.height();
    gfx::RectF separator_bounds(
        GetMirroredXWithWidthInView(new_tab_button_bounds_.x() - width, width),
        (height() - separator_height) / 2, width, separator_height);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetTabSeparatorColor());
    canvas->DrawRect(separator_bounds, flags);
  }
}

const char* TabStrip::GetClassName() const {
  static const char kViewClassName[] = "TabStrip";
  return kViewClassName;
}

gfx::Size TabStrip::CalculatePreferredSize() const {
  int needed_tab_width;
  if (touch_layout_ || adjust_layout_) {
    // For stacked tabs the minimum size is calculated as the size needed to
    // handle showing any number of tabs.
    needed_tab_width =
        GetStackableTabWidth() + (2 * kStackedPadding * kMaxStackedCount);
  } else {
    // Otherwise the minimum width is based on the actual number of tabs.
    const int pinned_tab_count = GetPinnedTabCount();
    needed_tab_width = pinned_tab_count * TabStyle::GetPinnedWidth();
    const int remaining_tab_count = tab_count() - pinned_tab_count;
    const int min_selected_width = TabStyle::GetMinimumActiveWidth();
    const int min_unselected_width = TabStyle::GetMinimumInactiveWidth();
    if (remaining_tab_count > 0) {
      needed_tab_width += min_selected_width +
                          ((remaining_tab_count - 1) * min_unselected_width);
    }

    const int overlap = TabStyle::GetTabOverlap();
    if (tab_count() > 1)
      needed_tab_width -= (tab_count() - 1) * overlap;

    // Don't let the tabstrip shrink smaller than is necessary to show one tab,
    // and don't force it to be larger than is necessary to show 20 tabs.
    const int largest_min_tab_width =
        min_selected_width + 19 * (min_unselected_width - overlap);
    needed_tab_width = std::min(std::max(needed_tab_width, min_selected_width),
                                largest_min_tab_width);
  }
  return gfx::Size(needed_tab_width + TabToFollowingNewTabButtonSpacing() +
                       new_tab_button_bounds_.width() + GetFrameGrabWidth(),
                   GetLayoutConstant(TAB_HEIGHT));
}

void TabStrip::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTabList;
}

views::View* TabStrip::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return nullptr;

  if (!touch_layout_) {
    // Return any view that isn't a Tab or this TabStrip immediately. We don't
    // want to interfere.
    views::View* v = View::GetTooltipHandlerForPoint(point);
    if (v && v != this && strcmp(v->GetClassName(), Tab::kViewClassName))
      return v;

    views::View* tab = FindTabHitByPoint(point);
    if (tab)
      return tab;
  } else {
    if (new_tab_button_->visible()) {
      views::View* view =
          ConvertPointToViewAndGetTooltipHandler(this, new_tab_button_, point);
      if (view)
        return view;
    }
    Tab* tab = FindTabForEvent(point);
    if (tab)
      return ConvertPointToViewAndGetTooltipHandler(this, tab, point);
  }
  return this;
}

void TabStrip::OnThemeChanged() {
  // Adding or removing a frame image will change whether single tab mode is
  // available.
  SingleTabModeChanged();
}

BrowserRootView::DropIndex TabStrip::GetDropIndex(
    const ui::DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    const int tab_max_x = tab->x() + tab->width();

    // When hovering over the left or right quarter of a tab, the drop indicator
    // will point between tabs.
    const int hot_width = tab->width() / 4;

    if (x < tab_max_x) {
      if (x >= (tab_max_x - hot_width))
        return {i + 1, true};
      return {i, x < tab->x() + hot_width};
    }
  }

  // The drop isn't over a tab, add it to the end.
  return {tab_count(), true};
}

views::View* TabStrip::GetViewForDrop() {
  return this;
}

void TabStrip::HandleDragUpdate(
    const base::Optional<BrowserRootView::DropIndex>& index) {
  SetDropArrow(index);
}

void TabStrip::HandleDragExited() {
  SetDropArrow({});
}

///////////////////////////////////////////////////////////////////////////////
// TabStrip, private:

void TabStrip::Init() {
  set_id(VIEW_ID_TAB_STRIP);
  // So we get enter/exit on children to switch stacked layout on and off.
  set_notify_enter_exit_on_child(true);

  current_active_width_ = TabStyle::GetStandardWidth();
  current_inactive_width_ = current_active_width_;

  new_tab_button_ = new NewTabButton(this, this);
  new_tab_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));
  new_tab_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                     views::ImageButton::ALIGN_BOTTOM);
  new_tab_button_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(new_tab_button_));
  AddChildView(new_tab_button_);

  UpdateNewTabButtonBorder();
  new_tab_button_bounds_.set_size(new_tab_button_->GetPreferredSize());

  if (g_drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image = GetDropArrowImage(true);
    g_drop_indicator_width = drop_image->width();
    g_drop_indicator_height = drop_image->height();
  }

  UpdateContrastRatioValues();
}

void TabStrip::StartInsertTabAnimation(int model_index) {
  PrepareForAnimation();

  // The TabStrip can now use its entire width to lay out Tabs.
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  GenerateIdealBounds();

  // Insert the tab just after the current right edge of the previous tab, if
  // any.
  gfx::Rect bounds = ideal_bounds(model_index);
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (model_index > 0)
    bounds.set_x(tab_at(model_index - 1)->bounds().right() - tab_overlap);

  // Start at the width of the overlap in order to animate at the same speed the
  // surrounding tabs are moving, since at this width the subsequent tab is
  // naturally positioned at the same X coordinate.
  bounds.set_width(tab_overlap);

  // Animate in to the full width.
  tab_at(model_index)->SetBoundsRect(bounds);
  AnimateToIdealBounds();
}

void TabStrip::StartMoveTabAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    // If the tab is being dragged manually, skip it.
    Tab* tab = tab_at(i);
    if (tab->dragging() && !bounds_animator_.IsAnimating(tab))
      continue;

    // Also skip tabs already being animated to the same ideal bounds.  Calling
    // AnimateViewTo() again restarts the animation, which changes the timing of
    // how the tab animates, leading to hitches.
    const gfx::Rect& target_bounds = ideal_bounds(i);
    if (bounds_animator_.GetTargetBounds(tab) == target_bounds)
      continue;

    bounds_animator_.AnimateViewTo(tab, target_bounds);

    // Set an animation delegate for the tab so it will clip appropriately.
    // Don't do this if dragging() is true.  In this case the tab was
    // previously being dragged and is now animating back to its ideal
    // bounds; it already has an associated ResetDraggingStateDelegate that
    // will reset this dragging state. Replacing this delegate would mean
    // this code would also need to reset the dragging state immediately,
    // and that could allow the new tab button to be drawn atop this tab.
    if (!tab->dragging()) {
      bounds_animator_.SetAnimationDelegate(
          tab, std::make_unique<TabAnimationDelegate>(this, tab));
    }
  }

  if (bounds_animator_.GetTargetBounds(new_tab_button_) !=
      new_tab_button_bounds_)
    bounds_animator_.AnimateViewTo(new_tab_button_, new_tab_button_bounds_);
}

bool TabStrip::ShouldHighlightCloseButtonAfterRemove() {
  return in_tab_close_;
}

int TabStrip::TabToFollowingNewTabButtonSpacing() const {
  // When there is no following new tab button, there is no relevant spacing.
  if (controller_->GetNewTabButtonPosition() != AFTER_TABS)
    return 0;

  // The new tab button contains built-in padding, and should be placed flush
  // against the trailing separator.
  return -TabStyle::GetTabInternalPadding().right();
}

bool TabStrip::MayHideNewTabButtonWhileDragging() const {
  return controller_->GetNewTabButtonPosition() == AFTER_TABS;
}

int TabStrip::GetFrameGrabWidth() const {
  // The apparent width of the grab area.
  int width = 50;

  const NewTabButtonPosition position = controller_->GetNewTabButtonPosition();
  if (position == AFTER_TABS) {
    // The grab area is adjacent to the new tab button.  Treat the padding in
    // the new tab button as part of the grab area.
    width -= new_tab_button_->GetInsets().right();
  } else {
    // The grab area is adjacent to the last tab.  This tab has mostly empty
    // space where the outer (lower) corners are, which should be treated as
    // part of the grab area, so decrease the size of the remaining grab area by
    // that width.
    width -= TabStyle::GetTabInternalPadding().right();

    // The trailing NTB has a separator just before it; don't count that as part
    // of the frame grab width since it looks visually distinct.
    if (position == TRAILING)
      width += TabStyle::GetSeparatorSize().width();
  }

  return width;
}

bool TabStrip::TitlebarBackgroundIsTransparent() const {
#if defined(OS_WIN)
  // Windows 8+ uses transparent window contents (because the titlebar area is
  // drawn by the system and not Chrome), but the actual titlebar is opaque.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return false;
#endif
  return GetWidget()->ShouldWindowContentsBeTransparent();
}

void TabStrip::DoLayout() {
  last_layout_size_ = size();

  StopAnimating(false);

  SwapLayoutIfNecessary();

  if (touch_layout_)
    touch_layout_->SetWidth(GetTabAreaWidth());

  GenerateIdealBounds();

  views::ViewModelUtils::SetViewBoundsToIdealBounds(tabs_);
  SetTabVisibility();

  SchedulePaint();

  bounds_animator_.StopAnimatingView(new_tab_button_);
  new_tab_button_->SetBoundsRect(new_tab_button_bounds_);
}

void TabStrip::SetTabVisibility() {
  // We could probably be more efficient here by making use of the fact that the
  // tabstrip will always have any visible tabs, and then any invisible tabs, so
  // we could e.g. binary-search for the changeover point.  But since we have to
  // iterate through all the tabs to call SetVisible() anyway, it doesn't seem
  // worth it.
  for (int i = 0; i < tab_count(); ++i) {
    Tab* tab = tab_at(i);
    tab->SetVisible(ShouldTabBeVisible(tab));
  }
  for (const auto& closing_tab : tabs_closing_map_) {
    for (Tab* tab : closing_tab.second)
      tab->SetVisible(ShouldTabBeVisible(tab));
  }
}

void TabStrip::DragActiveTabStacked(const std::vector<int>& initial_positions,
                                    int delta) {
  DCHECK_EQ(tab_count(), static_cast<int>(initial_positions.size()));
  DCHECK(touch_layout_);
  SetIdealBoundsFromPositions(initial_positions);
  touch_layout_->DragActiveTab(delta);
  DoLayout();
}

void TabStrip::SetIdealBoundsFromPositions(const std::vector<int>& positions) {
  if (static_cast<size_t>(tab_count()) != positions.size())
    return;

  for (int i = 0; i < tab_count(); ++i) {
    gfx::Rect bounds(ideal_bounds(i));
    bounds.set_x(positions[i]);
    tabs_.set_ideal_bounds(i, bounds);
  }
}

bool TabStrip::IsStackingDraggedTabs() const {
  return drag_controller_.get() && drag_controller_->started_drag() &&
         (drag_controller_->move_behavior() ==
          TabDragController::MOVE_VISIBLE_TABS);
}

void TabStrip::LayoutDraggedTabsAt(const Tabs& tabs,
                                   Tab* active_tab,
                                   const gfx::Point& location,
                                   bool initial_drag) {
  // Immediately hide the new tab button if the last tab is being dragged.
  const Tab* last_visible_tab = GetLastVisibleTab();
  if (MayHideNewTabButtonWhileDragging() && last_visible_tab &&
      last_visible_tab->dragging())
    new_tab_button_->SetVisible(false);
  std::vector<gfx::Rect> bounds = CalculateBoundsForDraggedTabs(tabs);
  DCHECK_EQ(tabs.size(), bounds.size());
  int active_tab_model_index = GetModelIndexOfTab(active_tab);
  int active_tab_index = static_cast<int>(
      std::find(tabs.begin(), tabs.end(), active_tab) - tabs.begin());
  for (size_t i = 0; i < tabs.size(); ++i) {
    Tab* tab = tabs[i];
    gfx::Rect new_bounds = bounds[i];
    new_bounds.Offset(location.x(), location.y());
    int consecutive_index =
        active_tab_model_index - (active_tab_index - static_cast<int>(i));
    // If this is the initial layout during a drag and the tabs aren't
    // consecutive animate the view into position. Do the same if the tab is
    // already animating (which means we previously caused it to animate).
    if ((initial_drag && GetModelIndexOfTab(tabs[i]) != consecutive_index) ||
        bounds_animator_.IsAnimating(tabs[i])) {
      bounds_animator_.SetTargetBounds(tabs[i], new_bounds);
    } else {
      tab->SetBoundsRect(new_bounds);
    }
  }
  SetTabVisibility();
}

// static
std::vector<gfx::Rect> TabStrip::CalculateBoundsForDraggedTabs(
    const Tabs& tabs) {
  DCHECK(!tabs.empty());

  std::vector<gfx::Rect> bounds;
  const int overlap = TabStyle::GetTabOverlap();
  int x = 0;
  for (const Tab* tab : tabs) {
    const int width = tab->width();
    bounds.push_back(gfx::Rect(x, 0, width, tab->height()));
    x += width - overlap;
  }

  return bounds;
}

int TabStrip::TabStartX() const {
  if (controller_->GetNewTabButtonPosition() != LEADING)
    return 0;

  // The new tab button should sit relatively flush against the leading edge of
  // the following tab, which means accounting for any negative space in the tab
  // itself.
  return new_tab_button_bounds_.width() -
         TabStyle::GetTabInternalPadding().left();
}

int TabStrip::TabDragAreaEndX() const {
  return MayHideNewTabButtonWhileDragging() ? (width() - GetFrameGrabWidth())
                                            : (TabStartX() + GetTabAreaWidth());
}

int TabStrip::NewTabButtonIdealX() const {
  const auto position = controller_->GetNewTabButtonPosition();
  if (position == LEADING)
    return 0;

  const int tab_area_width = width() - new_tab_button_bounds_.width();
  if (position == TRAILING)
    return tab_area_width;

  const int trailing_x = tabs_.ideal_bounds(tab_count() - 1).right();
  // For non-stacked tabs the ideal bounds may go outside the bounds of the
  // tabstrip. Constrain the x-coordinate of the new tab button so that it is
  // always visible.
  return std::min(tab_area_width - GetFrameGrabWidth(),
                  trailing_x + TabToFollowingNewTabButtonSpacing());
}

int TabStrip::GetSizeNeededForTabs(const Tabs& tabs) {
  int width = 0;
  for (const Tab* tab : tabs)
    width += tab->width();
  if (!tabs.empty())
    width -= TabStyle::GetTabOverlap() * (tabs.size() - 1);
  return width;
}

int TabStrip::GetPinnedTabCount() const {
  int pinned_count = 0;
  while (pinned_count < tab_count() && tab_at(pinned_count)->data().pinned)
    pinned_count++;
  return pinned_count;
}

const Tab* TabStrip::GetLastVisibleTab() const {
  for (int i = tab_count() - 1; i >= 0; --i) {
    const Tab* tab = tab_at(i);
    if (tab->visible())
      return tab;
  }
  // While in normal use the tabstrip should always be wide enough to have at
  // least one visible tab, it can be zero-width in tests, meaning we get here.
  return nullptr;
}

void TabStrip::RemoveTabFromViewModel(int index) {
  Tab* closing_tab = tab_at(index);
  bool closing_tab_was_active = closing_tab->IsActive();

  // We still need to paint the tab until we actually remove it. Put it
  // in tabs_closing_map_ so we can find it.
  tabs_closing_map_[index].push_back(closing_tab);
  UpdateTabsClosingMap(index + 1, -1);
  tabs_.Remove(index);
  selected_tabs_.DecrementFrom(index);

  if (closing_tab_was_active)
    closing_tab->ActiveStateChanged();
}

void TabStrip::RemoveAndDeleteTab(Tab* tab) {
  std::unique_ptr<Tab> deleter(tab);
  FindClosingTabResult res(FindClosingTab(tab));
  res.first->second.erase(res.second);
  if (res.first->second.empty())
    tabs_closing_map_.erase(res.first);
}

void TabStrip::UpdateTabsClosingMap(int index, int delta) {
  if (tabs_closing_map_.empty())
    return;

  if (delta == -1 &&
      tabs_closing_map_.find(index - 1) != tabs_closing_map_.end() &&
      tabs_closing_map_.find(index) != tabs_closing_map_.end()) {
    const Tabs& tabs(tabs_closing_map_[index]);
    tabs_closing_map_[index - 1].insert(tabs_closing_map_[index - 1].end(),
                                        tabs.begin(), tabs.end());
  }
  TabsClosingMap updated_map;
  for (auto& i : tabs_closing_map_) {
    if (i.first > index)
      updated_map[i.first + delta] = i.second;
    else if (i.first < index)
      updated_map[i.first] = i.second;
  }
  if (delta > 0 && tabs_closing_map_.find(index) != tabs_closing_map_.end())
    updated_map[index + delta] = tabs_closing_map_[index];
  tabs_closing_map_.swap(updated_map);
}

void TabStrip::StartedDraggingTabs(const Tabs& tabs) {
  // Let the controller know that the user started dragging tabs.
  controller_->OnStartedDraggingTabs();

  // Hide the new tab button immediately if we didn't originate the drag.
  if (MayHideNewTabButtonWhileDragging() && !drag_controller_)
    new_tab_button_->SetVisible(false);

  PrepareForAnimation();

  // Reset dragging state of existing tabs.
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->set_dragging(false);

  for (size_t i = 0; i < tabs.size(); ++i) {
    tabs[i]->set_dragging(true);
    bounds_animator_.StopAnimatingView(tabs[i]);
  }

  // Move the dragged tabs to their ideal bounds.
  GenerateIdealBounds();

  // Sets the bounds of the dragged tabs.
  for (size_t i = 0; i < tabs.size(); ++i) {
    int tab_data_index = GetModelIndexOfTab(tabs[i]);
    DCHECK_NE(-1, tab_data_index);
    tabs[i]->SetBoundsRect(ideal_bounds(tab_data_index));
  }
  SetTabVisibility();
  SchedulePaint();
}

void TabStrip::DraggedTabsDetached() {
  // Let the controller know that the user is not dragging this tabstrip's tabs
  // anymore.
  controller_->OnStoppedDraggingTabs();
  new_tab_button_->SetVisible(true);
}

void TabStrip::StoppedDraggingTabs(const Tabs& tabs,
                                   const std::vector<int>& initial_positions,
                                   bool move_only,
                                   bool completed) {
  // Let the controller know that the user stopped dragging tabs.
  controller_->OnStoppedDraggingTabs();

  new_tab_button_->SetVisible(true);
  if (move_only && touch_layout_) {
    if (completed)
      touch_layout_->SizeToFit();
    else
      SetIdealBoundsFromPositions(initial_positions);
  }
  bool is_first_tab = true;
  for (size_t i = 0; i < tabs.size(); ++i)
    StoppedDraggingTab(tabs[i], &is_first_tab);
}

void TabStrip::StoppedDraggingTab(Tab* tab, bool* is_first_tab) {
  int tab_data_index = GetModelIndexOfTab(tab);
  if (tab_data_index == -1) {
    // The tab was removed before the drag completed. Don't do anything.
    return;
  }

  if (*is_first_tab) {
    *is_first_tab = false;
    PrepareForAnimation();

    // Animate the view back to its correct position.
    GenerateIdealBounds();
    AnimateToIdealBounds();
  }
  bounds_animator_.AnimateViewTo(tab, ideal_bounds(tab_data_index));
  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator_.SetAnimationDelegate(
      tab, std::make_unique<ResetDraggingStateDelegate>(this, tab));
}

void TabStrip::OwnDragController(TabDragController* controller) {
  // Typically, ReleaseDragController() and OwnDragController() calls are paired
  // via corresponding calls to TabDragController::Detach() and
  // TabDragController::Attach(). There is one exception to that rule: when a
  // drag might start, we create a TabDragController that is owned by the
  // potential source tabstrip in MaybeStartDrag(). If a drag actually starts,
  // we then call Attach() on the source tabstrip, but since the source tabstrip
  // already owns the TabDragController, so we don't need to do anything.
  if (controller != drag_controller_.get())
    drag_controller_.reset(controller);
}

void TabStrip::DestroyDragController() {
  new_tab_button_->SetVisible(true);
  drag_controller_.reset();
}

TabDragController* TabStrip::ReleaseDragController() {
  return drag_controller_.release();
}

TabStrip::FindClosingTabResult TabStrip::FindClosingTab(const Tab* tab) {
  DCHECK(tab->closing());
  for (auto i = tabs_closing_map_.begin(); i != tabs_closing_map_.end(); ++i) {
    auto j = std::find(i->second.begin(), i->second.end(), tab);
    if (j != i->second.end())
      return FindClosingTabResult(i, j);
  }
  NOTREACHED();
  return FindClosingTabResult(tabs_closing_map_.end(), Tabs::iterator());
}

void TabStrip::UpdateStackedLayoutFromMouseEvent(views::View* source,
                                                 const ui::MouseEvent& event) {
  if (!adjust_layout_)
    return;

// The following code attempts to switch to shrink (not stacked) layout when
// the mouse exits the tabstrip (or the mouse is pressed on a stacked tab) and
// to stacked layout when a touch device is used. This is made problematic by
// windows generating mouse move events that do not clearly indicate the move
// is the result of a touch device. This assumes a real mouse is used if
// |kMouseMoveCountBeforeConsiderReal| mouse move events are received within
// the time window |kMouseMoveTime|.  At the time we get a mouse press we know
// whether its from a touch device or not, but we don't layout then else
// everything shifts. Instead we wait for the release.
//
// TODO(sky): revisit this when touch events are really plumbed through.
#if !defined(OS_CHROMEOS)
  constexpr auto kMouseMoveTime = base::TimeDelta::FromMilliseconds(200);
  constexpr int kMouseMoveCountBeforeConsiderReal = 3;
#endif

  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      mouse_move_count_ = 0;
      last_mouse_move_time_ = base::TimeTicks();
      SetResetToShrinkOnExit((event.flags() & ui::EF_FROM_TOUCH) == 0);
      if (reset_to_shrink_on_exit_ && touch_layout_) {
        gfx::Point tab_strip_point(event.location());
        views::View::ConvertPointToTarget(source, this, &tab_strip_point);
        Tab* tab = FindTabForEvent(tab_strip_point);
        if (tab && touch_layout_->IsStacked(GetModelIndexOfTab(tab))) {
          SetStackedLayout(false);
          controller_->StackedLayoutMaybeChanged();
        }
      }
      break;

    case ui::ET_MOUSE_MOVED: {
#if defined(OS_CHROMEOS)
      // Ash does not synthesize mouse events from touch events.
      SetResetToShrinkOnExit(true);
#else
      gfx::Point location(event.location());
      ConvertPointToTarget(source, this, &location);
      if (location == last_mouse_move_location_)
        return;  // Ignore spurious moves.
      last_mouse_move_location_ = location;
      if ((event.flags() & ui::EF_FROM_TOUCH) ||
          (event.flags() & ui::EF_IS_SYNTHESIZED)) {
        last_mouse_move_time_ = base::TimeTicks();
      } else if ((base::TimeTicks::Now() - last_mouse_move_time_) >=
                 kMouseMoveTime) {
        mouse_move_count_ = 1;
        last_mouse_move_time_ = base::TimeTicks::Now();
      } else if (mouse_move_count_ < kMouseMoveCountBeforeConsiderReal) {
        ++mouse_move_count_;
      } else {
        SetResetToShrinkOnExit(true);
      }
#endif
      break;
    }

    case ui::ET_MOUSE_RELEASED: {
      gfx::Point location(event.location());
      ConvertPointToTarget(source, this, &location);
      last_mouse_move_location_ = location;
      mouse_move_count_ = 0;
      last_mouse_move_time_ = base::TimeTicks();
      if ((event.flags() & ui::EF_FROM_TOUCH) == ui::EF_FROM_TOUCH) {
        SetStackedLayout(true);
        controller_->StackedLayoutMaybeChanged();
      }
      break;
    }

    default:
      break;
  }
}

void TabStrip::UpdateContrastRatioValues() {
  // There may be no controller in unit tests, and the call to
  // GetTabBackgroundColor() below requires one, so bail early if it is absent.
  if (!controller_)
    return;

  const SkColor active_tab_bg_color = GetTabBackgroundColor(TAB_ACTIVE);
  const SkColor inactive_tab_bg_color = GetTabBackgroundColor(TAB_INACTIVE);

  // The contrast ratio for the hover effect on standard-width tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.4.
  constexpr float kDesiredContrastHoveredStandardWidthTab = 1.11f;
  const SkAlpha hover_base_alpha_wide =
      color_utils::GetBlendValueWithMinimumContrast(
          inactive_tab_bg_color, active_tab_bg_color, inactive_tab_bg_color,
          kDesiredContrastHoveredStandardWidthTab);
  hover_opacity_min_ = hover_base_alpha_wide / 255.0f;

  // The contrast ratio for the hover effect on min-width tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.65.
  constexpr float kDesiredContrastHoveredMinWidthTab = 1.19f;
  const SkAlpha hover_base_alpha_narrow =
      color_utils::GetBlendValueWithMinimumContrast(
          inactive_tab_bg_color, active_tab_bg_color, inactive_tab_bg_color,
          kDesiredContrastHoveredMinWidthTab);
  hover_opacity_max_ = hover_base_alpha_narrow / 255.0f;

  // The contrast ratio for the radial gradient effect on hovered tabs.
  // In the default color scheme, this corresponds to a hover opacity of 0.45.
  constexpr float kDesiredContrastRadialGradient = 1.13728f;
  const SkAlpha radial_highlight_alpha =
      color_utils::GetBlendValueWithMinimumContrast(
          inactive_tab_bg_color, active_tab_bg_color, inactive_tab_bg_color,
          kDesiredContrastRadialGradient);
  radial_highlight_opacity_ = radial_highlight_alpha / 255.0f;

  // The contrast ratio for the separator between inactive tabs.
  // In the default color scheme, this corresponds to a separator opacity of
  // 0.46.
  const SkColor text_color = GetTabForegroundColor(TAB_INACTIVE);
  constexpr float kTabSeparatorRatio = 1.84f;
  const SkAlpha separator_alpha = color_utils::GetBlendValueWithMinimumContrast(
      inactive_tab_bg_color, text_color, inactive_tab_bg_color,
      kTabSeparatorRatio);
  separator_color_ = color_utils::AlphaBlend(text_color, inactive_tab_bg_color,
                                             separator_alpha);
}

void TabStrip::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (tab_count() == 0)
    return;

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  in_tab_close_ = false;
  available_width_for_tabs_ = -1;
  int pinned_tab_count = GetPinnedTabCount();
  if (pinned_tab_count == tab_count()) {
    // Only pinned tabs, we know the tab widths won't have changed (all
    // pinned tabs have the same width), so there is nothing to do.
    return;
  }
  // Don't try and avoid layout based on tab sizes. If tabs are small enough
  // then the width of the active tab may not change, but other widths may
  // have. This is particularly important if we've overflowed (all tabs are at
  // the min).
  StartResizeLayoutAnimation();
}

void TabStrip::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!drag_controller_.get())
    ResizeLayoutTabs();
  else
    StartResizeLayoutTabsFromTouchTimer();
}

void TabStrip::StartResizeLayoutTabsFromTouchTimer() {
  // Amount of time we delay before resizing after a close from a touch.
  constexpr auto kTouchResizeLayoutTime = base::TimeDelta::FromSeconds(2);

  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(FROM_HERE, kTouchResizeLayoutTime, this,
                             &TabStrip::ResizeLayoutTabsFromTouch);
}

void TabStrip::SetTabBoundsForDrag(const std::vector<gfx::Rect>& tab_bounds) {
  StopAnimating(false);
  DCHECK_EQ(tab_count(), static_cast<int>(tab_bounds.size()));
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->SetBoundsRect(tab_bounds[i]);
  // Reset the layout size as we've effectively layed out a different size.
  // This ensures a layout happens after the drag is done.
  last_layout_size_ = gfx::Size();
}

void TabStrip::AddMessageLoopObserver() {
  if (!mouse_watcher_) {
    constexpr int kTabStripAnimationVSlop = 40;
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<views::MouseWatcherViewHost>(
            this, gfx::Insets(0, 0, kTabStripAnimationVSlop, 0)),
        this);
  }
  mouse_watcher_->Start(GetWidget()->GetNativeWindow());
}

void TabStrip::RemoveMessageLoopObserver() {
  mouse_watcher_ = nullptr;
}

gfx::Rect TabStrip::GetDropBounds(int drop_index,
                                  bool drop_before,
                                  bool* is_beneath) {
  DCHECK_NE(drop_index, -1);

  Tab* tab = tab_at(std::min(drop_index, tab_count() - 1));
  int center_x = tab->x();
  const int width = tab->width();
  const int overlap = TabStyle::GetTabOverlap();
  if (drop_index < tab_count())
    center_x += drop_before ? (overlap / 2) : (width / 2);
  else
    center_x += width - (overlap / 2);

  // Mirror the center point if necessary.
  center_x = GetMirroredXInView(center_x);

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - g_drop_indicator_width / 2,
                      -g_drop_indicator_height);
  ConvertPointToScreen(this, &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), g_drop_indicator_width,
                        g_drop_indicator_height);

  // If the rect doesn't fit on the monitor, push the arrow to the bottom.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetDisplayMatching(drop_bounds);
  *is_beneath = !display.bounds().Contains(drop_bounds);
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());

  return drop_bounds;
}

void TabStrip::SetDropArrow(
    const base::Optional<BrowserRootView::DropIndex>& index) {
  if (!index) {
    controller_->OnDropIndexUpdate(-1, false);
    drop_arrow_.reset();
    return;
  }

  // Let the controller know of the index update.
  controller_->OnDropIndexUpdate(index->value, index->drop_before);

  if (drop_arrow_ && (index == drop_arrow_->index))
    return;

  bool is_beneath;
  gfx::Rect drop_bounds =
      GetDropBounds(index->value, index->drop_before, &is_beneath);

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(*index, !is_beneath, GetWidget());
  } else {
    drop_arrow_->index = *index;
    if (is_beneath == drop_arrow_->point_down) {
      drop_arrow_->point_down = !is_beneath;
      drop_arrow_->arrow_view->SetImage(
          GetDropArrowImage(drop_arrow_->point_down));
    }
  }

  // Reposition the window. Need to show it too as the window is initially
  // hidden.
  drop_arrow_->arrow_window->SetBounds(drop_bounds);
  drop_arrow_->arrow_window->Show();
}

// static
gfx::ImageSkia* TabStrip::GetDropArrowImage(bool is_down) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStrip:DropArrow:
// ----------------------------------------------------------

TabStrip::DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                               bool point_down,
                               views::Widget* context)
    : index(index), point_down(point_down) {
  arrow_view = new views::ImageView;
  arrow_view->SetImage(GetDropArrowImage(point_down));

  arrow_window = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.keep_on_top = true;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.bounds = gfx::Rect(g_drop_indicator_width, g_drop_indicator_height);
  params.context = context->GetNativeWindow();
  arrow_window->Init(params);
  arrow_window->SetContentsView(arrow_view);
}

TabStrip::DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  arrow_window->Close();
}

///////////////////////////////////////////////////////////////////////////////

void TabStrip::PrepareForAnimation() {
  if (!IsDragSessionActive() && !TabDragController::IsAttachedTo(this)) {
    for (int i = 0; i < tab_count(); ++i)
      tab_at(i)->set_dragging(false);
  }
}

void TabStrip::GenerateIdealBounds() {
  if (tab_count() == 0)
    return;  // Should only happen during creation/destruction, ignore.

  const int old_max_x = GetTabsMaxX();

  if (!touch_layout_) {
    const int available_width = (available_width_for_tabs_ < 0)
                                    ? GetTabAreaWidth()
                                    : available_width_for_tabs_;
    const std::vector<gfx::Rect> tabs_bounds = CalculateBounds(
        GetTabSizeInfo(), GetPinnedTabCount(), tab_count(),
        controller_->GetActiveIndex(), TabStartX(), available_width,
        &current_active_width_, &current_inactive_width_);
    DCHECK_EQ(static_cast<size_t>(tab_count()), tabs_bounds.size());

    for (size_t i = 0; i < tabs_bounds.size(); ++i)
      tabs_.set_ideal_bounds(i, tabs_bounds[i]);
  }

  new_tab_button_bounds_.set_origin(gfx::Point(NewTabButtonIdealX(), 0));

  if (GetTabsMaxX() != old_max_x) {
    for (TabStripObserver& observer : observers_)
      observer.OnTabsMaxXChanged();
  }
}

int TabStrip::GenerateIdealBoundsForPinnedTabs(int* first_non_pinned_index) {
  const int num_pinned_tabs = GetPinnedTabCount();

  if (first_non_pinned_index)
    *first_non_pinned_index = num_pinned_tabs;

  const int start_x = TabStartX();
  if (num_pinned_tabs == 0)
    return start_x;

  std::vector<gfx::Rect> tab_bounds(tab_count());
  int non_pinned_x = CalculateBoundsForPinnedTabs(
      GetTabSizeInfo(), num_pinned_tabs, tab_count(), start_x, &tab_bounds);
  for (int i = 0; i < num_pinned_tabs; ++i)
    tabs_.set_ideal_bounds(i, tab_bounds[i]);
  return non_pinned_x;
}

int TabStrip::GetTabAreaWidth() const {
  return width() - GetFrameGrabWidth() - new_tab_button_bounds_.width() -
         TabToFollowingNewTabButtonSpacing();
}

void TabStrip::StartResizeLayoutAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStrip::StartPinnedTabAnimation() {
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  PrepareForAnimation();

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

bool TabStrip::IsPointInTab(Tab* tab,
                            const gfx::Point& point_in_tabstrip_coords) {
  if (!tab->visible())
    return false;
  gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
  View::ConvertPointToTarget(this, tab, &point_in_tab_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

Tab* TabStrip::FindTabForEvent(const gfx::Point& point) {
  DCHECK(touch_layout_);
  int active_tab_index = touch_layout_->active_index();
  Tab* tab = FindTabForEventFrom(point, active_tab_index, -1);
  return tab ? tab : FindTabForEventFrom(point, active_tab_index + 1, 1);
}

Tab* TabStrip::FindTabForEventFrom(const gfx::Point& point,
                                   int start,
                                   int delta) {
  // |start| equals tab_count() when there are only pinned tabs.
  if (start == tab_count())
    start += delta;
  for (int i = start; i >= 0 && i < tab_count(); i += delta) {
    if (IsPointInTab(tab_at(i), point))
      return tab_at(i);
  }
  return nullptr;
}

Tab* TabStrip::FindTabHitByPoint(const gfx::Point& point) {
  // The display order doesn't necessarily match the child order, so we iterate
  // in display order.
  for (int i = 0; i < tab_count(); ++i) {
    // If we don't first exclude points outside the current tab, the code below
    // will return the wrong tab if the next tab is selected, the following tab
    // is active, and |point| is in the overlap region between the two.
    Tab* tab = tab_at(i);
    if (!IsPointInTab(tab, point))
      continue;

    // Selected tabs render atop unselected ones, and active tabs render atop
    // everything.  Check whether the next tab renders atop this one and |point|
    // is in the overlap region.
    Tab* next_tab = i < (tab_count() - 1) ? tab_at(i + 1) : nullptr;
    if (next_tab &&
        (next_tab->IsActive() ||
         (next_tab->IsSelected() && !tab->IsSelected())) &&
        IsPointInTab(next_tab, point))
      return next_tab;

    // This is the topmost tab for this point.
    return tab;
  }

  return nullptr;
}

std::vector<int> TabStrip::GetTabXCoordinates() {
  std::vector<int> results;
  for (int i = 0; i < tab_count(); ++i)
    results.push_back(ideal_bounds(i).x());
  return results;
}

void TabStrip::SwapLayoutIfNecessary() {
  bool needs_touch = NeedsTouchLayout();
  bool using_touch = touch_layout_ != nullptr;
  if (needs_touch == using_touch)
    return;

  if (needs_touch) {
    const int overlap = TabStyle::GetTabOverlap();
    touch_layout_.reset(new StackedTabStripLayout(
        gfx::Size(GetStackableTabWidth(), GetLayoutConstant(TAB_HEIGHT)),
        overlap, kStackedPadding, kMaxStackedCount, &tabs_));
    touch_layout_->SetWidth(GetTabAreaWidth());
    // This has to be after SetWidth() as SetWidth() is going to reset the
    // bounds of the pinned tabs (since StackedTabStripLayout doesn't yet know
    // how many pinned tabs there are).
    touch_layout_->SetXAndPinnedCount(GenerateIdealBoundsForPinnedTabs(nullptr),
                                      GetPinnedTabCount());
    touch_layout_->SetActiveIndex(controller_->GetActiveIndex());

    base::RecordAction(
        base::UserMetricsAction("StackedTab_EnteredStackedLayout"));
  } else {
    touch_layout_.reset();
  }
  PrepareForAnimation();
  GenerateIdealBounds();
  SetTabVisibility();
  AnimateToIdealBounds();
}

bool TabStrip::NeedsTouchLayout() const {
  if (!stacked_layout_)
    return false;

  const int pinned_tab_count = GetPinnedTabCount();
  const int normal_count = tab_count() - pinned_tab_count;
  if (normal_count <= 1 || normal_count == pinned_tab_count)
    return false;

  const int tab_overlap = TabStyle::GetTabOverlap();
  const int normal_width =
      GetStackableTabWidth() * normal_count - tab_overlap * (normal_count - 1);
  const int available_width = GetTabAreaWidth();
  const int pinned_width =
      pinned_tab_count
          ? (pinned_tab_count * TabStyle::GetPinnedWidth() - tab_overlap)
          : 0;
  return normal_width > (available_width - pinned_width - TabStartX());
}

void TabStrip::SetResetToShrinkOnExit(bool value) {
  if (!adjust_layout_)
    return;

  // We have to be using stacked layout to reset out of it.
  value &= stacked_layout_;

  if (value == reset_to_shrink_on_exit_)
    return;

  reset_to_shrink_on_exit_ = value;
  // Add an observer so we know when the mouse moves out of the tabstrip.
  if (reset_to_shrink_on_exit_)
    AddMessageLoopObserver();
  else
    RemoveMessageLoopObserver();
}

void TabStrip::UpdateNewTabButtonBorder() {
  // The button is placed vertically exactly in the center of the tabstrip.
  const int extra_vertical_space = GetLayoutConstant(TAB_HEIGHT) -
                                   GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) -
                                   NewTabButton::kButtonSize.height();
  constexpr int kHorizontalInset = 8;
  new_tab_button_->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      extra_vertical_space / 2, kHorizontalInset, 0, kHorizontalInset)));
}

void TabStrip::SingleTabModeChanged() {
  const int active_tab_index = controller_->GetActiveIndex();
  if (IsValidModelIndex(active_tab_index))
    tab_at(active_tab_index)->Layout();
  FrameColorsChanged();
  for (TabStripObserver& observer : observers_)
    observer.OnSingleTabModeChanged();
}

void TabStrip::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == new_tab_button_) {
    base::RecordAction(base::UserMetricsAction("NewTab_Button"));
    UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", TabStripModel::NEW_TAB_BUTTON,
                              TabStripModel::NEW_TAB_ENUM_COUNT);
    if (event.IsMouseEvent()) {
      const ui::MouseEvent& mouse = static_cast<const ui::MouseEvent&>(event);
      if (mouse.IsOnlyMiddleMouseButton()) {
        if (ui::Clipboard::IsSupportedClipboardType(
                ui::CLIPBOARD_TYPE_SELECTION)) {
          ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
          CHECK(clipboard);
          base::string16 clipboard_text;
          clipboard->ReadText(ui::CLIPBOARD_TYPE_SELECTION, &clipboard_text);
          if (!clipboard_text.empty())
            controller_->CreateNewTabWithLocation(clipboard_text);
        }
        return;
      }
    }

    controller_->CreateNewTab();
    if (event.type() == ui::ET_GESTURE_TAP)
      TouchUMA::RecordGestureAction(TouchUMA::kGestureNewTabTap);
  }
}

// Overridden to support automation. See automation_proxy_uitest.cc.
const views::View* TabStrip::GetViewByID(int view_id) const {
  if (tab_count() > 0) {
    if (view_id == VIEW_ID_TAB_LAST)
      return tab_at(tab_count() - 1);
    if ((view_id >= VIEW_ID_TAB_0) && (view_id < VIEW_ID_TAB_LAST)) {
      int index = view_id - VIEW_ID_TAB_0;
      return (index >= 0 && index < tab_count()) ? tab_at(index) : nullptr;
    }
  }

  return View::GetViewByID(view_id);
}

bool TabStrip::OnMousePressed(const ui::MouseEvent& event) {
  UpdateStackedLayoutFromMouseEvent(this, event);
  // We can't return true here, else clicking in an empty area won't drag the
  // window.
  return false;
}

bool TabStrip::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueDrag(this, event);
  return true;
}

void TabStrip::OnMouseReleased(const ui::MouseEvent& event) {
  EndDrag(END_DRAG_COMPLETE);
  UpdateStackedLayoutFromMouseEvent(this, event);
}

void TabStrip::OnMouseCaptureLost() {
  EndDrag(END_DRAG_CAPTURE_LOST);
}

void TabStrip::OnMouseMoved(const ui::MouseEvent& event) {
  UpdateStackedLayoutFromMouseEvent(this, event);
}

void TabStrip::OnMouseEntered(const ui::MouseEvent& event) {
  SetResetToShrinkOnExit(true);
}

void TabStrip::OnGestureEvent(ui::GestureEvent* event) {
  SetResetToShrinkOnExit(false);
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_END:
      EndDrag(END_DRAG_COMPLETE);
      if (adjust_layout_) {
        SetStackedLayout(true);
        controller_->StackedLayoutMaybeChanged();
      }
      break;

    case ui::ET_GESTURE_LONG_PRESS:
      if (drag_controller_.get())
        drag_controller_->SetMoveBehavior(TabDragController::REORDER);
      break;

    case ui::ET_GESTURE_LONG_TAP: {
      EndDrag(END_DRAG_CANCEL);
      gfx::Point local_point = event->location();
      Tab* tab = touch_layout_ ? FindTabForEvent(local_point)
                               : FindTabHitByPoint(local_point);
      if (tab) {
        ConvertPointToScreen(this, &local_point);
        ShowContextMenuForTab(tab, local_point, ui::MENU_SOURCE_TOUCH);
      }
      break;
    }

    case ui::ET_GESTURE_SCROLL_UPDATE:
      ContinueDrag(this, *event);
      break;

    case ui::ET_GESTURE_TAP_DOWN:
      EndDrag(END_DRAG_CANCEL);
      break;

    case ui::ET_GESTURE_TAP: {
      const int active_index = controller_->GetActiveIndex();
      DCHECK_NE(-1, active_index);
      Tab* active_tab = tab_at(active_index);
      TouchUMA::GestureActionType action = TouchUMA::kGestureTabNoSwitchTap;
      if (active_tab->tab_activated_with_last_tap_down())
        action = TouchUMA::kGestureTabSwitchTap;
      TouchUMA::RecordGestureAction(action);
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

views::View* TabStrip::TargetForRect(views::View* root, const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  const gfx::Point point(rect.CenterPoint());

  if (!touch_layout_) {
    // Return any view that isn't a Tab or this TabStrip immediately. We don't
    // want to interfere.
    views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
    if (v && v != this && strcmp(v->GetClassName(), Tab::kViewClassName))
      return v;

    views::View* tab = FindTabHitByPoint(point);
    if (tab)
      return tab;
  } else {
    if (new_tab_button_->visible()) {
      views::View* view =
          ConvertPointToViewAndGetEventHandler(this, new_tab_button_, point);
      if (view)
        return view;
    }
    Tab* tab = FindTabForEvent(point);
    if (tab)
      return ConvertPointToViewAndGetEventHandler(this, tab, point);
  }
  return this;
}

void TabStrip::OnTouchUiChanged() {
  UpdateNewTabButtonBorder();
  new_tab_button_bounds_.set_size(new_tab_button_->GetPreferredSize());
  new_tab_button_->SetBoundsRect(new_tab_button_bounds_);
  StopAnimating(true);
}
