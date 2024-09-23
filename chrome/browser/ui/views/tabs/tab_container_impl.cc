// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container_impl.h"

#include <memory>

#include "base/bits.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_scrolling_animation.h"
#include "chrome/browser/ui/views/tabs/tab_slot_animation_delegate.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/tabs/z_orderable_tab_container_element.h"
#include "chrome/grit/theme_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_utils.h"

namespace {

// Size of the drop indicator.
int g_drop_indicator_width = 0;
int g_drop_indicator_height = 0;

int GetDropArrowImageResourceId(bool is_down) {
  return is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabContainerImpl::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabContainerImpl::RemoveTabDelegate : public TabSlotAnimationDelegate {
 public:
  RemoveTabDelegate(TabContainer* tab_container, Tab* tab);
  RemoveTabDelegate(const RemoveTabDelegate&) = delete;
  RemoveTabDelegate& operator=(const RemoveTabDelegate&) = delete;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
};

TabContainerImpl::RemoveTabDelegate::RemoveTabDelegate(
    TabContainer* tab_container,
    Tab* tab)
    : TabSlotAnimationDelegate(tab_container, tab) {}

void TabContainerImpl::RemoveTabDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  tab_container()->OnTabCloseAnimationCompleted(static_cast<Tab*>(slot_view()));
}

void TabContainerImpl::RemoveTabDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

TabContainerImpl::TabContainerImpl(
    TabContainerController& controller,
    TabHoverCardController* hover_card_controller,
    TabDragContextBase* drag_context,
    TabSlotController& tab_slot_controller,
    views::View* scroll_contents_view)
    : controller_(controller),
      hover_card_controller_(hover_card_controller),
      drag_context_(drag_context),
      tab_slot_controller_(tab_slot_controller),
      scroll_contents_view_(scroll_contents_view),
      overall_bounds_view_(*AddChildView(std::make_unique<views::View>())),
      bounds_animator_(this),
      layout_helper_(std::make_unique<TabStripLayoutHelper>(
          controller,
          base::BindRepeating(&TabContainerImpl::GetTabsViewModel,
                              base::Unretained(this)))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    bounds_animator_.SetAnimationDuration(base::TimeDelta());
  }

  bounds_animator_.AddObserver(this);

  overall_bounds_view_->SetVisible(false);

  if (g_drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetDropArrowImageResourceId(true));
    g_drop_indicator_width = drop_image->width();
    g_drop_indicator_height = drop_image->height();
  }
}

TabContainerImpl::~TabContainerImpl() {
  // The animations may reference the tabs or group views. Shut down the
  // animation before we destroy any animated views.
  CancelAnimation();

  // Since TabGroupViews expects be able to remove the views it creates, clear
  // |group_views_| before removing the remaining children below.
  group_views_.clear();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  RemoveAllChildViews();
}

void TabContainerImpl::SetAvailableWidthCallback(
    base::RepeatingCallback<int()> available_width_callback) {
  available_width_callback_ = available_width_callback;
}

Tab* TabContainerImpl::AddTab(std::unique_ptr<Tab> tab,
                              int model_index,
                              TabPinned pinned) {
  // First add the tab to the view model, this is done because AddChildView sets
  // some tooltip information which tries to calculate the hit test, which needs
  // information about its adjacent tabs which it gets from the view model.
  AddTabToViewModel(tab.get(), model_index, pinned);
  Tab* tab_ptr = AddChildView(std::move(tab));
  OrderTabSlotView(tab_ptr);

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (GetTabCount() > 1 && GetWidget() && GetWidget()->IsVisible()) {
    StartInsertTabAnimation(model_index);
  } else {
    CompleteAnimationAndLayout();
  }

  return tab_ptr;
}

void TabContainerImpl::MoveTab(int from_model_index, int to_model_index) {
  Tab* tab = GetTabAtModelIndex(from_model_index);
  tabs_view_model_.Move(from_model_index, to_model_index);
  layout_helper_->MoveTab(tab->group(), from_model_index, to_model_index);
  OrderTabSlotView(tab);

  layout_helper_->SetTabPinned(to_model_index, tab->data().pinned
                                                   ? TabPinned::kPinned
                                                   : TabPinned::kUnpinned);

  AnimateToIdealBounds();

  UpdateAccessibleTabIndices();
}

void TabContainerImpl::RemoveTab(int model_index, bool was_active) {
  UpdateClosingModeOnRemovedTab(model_index, was_active);

  Tab* const tab = GetTabAtModelIndex(model_index);
  tab->SetClosing(true);

  CloseTabInViewModel(model_index);

  StartRemoveTabAnimation(tab, model_index);

  UpdateAccessibleTabIndices();
}

void TabContainerImpl::SetTabPinned(int model_index, TabPinned pinned) {
  layout_helper_->SetTabPinned(model_index, pinned);

  if (GetWidget() && GetWidget()->IsVisible()) {
    ExitTabClosingMode();

    AnimateToIdealBounds();
  } else {
    CompleteAnimationAndLayout();
  }
}

void TabContainerImpl::SetActiveTab(std::optional<size_t> prev_active_index,
                                    std::optional<size_t> new_active_index) {
  auto maybe_update_group_visuals = [this](std::optional<size_t> tab_index) {
    if (!tab_index.has_value()) {
      return;
    }
    std::optional<tab_groups::TabGroupId> group =
        GetTabAtModelIndex(tab_index.value())->group();
    if (group.has_value()) {
      UpdateTabGroupVisuals(group.value());
    }
  };

  maybe_update_group_visuals(prev_active_index);
  maybe_update_group_visuals(new_active_index);

  layout_helper_->SetActiveTab(prev_active_index, new_active_index);

  if (GetActiveTabWidth() == GetInactiveTabWidth()) {
    // When tabs are wide enough, selecting a new tab cannot change the
    // ideal bounds, so only a repaint is necessary.
    SchedulePaint();
  } else if (controller_->IsAnimatingInTabStrip() ||
             drag_context_->IsDragSessionActive()) {
    // The selection change will have modified the ideal bounds of the tabs. We
    // need to recompute and retarget the animation to these new bounds. Note:
    // This is safe even if we're in the midst of mouse-based tab closure--we
    // won't expand the tabstrip back to the full window width--because
    // PrepareForCloseAt() will have set `override_available_width_for_tabs_`
    // already.
    AnimateToIdealBounds();
  } else {
    // As in the animating case above, the selection change will have
    // affected the desired bounds of the tabs, but since we're in a steady
    // state we can just snap to the new bounds.
    CompleteAnimationAndLayout();
  }

  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip) &&
      new_active_index.has_value()) {
    ScrollTabToVisible(new_active_index.value());
  }
}

Tab* TabContainerImpl::RemoveTabFromViewModel(int model_index) {
  Tab* const tab = GetTabAtModelIndex(model_index);
  tabs_view_model_.Remove(model_index);
  OnTabRemoved(tab);

  return tab;
}

Tab* TabContainerImpl::AddTabToViewModel(Tab* tab,
                                         int model_index,
                                         TabPinned pinned) {
  tabs_view_model_.Add(tab, model_index);
  layout_helper_->InsertTabAt(model_index, tab, pinned);
  UpdateAccessibleTabIndices();

  return tab;
}

void TabContainerImpl::ReturnTabSlotView(TabSlotView* view) {
  // Take `view` back now that it's done dragging or pinning.

  // If `view` has no parent (vs the expected case where its parent is a
  // TabDragContext or CompoundTabContainer), then it's been removed from the
  // View hierarchy as part of deletion, triggering animations to end, which in
  // turn will bring us here if tabs are being dragged or are pinning. We need
  // to update our data structures accordingly and otherwise not interfere.
  if (!view->parent()) {
    Tab* tab = views::AsViewClass<Tab>(view);
    if (tab) {
      DCHECK(tab->closing());
      OnTabRemoved(tab);
    }
    return;
  }

  const gfx::Rect bounds_in_tab_container_coords = gfx::ToEnclosingRect(
      ConvertRectToTarget(view->parent(), this, gfx::RectF(view->bounds())));
  AddChildView(view);
  view->SetBoundsRect(bounds_in_tab_container_coords);

  Tab* tab = views::AsViewClass<Tab>(view);
  if (tab && tab->closing()) {
    // This tab was closed during the drag. It's already been removed from our
    // other data structures in RemoveTab(), and TabDragContext animated it
    // closed for us, so we can just destroy it.
    OnTabCloseAnimationCompleted(tab);
    return;
  }

  OrderTabSlotView(view);

  if (view->group()) {
    UpdateTabGroupVisuals(view->group().value());
  }
}

void TabContainerImpl::ScrollTabToVisible(int model_index) {
  std::optional<gfx::Rect> visible_content_rect = GetVisibleContentRect();

  if (!visible_content_rect.has_value()) {
    return;
  }

  // If the tab strip won't be scrollable after the current tabstrip animations
  // complete, scroll animation wouldn't be meaningful.
  if (tabs_view_model_.ideal_bounds(GetTabCount() - 1).right() <=
      GetAvailableWidthForTabContainer()) {
    return;
  }

  gfx::Rect active_tab_ideal_bounds =
      tabs_view_model_.ideal_bounds(model_index);

  if ((active_tab_ideal_bounds.x() >= visible_content_rect->x()) &&
      (active_tab_ideal_bounds.right() <= visible_content_rect->right())) {
    return;
  }

  bool scroll_left = active_tab_ideal_bounds.x() < visible_content_rect->x();
  if (scroll_left) {
    // Scroll the left edge of |visible_content_rect| to show the left edge of
    // the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    int start_left_edge(visible_content_rect->x());
    int target_left_edge(active_tab_ideal_bounds.x());

    AnimateScrollToShowXCoordinate(start_left_edge, target_left_edge);
  } else {
    // Scroll the right edge of |visible_content_rect| to show the right edge
    // of the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    int start_right_edge(visible_content_rect->right());
    int target_right_edge(active_tab_ideal_bounds.right());
    AnimateScrollToShowXCoordinate(start_right_edge, target_right_edge);
  }
}

void TabContainerImpl::ScrollTabContainerByOffset(int offset) {
  std::optional<gfx::Rect> visible_content_rect = GetVisibleContentRect();
  if (!visible_content_rect.has_value() || offset == 0) {
    return;
  }

  // If tabcontainer is scrolled towards trailing tab, the start edge should
  // have the x coordinate of the right bound. If it is scrolled towards the
  // leading tab it should have the x coordinate of the left bound.
  int start_edge =
      (offset > 0) ? visible_content_rect->right() : visible_content_rect->x();

  AnimateScrollToShowXCoordinate(start_edge, start_edge + offset);
}

void TabContainerImpl::OnGroupCreated(const tab_groups::TabGroupId& group) {
  auto group_view = std::make_unique<TabGroupViews>(
      this, drag_context_, *tab_slot_controller_, group);
  layout_helper_->InsertGroupHeader(group, group_view->header());
  group_views_[group] = std::move(group_view);
}

void TabContainerImpl::OnGroupEditorOpened(
    const tab_groups::TabGroupId& group) {
  // The context menu relies on a Browser object which is not provided in
  // TabStripTest.
  if (tab_slot_controller_->GetBrowser()) {
    group_views_[group]->header()->ShowContextMenuForViewImpl(
        this, gfx::Point(), ui::MENU_SOURCE_NONE);
  }
}

void TabContainerImpl::OnGroupContentsChanged(
    const tab_groups::TabGroupId& group) {
  // If a tab was removed, the underline bounds might be stale.
  group_views_[group]->UpdateBounds();

  // The group header may be in the wrong place if the tab didn't actually
  // move in terms of model indices.
  OnGroupMoved(group);
  AnimateToIdealBounds();
}

void TabContainerImpl::OnGroupVisualsChanged(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData* old_visuals,
    const tab_groups::TabGroupVisualData* new_visuals) {
  GetGroupViews(group)->OnGroupVisualsChanged();
  // The group title may have changed size, so update bounds.
  // First exit tab closing mode, unless this change was a collapse, in which
  // case we want to stay in tab closing mode.
  const bool is_collapsing = old_visuals && !old_visuals->is_collapsed() &&
                             new_visuals->is_collapsed();
  if (!is_collapsing) {
    ExitTabClosingMode();
  }
  AnimateToIdealBounds();

  // The active tab may need to repaint its group stroke if it's in `group`.
  const std::optional<int> active_index = controller_->GetActiveIndex();
  if (active_index.has_value()) {
    GetTabAtModelIndex(active_index.value())->SchedulePaint();
  }
}

void TabContainerImpl::OnGroupMoved(const tab_groups::TabGroupId& group) {
  DCHECK(group_views_[group]);

  layout_helper_->UpdateGroupHeaderIndex(group);

  OrderTabSlotView(group_views_[group]->header());
}

void TabContainerImpl::ToggleTabGroup(
    const tab_groups::TabGroupId& group,
    bool is_collapsing,
    ToggleTabGroupCollapsedStateOrigin origin) {
  if (is_collapsing && GetWidget()) {
    if (origin != ToggleTabGroupCollapsedStateOrigin::kMouse &&
        origin != ToggleTabGroupCollapsedStateOrigin::kGesture) {
      return;
    }

    const int current_group_width = GetGroupViews(group)->GetBounds().width();
    // A collapsed group only has the width of its header, which is slightly
    // smaller for collapsed groups compared to expanded groups.
    const int collapsed_group_width =
        GetGroupViews(group)->header()->GetCollapsedHeaderWidth();
    const CloseTabSource source =
        origin == ToggleTabGroupCollapsedStateOrigin::kMouse
            ? CloseTabSource::CLOSE_TAB_FROM_MOUSE
            : CloseTabSource::CLOSE_TAB_FROM_TOUCH;

    EnterTabClosingMode(
        tabs_view_model_.ideal_bounds(GetTabCount() - 1).right() -
            current_group_width + collapsed_group_width,
        source);
  } else {
    ExitTabClosingMode();
  }
}

void TabContainerImpl::OnGroupClosed(const tab_groups::TabGroupId& group) {
  bounds_animator_.StopAnimatingView(group_views_.at(group).get()->header());
  layout_helper_->RemoveGroupHeader(group);
  group_views_.erase(group);

  AnimateToIdealBounds();
}

void TabContainerImpl::UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) {
  const auto group_views = group_views_.find(group_id);
  if (group_views != group_views_.end()) {
    group_views->second->UpdateBounds();
  }
}

void TabContainerImpl::NotifyTabGroupEditorBubbleOpened() {
  // Suppress the mouse watching behavior of tab closing mode.
  RemoveMessageLoopObserver();
}

void TabContainerImpl::NotifyTabGroupEditorBubbleClosed() {
  // Restore the mouse watching behavior of tab closing mode.
  if (in_tab_close_) {
    AddMessageLoopObserver();
  }
}

// TODO(tbergquist): This should really return an optional<size_t>.
std::optional<int> TabContainerImpl::GetModelIndexOf(
    const TabSlotView* slot_view) const {
  return tabs_view_model_.GetIndexOfView(slot_view);
}

Tab* TabContainerImpl::GetTabAtModelIndex(int index) const {
  return tabs_view_model_.view_at(index);
}

int TabContainerImpl::GetTabCount() const {
  return tabs_view_model_.view_size();
}

// TODO(tbergquist): This should really return an optional<size_t>.
std::optional<int> TabContainerImpl::GetModelIndexOfFirstNonClosingTab(
    Tab* tab) const {
  if (tab->closing()) {
    // If the tab is already closing, close the next tab. We do this so that the
    // user can rapidly close tabs by clicking the close button and not have
    // the animations interfere with that.
    std::vector<Tab*> all_tabs = layout_helper_->GetTabs();
    auto it = base::ranges::find(all_tabs, tab);
    while (it < all_tabs.end() && (*it)->closing()) {
      it++;
    }

    if (it == all_tabs.end()) {
      return std::nullopt;
    }
    tab = *it;
  }

  return GetModelIndexOf(tab);
}

void TabContainerImpl::UpdateHoverCard(
    Tab* tab,
    TabSlotController::HoverCardUpdateType update_type) {
  // Some operations (including e.g. starting a drag) can cause the tab focus
  // to change at the same time as the tabstrip is starting to animate; the
  // hover card should not be visible at this time.
  // See crbug.com/1220840 for an example case.
  if (controller_->IsAnimatingInTabStrip()) {
    tab = nullptr;
    update_type = TabSlotController::HoverCardUpdateType::kAnimating;
  }

  if (!hover_card_controller_) {
    return;
  }

  hover_card_controller_->UpdateHoverCard(tab, update_type);
}

void TabContainerImpl::HandleLongTap(ui::GestureEvent* event) {
  event->target()->ConvertEventToTarget(this, event);
  gfx::Point local_point = event->location();
  Tab* tab = FindTabHitByPoint(local_point);
  if (tab) {
    ConvertPointToScreen(this, &local_point);
    tab_slot_controller_->ShowContextMenuForTab(tab, local_point,
                                                ui::MENU_SOURCE_TOUCH);
  }
}

bool TabContainerImpl::IsRectInContentArea(const gfx::Rect& rect) {
  // If there is no control at this location, the hit is in the caption area.
  const views::View* v = GetEventHandlerForRect(rect);
  if (v == this) {
    return false;
  }

  if (controller_->CanExtendDragHandle()) {
    // When the window has a top drag handle, a thin strip at the top of
    // inactive tabs and the new tab button can be treated as part of the window
    // drag handle, to increase draggability.  This region starts 1 DIP above
    // the top of the separator.
    const int drag_handle_extension =
        TabStyle::Get()->GetDragHandleExtension(height());

    // A hit on an inactive tab is in the content area unless it is in the thin
    // strip mentioned above.
    const std::optional<size_t> tab_index = tabs_view_model_.GetIndexOfView(v);
    if (tab_index.has_value() && IsValidModelIndex(tab_index.value())) {
      Tab* tab = GetTabAtModelIndex(tab_index.value());
      gfx::Rect tab_drag_handle = tab->GetMirroredBounds();
      tab_drag_handle.set_height(drag_handle_extension);
      return tab->IsActive() || !tab_drag_handle.Intersects(rect);
    }
  }

  // |v| is some other view (e.g. a close button in a tab) and therefore |rect|
  // is in client area.
  return true;
}

std::optional<ZOrderableTabContainerElement>
TabContainerImpl::GetLeadingElementForZOrdering() const {
  // Use `tabs_view_model_` instead of `layout_helper_` to ignore closing tabs
  // to prevent discontinuous z-order flips when tab close animations end.
  if (GetTabCount() == 0) {
    return std::nullopt;
  }
  Tab* const leading_tab = tabs_view_model_.view_at(0);

  // If `leading_tab` is grouped, it's preceded by its group header.
  if (leading_tab->group().has_value()) {
    return ZOrderableTabContainerElement(
        group_views_.at(leading_tab->group().value())->header());
  }

  return ZOrderableTabContainerElement(leading_tab);
}

std::optional<ZOrderableTabContainerElement>
TabContainerImpl::GetTrailingElementForZOrdering() const {
  // Use `tabs_view_model_` instead of `layout_helper_` to ignore closing tabs
  // to prevent discontinuous z-order flips when tab close animations end.
  if (GetTabCount() == 0) {
    return std::nullopt;
  }

  Tab* const trailing_tab =
      tabs_view_model_.view_at(tabs_view_model_.view_size() - 1);

  // Tab group headers could be the trailing element, if the group is collapsed.
  // However, this method doesn't need to consider that case because it is
  // currently only called on the pinned TabContainer in a CompoundTabContainer,
  // which can't have tab groups. DCHECK that assumption:
  DCHECK(!trailing_tab->group().has_value());

  return ZOrderableTabContainerElement(trailing_tab);
}

void TabContainerImpl::OnTabSlotAnimationProgressed(TabSlotView* view) {
  if (view && view->group()) {
    UpdateTabGroupVisuals(view->group().value());
  }
}

void TabContainerImpl::InvalidateIdealBounds() {
  last_layout_size_ = gfx::Size();
}

void TabContainerImpl::AnimateToIdealBounds() {
  UpdateIdealBounds();
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kAnimating);

  for (int i = 0; i < GetTabCount(); ++i) {
    Tab* tab = GetTabAtModelIndex(i);
    const gfx::Rect& target_bounds = tabs_view_model_.ideal_bounds(i);

    AnimateTabSlotViewTo(tab, target_bounds);
  }

  for (const auto& header_pair : group_views_) {
    TabGroupHeader* const header = header_pair.second->header();
    const gfx::Rect& target_bounds =
        layout_helper_->group_header_ideal_bounds().at(header_pair.first);

    AnimateTabSlotViewTo(header, target_bounds);
  }

  const gfx::Rect overall_target_bounds = gfx::Rect(GetIdealTrailingX(), 0);
  bounds_animator_.AnimateViewTo(base::to_address(overall_bounds_view_),
                                 overall_target_bounds);

  // Because the preferred size of the tabstrip depends on the IsAnimating()
  // condition, but starting an animation doesn't necessarily invalidate the
  // existing preferred size and layout (which may now be incorrect), we need to
  // signal this explicitly.
  PreferredSizeChanged();
}

bool TabContainerImpl::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

void TabContainerImpl::CancelAnimation() {
  drag_context_->CompleteEndDragAnimations();
  bounds_animator_.Cancel();
}

void TabContainerImpl::CompleteAnimationAndLayout() {
  last_available_width_ = GetAvailableWidthForTabContainer();
  last_layout_size_ = size();

  CancelAnimation();

  UpdateIdealBounds();
  SnapToIdealBounds();

  SetTabSlotVisibility();
  SchedulePaint();
}

int TabContainerImpl::GetAvailableWidthForTabContainer() const {
  // Falls back to views::View::GetAvailableSize() when
  // |available_width_callback_| is not defined, e.g. when tab scrolling is
  // disabled.
  return available_width_callback_
             ? available_width_callback_.Run()
             : parent()->GetAvailableSize(this).width().value();
}

void TabContainerImpl::EnterTabClosingMode(std::optional<int> override_width,
                                           CloseTabSource source) {
  in_tab_close_ = true;
  if (override_width.has_value()) {
    override_available_width_for_tabs_ = override_width;
  }

  // Default to freezing tabs in their current state if our caller doesn't have
  // a more specific plan.
  if (!override_available_width_for_tabs_.has_value()) {
    override_available_width_for_tabs_ = width();
  }

  resize_layout_timer_.Stop();
  if (source == CLOSE_TAB_FROM_TOUCH) {
    StartResizeLayoutTabsFromTouchTimer();
  } else {
    AddMessageLoopObserver();
  }
}

void TabContainerImpl::ExitTabClosingMode() {
  in_tab_close_ = false;
  override_available_width_for_tabs_.reset();
}

void TabContainerImpl::SetTabSlotVisibility() {
  std::set<tab_groups::TabGroupId> visibility_changed_groups;
  bool last_tab_visible = false;
  std::optional<tab_groups::TabGroupId> last_tab_group = std::nullopt;
  std::vector<Tab*> tabs = layout_helper_->GetTabs();
  for (Tab* tab : base::Reversed(tabs)) {
    std::optional<tab_groups::TabGroupId> current_group = tab->group();
    if (current_group != last_tab_group && last_tab_group.has_value()) {
      TabGroupViews* group_view = group_views_.at(last_tab_group.value()).get();

      // If we change the visibility of a group header, we must recalculate that
      // group's underline bounds.
      if (last_tab_visible != group_view->header()->GetVisible()) {
        visibility_changed_groups.insert(last_tab_group.value());
      }

      group_view->header()->SetVisible(last_tab_visible);
      // Hide underlines if they would underline an invisible tab, but don't
      // show underlines if they're hidden during a header drag session.
      if (!group_view->header()->dragging()) {
        group_view->underline()->MaybeSetVisible(last_tab_visible);
      }
    }
    last_tab_visible = ShouldTabBeVisible(tab);
    last_tab_group = tab->closing() ? std::nullopt : current_group;

    // Collapsed tabs disappear once they've reached their minimum size. This
    // is different than very small non-collapsed tabs, because in that case
    // the tab (and its favicon) must still be visible.
    const bool is_collapsed =
        (current_group.has_value() &&
         controller_->IsGroupCollapsed(current_group.value()) &&
         tab->bounds().width() <= tab->tab_style()->GetTabOverlap());
    const bool should_be_visible = is_collapsed ? false : last_tab_visible;

    // If we change the visibility of a tab in a group, we must recalculate that
    // group's underline bounds.
    if (should_be_visible != tab->GetVisible() && tab->group().has_value()) {
      visibility_changed_groups.insert(tab->group().value());
    }

    tab->SetVisible(should_be_visible);
  }

  // Update bounds for any groups containing a modified tab. N.B. this method
  // also updates the title and color of the group, but this should always be a
  // no-op in practice, as changes to those immediately take effect via other
  // notification channels.
  for (const auto& group : visibility_changed_groups) {
    UpdateTabGroupVisuals(group);
  }
}

bool TabContainerImpl::InTabClose() {
  return in_tab_close_;
}

TabGroupViews* TabContainerImpl::GetGroupViews(
    tab_groups::TabGroupId group_id) const {
  auto group_views = group_views_.find(group_id);
  CHECK(group_views != group_views_.end());
  return group_views->second.get();
}

const std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
TabContainerImpl::get_group_views_for_testing() const {
  return group_views_;
}

int TabContainerImpl::GetActiveTabWidth() const {
  return layout_helper_->active_tab_width();
}

int TabContainerImpl::GetInactiveTabWidth() const {
  return layout_helper_->inactive_tab_width();
}

gfx::Rect TabContainerImpl::GetIdealBounds(int model_index) const {
  return tabs_view_model_.ideal_bounds(model_index);
}

gfx::Rect TabContainerImpl::GetIdealBounds(tab_groups::TabGroupId group) const {
  return layout_helper_->group_header_ideal_bounds().at(group);
}

void TabContainerImpl::Layout(PassKey) {
  if (controller_->IsAnimatingInTabStrip()) {
    // Hide tabs that have animated at least partially out of the clip region.
    SetTabSlotVisibility();
    return;
  }

  // Only do a layout if our size or the available width changed.
  const int available_width = GetAvailableWidthForTabContainer();
  if (last_layout_size_ == size() && last_available_width_ == available_width) {
    return;
  }
  if (IsDragSessionActive()) {
    return;
  }
  CompleteAnimationAndLayout();
}

void TabContainerImpl::PaintChildren(const views::PaintInfo& paint_info) {
  // N.B. We override PaintChildren only to define paint order for our children.
  // We do this instead of GetChildrenInZOrder because GetChildrenInZOrder is
  // called in many more contexts for many more reasons, e.g. whenever views are
  // added or removed, and in particular can be called while we are partway
  // through creating a tab group and are not in a self-consistent state.

  std::vector<ZOrderableTabContainerElement> orderable_children;
  for (views::View* child : children()) {
    if (!ZOrderableTabContainerElement::CanOrderView(child)) {
      continue;
    }
    orderable_children.emplace_back(child);
  }

  // Sort in ascending order by z-value. Stable sort breaks ties by child index.
  std::stable_sort(orderable_children.begin(), orderable_children.end());

  for (const ZOrderableTabContainerElement& child : orderable_children) {
    child.view()->Paint(paint_info);
  }
}

gfx::Size TabContainerImpl::GetMinimumSize() const {
  // During animations, our minimum width tightly hugs the current bounds of our
  // children.
  std::optional<int> minimum_width = GetMidAnimationTrailingX();
  if (!minimum_width.has_value()) {
    // Otherwise, the tabstrip is in a steady state, so we want to use the width
    // that would be spanned by our children after animations complete. This
    // allows tabs to resize directly with window resizes instead of mediating
    // that through animation.
    minimum_width = override_available_width_for_tabs_.value_or(
        layout_helper_->CalculateMinimumWidth());
  }

  return gfx::Size(minimum_width.value(), GetLayoutConstant(TAB_STRIP_HEIGHT));
}

gfx::Size TabContainerImpl::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // During animations, our preferred width tightly hugs the current bounds of
  // our children.
  std::optional<int> preferred_width = GetMidAnimationTrailingX();
  if (!preferred_width.has_value()) {
    // Otherwise, the tabstrip is in a steady state, so we want to use the width
    // that would be spanned by our children after animations complete. This
    // allows tabs to resize directly with window resizes instead of mediating
    // that through animation.
    preferred_width = override_available_width_for_tabs_.value_or(
        layout_helper_->CalculatePreferredWidth());
  }

  return gfx::Size(preferred_width.value(),
                   GetLayoutConstant(TAB_STRIP_HEIGHT));
}

views::View* TabContainerImpl::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  if (!HitTestPoint(point)) {
    return nullptr;
  }

  // Return any view that isn't a Tab or this TabContainer immediately. We don't
  // want to interfere.
  views::View* v = View::GetTooltipHandlerForPoint(point);
  if (v && v != this && !views::IsViewClass<Tab>(v)) {
    return v;
  }

  views::View* tab = FindTabHitByPoint(point);
  if (tab) {
    return tab;
  }

  return this;
}

std::optional<BrowserRootView::DropIndex> TabContainerImpl::GetDropIndex(
    const ui::DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  CompleteAnimationAndLayout();

  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());

  std::vector<TabSlotView*> views = layout_helper_->GetTabSlotViews();

  using BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup;
  using BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup;
  using BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;
  using BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;

  // Loop until we find a tab or group header that intersects |event|'s
  // location.
  for (TabSlotView* view : views) {
    const int max_x = view->x() + view->width();
    if (x >= max_x) {
      continue;
    }

    if (view->GetTabSlotViewType() == TabSlotView::ViewType::kTab) {
      Tab* const tab = static_cast<Tab*>(view);
      // Closing tabs should be skipped.
      if (tab->closing()) {
        continue;
      }

      // GetModelIndexOf is an O(n) operation. Since we will definitely
      // return from the loop at this point, it is only called once.
      // Hence the loop is still O(n). Calling this every loop iteration
      // must be avoided since it will become O(n^2).
      const int model_index = GetModelIndexOf(tab).value();

      enum {
        kInsertToLeft,
        kReplace,
        kInsertToRight,
      } location;

      // When hovering over the left or right quarter of a tab, the drop
      // indicator will point between tabs. Otherwise, it will point at the tab.
      const int hot_width = tab->width() / 4;

      if (x >= (tab->x() + tab->width() - hot_width)) {
        location = kInsertToRight;
      } else if (x < tab->x() + hot_width) {
        location = kInsertToLeft;
      } else {
        location = kReplace;
      }

      switch (location) {
        case kInsertToLeft: {
          const bool first_in_group =
              tab->group().has_value() &&
              model_index ==
                  controller_->GetFirstTabInGroup(tab->group().value());
          return BrowserRootView::DropIndex{
              .index = model_index,
              .relative_to_index = kInsertBeforeIndex,
              .group_inclusion =
                  first_in_group ? kIncludeInGroup : kDontIncludeInGroup};
        }

        case kReplace: {
          return BrowserRootView::DropIndex{
              .index = model_index,
              .relative_to_index = kReplaceIndex,
              .group_inclusion = kDontIncludeInGroup};
        }

        case kInsertToRight: {
          return BrowserRootView::DropIndex{
              .index = model_index + 1,
              .relative_to_index = kInsertBeforeIndex,
              .group_inclusion = kDontIncludeInGroup};
        }
      }
    } else {
      TabGroupHeader* const group_header = static_cast<TabGroupHeader*>(view);
      const int first_tab_index =
          controller_->GetFirstTabInGroup(group_header->group().value())
              .value();

      if (x < max_x - group_header->width() / 2) {
        return BrowserRootView::DropIndex{
            .index = first_tab_index,
            .relative_to_index = kInsertBeforeIndex,
            .group_inclusion = kDontIncludeInGroup};
      } else {
        return BrowserRootView::DropIndex{
            .index = first_tab_index,
            .relative_to_index = kInsertBeforeIndex,
            .group_inclusion = kIncludeInGroup};
      }
    }
  }

  // The drop isn't over a tab, add it to the end.
  return BrowserRootView::DropIndex{.index = GetTabCount(),
                                    .relative_to_index = kInsertBeforeIndex,
                                    .group_inclusion = kDontIncludeInGroup};
}

views::View* TabContainerImpl::GetViewForDrop() {
  return this;
}

BrowserRootView::DropTarget* TabContainerImpl::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  if (IsDrawn()) {
    // Allow the drop as long as the mouse is over tab container or vertically
    // before it.
    if (loc_in_local_coords.y() < height()) {
      return this;
    }
  }

  return nullptr;
}

void TabContainerImpl::HandleDragUpdate(
    const std::optional<BrowserRootView::DropIndex>& index) {
  SetDropArrow(index);
}

void TabContainerImpl::HandleDragExited() {
  SetDropArrow({});
}

views::View* TabContainerImpl::TargetForRect(views::View* root,
                                             const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect)) {
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }
  const gfx::Point point(rect.CenterPoint());

  // Return any view that isn't a Tab or this TabStrip immediately. We don't
  // want to interfere.
  views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
  if (v && v != this && !views::IsViewClass<Tab>(v)) {
    return v;
  }

  views::View* tab = FindTabHitByPoint(point);
  if (tab) {
    return tab;
  }

  return this;
}

void TabContainerImpl::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
}

void TabContainerImpl::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  // The rightmost tab (or the `overall_bounds_view_`) moving might have changed
  // our preferred width.
  PreferredSizeChanged();
}

void TabContainerImpl::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  // Send the Container a message to simulate a mouse moved event at the current
  // mouse position. This tickles the Tab the mouse is currently over to show
  // the "hot" state of the close button, or to show the hover card, etc.  Note
  // that this is not required (and indeed may crash!) during a drag session.
  if (!IsDragSessionActive()) {
    // The widget can apparently be null during shutdown.
    views::Widget* widget = GetWidget();
    if (widget) {
      widget->SynthesizeMouseMoveEvent();
    }
  }

  PreferredSizeChanged();
}

// TabContainerImpl::DropArrow:
// ----------------------------------------------------------

TabContainerImpl::DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                                       bool point_down,
                                       views::Widget* context)
    : index_(index), point_down_(point_down) {
  arrow_window_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(g_drop_indicator_width, g_drop_indicator_height);
  params.context = context->GetNativeWindow();
  arrow_window_->Init(std::move(params));
  arrow_view_ =
      arrow_window_->SetContentsView(std::make_unique<views::ImageView>());
  arrow_view_->SetImage(
      ui::ImageModel::FromResourceId(GetDropArrowImageResourceId(point_down_)));
  scoped_observation_.Observe(arrow_window_.get());

  arrow_window_->Show();
}

TabContainerImpl::DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  if (arrow_window_) {
    arrow_window_->Close();
  }
}

void TabContainerImpl::DropArrow::SetPointDown(bool down) {
  if (point_down_ == down) {
    return;
  }

  point_down_ = down;
  arrow_view_->SetImage(
      ui::ImageModel::FromResourceId(GetDropArrowImageResourceId(point_down_)));
}

void TabContainerImpl::DropArrow::SetWindowBounds(const gfx::Rect& bounds) {
  arrow_window_->SetBounds(bounds);
}

void TabContainerImpl::DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_window_.get()));
  scoped_observation_.Reset();
  arrow_window_ = nullptr;
}

views::ViewModelT<Tab>* TabContainerImpl::GetTabsViewModel() {
  return &tabs_view_model_;
}

std::optional<gfx::Rect> TabContainerImpl::GetVisibleContentRect() {
  views::ScrollView* scroll_container =
      views::ScrollView::GetScrollViewForContents(scroll_contents_view_);
  if (!scroll_container) {
    return std::nullopt;
  }

  return scroll_container->GetVisibleRect();
}

void TabContainerImpl::AnimateScrollToShowXCoordinate(const int start_edge,
                                                      const int target_edge) {
  if (tab_scrolling_animation_) {
    tab_scrolling_animation_->Stop();
  }

  gfx::Rect start_rect(start_edge, 0, 0, 0);
  gfx::Rect target_rect(target_edge, 0, 0, 0);

  tab_scrolling_animation_ = std::make_unique<TabScrollingAnimation>(
      scroll_contents_view_, bounds_animator_.container(),
      bounds_animator_.GetAnimationDuration(), start_rect, target_rect);
  tab_scrolling_animation_->Start();
}

void TabContainerImpl::AnimateTabSlotViewTo(TabSlotView* tab_slot_view,
                                            const gfx::Rect& target_bounds) {
  // If we don't own the tab, let our controller handle it.
  if (tab_slot_view->parent() != this) {
    controller_->UpdateAnimationTarget(tab_slot_view, target_bounds);
    return;
  }

  // Also skip slots already being animated to the same ideal bounds.  Calling
  // AnimateViewTo() again restarts the animation, which changes the timing of
  // how the slot animates, leading to hitches.
  if (bounds_animator_.GetTargetBounds(tab_slot_view) == target_bounds) {
    return;
  }

  bounds_animator_.AnimateViewTo(
      tab_slot_view, target_bounds,
      std::make_unique<TabSlotAnimationDelegate>(this, tab_slot_view));
}

void TabContainerImpl::UpdateIdealBounds() {
  // No tabs = no width. This can happen during startup and shutdown, or, if
  // CompoundTabContainer is in use, all the tabs are in the other TabContainer.
  if (GetTabCount() == 0) {
    return;
  }

  // Update |last_available_width_| in case there is a different amount of
  // available width than there was in the last layout (e.g. if the tabstrip
  // is currently hidden).
  last_available_width_ = GetAvailableWidthForTabContainer();

  layout_helper_->UpdateIdealBounds(CalculateAvailableWidthForTabs());
}

void TabContainerImpl::SnapToIdealBounds() {
  for (int i = 0; i < GetTabCount(); ++i) {
    if (GetTabAtModelIndex(i)->parent() != this) {
      continue;
    }
    GetTabAtModelIndex(i)->SetBoundsRect(tabs_view_model_.ideal_bounds(i));
  }

  for (const auto& header_pair : group_views_) {
    if (header_pair.second->header()->parent() != this) {
      continue;
    }
    header_pair.second->header()->SetBoundsRect(
        layout_helper_->group_header_ideal_bounds().at(header_pair.first));
    header_pair.second->UpdateBounds();
  }

  overall_bounds_view_->SetBoundsRect(gfx::Rect(GetIdealTrailingX(), 0));

  PreferredSizeChanged();
}

int TabContainerImpl::CalculateAvailableWidthForTabs() const {
  return override_available_width_for_tabs_.value_or(
      GetAvailableWidthForTabContainer());
}

void TabContainerImpl::StartInsertTabAnimation(int model_index) {
  ExitTabClosingMode();

  gfx::Rect bounds = GetTabAtModelIndex(model_index)->bounds();
  bounds.set_height(GetLayoutConstant(TAB_STRIP_HEIGHT));

  // Adjust the starting bounds of the new tab.
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  if (model_index > 0) {
    // If we have a tab to our left, start at its right edge.
    bounds.set_x(GetTabAtModelIndex(model_index - 1)->bounds().right() -
                 tab_overlap);
  } else {
    CHECK_LT(model_index + 1, GetTabCount())
        << "First tab inserted into the tabstrip should not animate.";
    // Otherwise, if we have a tab to our right, start at its left edge.
    bounds.set_x(GetTabAtModelIndex(model_index + 1)->bounds().x());
  }

  // Start at the width of the overlap in order to animate at the same speed
  // the surrounding tabs are moving, since at this width the subsequent tab
  // is naturally positioned at the same X coordinate.
  bounds.set_width(tab_overlap);
  GetTabAtModelIndex(model_index)->SetBoundsRect(bounds);

  // Animate in to the full width.
  AnimateToIdealBounds();
}

void TabContainerImpl::StartRemoveTabAnimation(Tab* tab,
                                               int former_model_index) {
  // Update ideal bounds before using them to check if we should stay in tab
  // closing mode. See crbug.com/40838229.
  UpdateIdealBounds();
  if (in_tab_close_ && GetTabCount() > 0 &&
      override_available_width_for_tabs_ >
          tabs_view_model_.ideal_bounds(GetTabCount() - 1).right()) {
    // Tab closing mode is no longer constraining tab widths - they're at full
    // size. Exit tab closing mode so that it doesn't artificially inflate our
    // bounds.
    ExitTabClosingMode();
  }

  AnimateToIdealBounds();

  gfx::Rect target_bounds =
      GetTargetBoundsForClosingTab(tab, former_model_index);

  // If the tab is being dragged, we don't own it, and can't run animations on
  // it. We need to take it back first.
  if (tab->dragging()) {
    // Don't bother animating if the tab has been detached rather than closed -
    // i.e. it's being moved to another tabstrip. At this point it's safe to
    // just destroy the tab immediately.
    if (tab->detached()) {
      OnTabCloseAnimationCompleted(tab);
      return;
    }

    DCHECK(IsDragSessionEnding());
  }

  if (tab->parent() != this) {
    // Notify our parent of the new animation target, since we can't animate
    // `tab` ourselves.
    controller_->UpdateAnimationTarget(tab, target_bounds);
    return;
  }

  // TODO(pkasting): When closing multiple tabs, we get repeated RemoveTabAt()
  // calls, each of which closes a new tab and thus generates different ideal
  // bounds.  We should update the animations of any other tabs that are
  // currently being closed to reflect the new ideal bounds, or else change from
  // removing one tab at a time to animating the removal of all tabs at once.

  bounds_animator_.AnimateViewTo(
      tab, target_bounds, std::make_unique<RemoveTabDelegate>(this, tab));
}

gfx::Rect TabContainerImpl::GetTargetBoundsForClosingTab(
    Tab* tab,
    int former_model_index) const {
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();

  // Compute the target bounds for animating this tab closed.  The tab's left
  // edge should stay joined to the right edge of the previous tab, if any.
  gfx::Rect target_bounds = tab->bounds();
  target_bounds.set_x(
      (former_model_index > 0)
          ? (tabs_view_model_.ideal_bounds(former_model_index - 1).right() -
             tab_overlap)
          : 0);

  // The tab should animate to the width of the overlap in order to close at the
  // same speed the surrounding tabs are moving, since at this width the
  // subsequent tab is naturally positioned at the same X coordinate.
  target_bounds.set_width(tab_overlap);

  return target_bounds;
}

int TabContainerImpl::GetIdealTrailingX() const {
  // Our ideal width is the trailing x of our rightmost tab's ideal bounds.
  return GetTabCount() > 0
             ? tabs_view_model_.ideal_bounds(GetTabCount() - 1).right()
             : 0;
}

std::optional<int> TabContainerImpl::GetMidAnimationTrailingX() const {
  if (!controller_->IsAnimatingInTabStrip() || IsDragSessionActive() ||
      IsDragSessionEnding()) {
    return std::nullopt;
  }

  // During animations not related to a drag session, we want to tightly hug
  // our tabs. This allows the NTB to slide smoothly as tabs are opened and
  // closed.

  int trailing_x = 0;
  // The visual order of the tabs can be out of sync with the logical order,
  // so we have to check all of them to find the visually trailing-most one.
  for (views::View* child : children()) {
    trailing_x = std::max(trailing_x, child->bounds().right());
  }

  return trailing_x;
}

void TabContainerImpl::CloseTabInViewModel(int index) {
  Tab* tab = GetTabAtModelIndex(index);
  bool tab_was_active = tab->IsActive();

  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);

  tabs_view_model_.Remove(index);
  layout_helper_->MarkTabAsClosing(index, tab);

  if (tab_was_active) {
    tab->ActiveStateChanged();
  }
}

void TabContainerImpl::OnTabRemoved(Tab* tab) {
  // Remove `tab` from `layout_helper_` so we don't try to lay it out later.
  layout_helper_->RemoveTab(tab);
}

void TabContainerImpl::OnTabCloseAnimationCompleted(Tab* tab) {
  DCHECK(tab->closing());
  OnTabRemoved(tab);

  // Delete `tab`.
  tab->parent()->RemoveChildViewT(tab);
}

void TabContainerImpl::UpdateClosingModeOnRemovedTab(int model_index,
                                                     bool was_active) {
  // The tab at |model_index| has already been removed from the model, but is
  // still in |tabs_view_model_|.  Index math with care!
  const int model_count = GetTabCount() - 1;

  // If we're closing the last tab, tab closing mode is no longer meaningful.
  if (model_count == 0) {
    ExitTabClosingMode();
  }

  // No updates needed if we aren't in tab closing mode or are closing the
  // trailingmost tab.
  if (!in_tab_close_ || model_index == model_count) {
    return;
  }

  // Update `override_available_width_for_tabs_` so that as the user closes tabs
  // with the mouse a tab continues to fall under the mouse.
  const Tab* const tab_being_removed = GetTabAtModelIndex(model_index);
  int size_delta = tab_being_removed->width();

  // When removing an active, non-pinned tab, the next active tab will be
  // given the active width (unless it is pinned). Thus the width being
  // removed from the container is really the current width of whichever
  // inactive tab will be made active.
  if (was_active && !tab_being_removed->data().pinned &&
      layout_helper_->active_tab_width() >
          layout_helper_->inactive_tab_width()) {
    const std::optional<int> next_active_viewmodel_index =
        controller_->GetActiveIndex();
    // The next active tab may not be in this TabContainer.
    if (next_active_viewmodel_index.has_value()) {
      // At this point, model's internal state has already been updated.
      // `contents` has been detached from model and the active index has
      // been updated. But the tab for `contents` isn't removed yet. Thus,
      // we need to fix up `next_active_viewmodel_index` based on it.
      const bool adjust_for_removed_tab =
          model_index <= next_active_viewmodel_index;
      const int next_active_index = next_active_viewmodel_index.value() +
                                    (adjust_for_removed_tab ? 1 : 0);
      const Tab* const next_active_tab = GetTabAtModelIndex(next_active_index);
      if (!next_active_tab->data().pinned) {
        size_delta = next_active_tab->width();
      }
    }
  }

  override_available_width_for_tabs_ =
      tabs_view_model_.ideal_bounds(model_count).right() - size_delta +
      TabStyle::Get()->GetTabOverlap();
}

void TabContainerImpl::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (GetTabCount() == 0) {
    return;
  }

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  ExitTabClosingMode();
  int pinned_tab_count = layout_helper_->GetPinnedTabCount();
  if (pinned_tab_count == GetTabCount()) {
    // Only pinned tabs, we know the tab widths won't have changed (all
    // pinned tabs have the same width), so there is nothing to do.
    return;
  }
  // Don't try and avoid layout based on tab sizes. If tabs are small enough
  // then the width of the active tab may not change, but other widths may
  // have. This is particularly important if we've overflowed (all tabs are at
  // the min).
  AnimateToIdealBounds();
}

void TabContainerImpl::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!IsDragSessionActive()) {
    ResizeLayoutTabs();
  } else {
    StartResizeLayoutTabsFromTouchTimer();
  }
}

void TabContainerImpl::StartResizeLayoutTabsFromTouchTimer() {
  // Amount of time we delay before resizing after a close from a touch.
  constexpr auto kTouchResizeLayoutTime = base::Seconds(2);

  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(FROM_HERE, kTouchResizeLayoutTime, this,
                             &TabContainerImpl::ResizeLayoutTabsFromTouch);
}

bool TabContainerImpl::IsDragSessionActive() const {
  // `drag_context_` may be null in tests.
  return drag_context_ && drag_context_->IsDragSessionActive();
}

bool TabContainerImpl::IsDragSessionEnding() const {
  // `drag_context_` may be null in tests.
  return drag_context_ && drag_context_->IsAnimatingDragEnd();
}

void TabContainerImpl::AddMessageLoopObserver() {
  if (!mouse_watcher_) {
    // Expand the watched region downwards below the bottom of the tabstrip.
    // This allows users to move the cursor horizontally, to another tab,
    // without accidentally exiting closing mode if they drift verticaally
    // slightly out of the tabstrip.
    constexpr int kTabStripAnimationVSlop = 40;
    // Expand the watched region to the right to cover the NTB. This prevents
    // the scenario where the user goes to click on the NTB while they're in
    // closing mode, and closing mode exits just as they reach the NTB.
    constexpr int kTabStripAnimationHSlop = 60;
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<views::MouseWatcherViewHost>(
            controller_->GetTabClosingModeMouseWatcherHostView(),
            gfx::Insets::TLBR(
                0, base::i18n::IsRTL() ? kTabStripAnimationHSlop : 0,
                kTabStripAnimationVSlop,
                base::i18n::IsRTL() ? 0 : kTabStripAnimationHSlop)),
        this);
  }
  mouse_watcher_->Start(GetWidget()->GetNativeWindow());
}

void TabContainerImpl::RemoveMessageLoopObserver() {
  mouse_watcher_ = nullptr;
}

void TabContainerImpl::OrderTabSlotView(TabSlotView* slot_view) {
  if (slot_view->parent() != this) {
    return;
  }

  // |slot_view| is in the wrong place in children(). Fix it.
  std::vector<TabSlotView*> slots = layout_helper_->GetTabSlotViews();
  size_t target_slot_index =
      base::ranges::find(slots, slot_view) - slots.begin();
  // Find the index in children() that corresponds to |target_slot_index|.
  size_t view_index = 0;
  for (size_t slot_index = 0; slot_index < target_slot_index; ++slot_index) {
    // If we don't own this view, skip it *without* advancing in children().
    if (slots[slot_index]->parent() != this) {
      continue;
    }
    if (view_index == children().size()) {
      break;
    }
    ++view_index;
  }

  ReorderChildView(slot_view, view_index);
}

bool TabContainerImpl::IsPointInTab(
    Tab* tab,
    const gfx::Point& point_in_tabstrip_coords) {
  if (!tab->GetVisible()) {
    return false;
  }
  if (tab->parent() != this) {
    return false;
  }

  const gfx::Point point_in_tab_coords =
      View::ConvertPointToTarget(this, tab, point_in_tabstrip_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

Tab* TabContainerImpl::FindTabHitByPoint(const gfx::Point& point) {
  // Check all tabs, even closing tabs. Mouse events need to reach closing tabs
  // for users to be able to rapidly middle-click close several tabs.
  std::vector<Tab*> all_tabs = layout_helper_->GetTabs();

  // The display order doesn't necessarily match the child order, so we iterate
  // in display order.
  for (size_t i = 0; i < all_tabs.size(); ++i) {
    // If we don't first exclude points outside the current tab, the code below
    // will return the wrong tab if the next tab is selected, the following tab
    // is active, and |point| is in the overlap region between the two.
    Tab* tab = all_tabs[i];
    if (!IsPointInTab(tab, point)) {
      continue;
    }

    // Selected tabs render atop unselected ones, and active tabs render atop
    // everything.  Check whether the next tab renders atop this one and |point|
    // is in the overlap region.
    Tab* next_tab = i < (all_tabs.size() - 1) ? all_tabs[i + 1] : nullptr;
    if (next_tab &&
        (next_tab->IsActive() ||
         (next_tab->IsSelected() && !tab->IsSelected())) &&
        IsPointInTab(next_tab, point)) {
      return next_tab;
    }

    // This is the topmost tab for this point.
    return tab;
  }

  return nullptr;
}

bool TabContainerImpl::ShouldTabBeVisible(const Tab* tab) const {
  // When the tabstrip is scrollable, it can grow to accommodate any number of
  // tabs, so tabs can never become clipped.
  // N.B. Tabs can still be not-visible because they're in a collapsed group,
  // but that's handled elsewhere.
  // N.B. This is separate from the tab being potentially scrolled offscreen -
  // this solely determines whether the tab should be clipped for the
  // pre-scrolling overflow behavior.
  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
    return true;
  }

  // Detached tabs should always be invisible (as they close).
  if (tab->detached()) {
    return false;
  }

  // If the tab would be clipped by the trailing edge of the strip, even if the
  // tabstrip were resized to its greatest possible width, it shouldn't be
  // visible.
  int right_edge = tab->bounds().right();
  const int tabstrip_right = tab->parent() != this
                                 ? drag_context_->GetTabDragAreaWidth()
                                 : GetAvailableWidthForTabContainer();
  if (right_edge > tabstrip_right) {
    return false;
  }

  // Non-clipped dragging tabs should always be visible.
  if (tab->dragging()) {
    return true;
  }

  // Let all non-clipped closing tabs be visible.  These will probably finish
  // closing before the user changes the active tab, so there's little reason to
  // try and make the more complex logic below apply.
  if (tab->closing()) {
    return true;
  }

  // Now we need to check whether the tab isn't currently clipped, but could
  // become clipped if we changed the active tab, widening either this tab or
  // the tabstrip portion before it.

  // Pinned tabs don't change size when activated, so any tab in the pinned tab
  // region is safe.
  if (tab->data().pinned) {
    return true;
  }

  // If the active tab is on or before this tab, we're safe.
  if (controller_->GetActiveIndex() <= GetModelIndexOf(tab)) {
    return true;
  }

  // We need to check what would happen if the active tab were to move to this
  // tab or before. If animating, we want to use the target bounds in this
  // calculation.
  if (IsAnimating()) {
    right_edge = bounds_animator_.GetTargetBounds(tab).right();
  }
  return (right_edge + layout_helper_->active_tab_width() -
          layout_helper_->inactive_tab_width()) <= tabstrip_right;
}

gfx::Rect TabContainerImpl::GetDropBounds(int drop_index,
                                          bool drop_before,
                                          bool drop_in_group,
                                          bool* is_beneath) {
  DCHECK_NE(drop_index, -1);

  // The X location the indicator points to.
  int center_x = -1;

  if (GetTabCount() == 0) {
    // If the tabstrip is empty, it doesn't matter where the drop arrow goes.
    // The tabstrip can only be transiently empty, e.g. during shutdown.
    return gfx::Rect();
  }

  Tab* tab = GetTabAtModelIndex(std::min(drop_index, GetTabCount() - 1));
  const bool first_in_group =
      drop_index < GetTabCount() && tab->group().has_value() &&
      GetModelIndexOf(tab) ==
          controller_->GetFirstTabInGroup(tab->group().value());

  const int overlap = tab->tab_style()->GetTabOverlap();
  if (!drop_before || !first_in_group || drop_in_group) {
    // Dropping between tabs, or between a group header and the group's first
    // tab.
    center_x = tab->x();
    const int width = tab->width();
    if (drop_index < GetTabCount()) {
      center_x += drop_before ? (overlap / 2) : (width / 2);
    } else {
      center_x += width - (overlap / 2);
    }
  } else {
    // Dropping before a group header.
    TabGroupHeader* const header = group_views_[tab->group().value()]->header();
    center_x = header->x() + overlap / 2;
  }

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
  if (*is_beneath) {
    drop_bounds.Offset(0, drop_bounds.height() + height());
  }

  return drop_bounds;
}

void TabContainerImpl::SetDropArrow(
    const std::optional<BrowserRootView::DropIndex>& index) {
  if (!index) {
    controller_->OnDropIndexUpdate(std::nullopt, false);
    drop_arrow_.reset();
    return;
  }

  // Let the controller know of the index update.
  const bool drop_before =
      index->relative_to_index ==
      BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;
  const bool group_inclusion =
      index->group_inclusion ==
      BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup;
  controller_->OnDropIndexUpdate(index->index, drop_before);

  if (drop_arrow_ && (index == drop_arrow_->index())) {
    return;
  }

  bool is_beneath;
  gfx::Rect drop_bounds =
      GetDropBounds(index->index, drop_before, group_inclusion, &is_beneath);

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(*index, !is_beneath, GetWidget());
  } else {
    drop_arrow_->set_index(*index);
    drop_arrow_->SetPointDown(!is_beneath);
  }

  // Reposition the window.
  drop_arrow_->SetWindowBounds(drop_bounds);
}

void TabContainerImpl::UpdateAccessibleTabIndices() {
  const int num_tabs = GetTabCount();
  for (int i = 0; i < num_tabs; ++i) {
    GetTabAtModelIndex(i)->GetViewAccessibility().SetPosInSet(i + 1);
    GetTabAtModelIndex(i)->GetViewAccessibility().SetSetSize(num_tabs);
  }
}

bool TabContainerImpl::IsValidModelIndex(int model_index) const {
  return controller_->IsValidModelIndex(model_index);
}

BEGIN_METADATA(TabContainerImpl)
ADD_READONLY_PROPERTY_METADATA(int, AvailableWidthForTabContainer)
END_METADATA
