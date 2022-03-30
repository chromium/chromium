// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"

#include "base/bits.h"
#include "base/containers/adapters.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
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

gfx::ImageSkia* GetDropArrowImage(bool is_down) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// Provides the ability to monitor when a tab's bounds have been animated. Used
// to hook callbacks to adjust things like tabstrip preferred size and tab group
// underlines.
class TabSlotAnimationDelegate : public gfx::AnimationDelegate {
 public:
  using OnAnimationProgressedCallback =
      base::RepeatingCallback<void(TabSlotView*)>;

  TabSlotAnimationDelegate(
      TabContainer* tab_container,
      TabSlotView* slot_view,
      OnAnimationProgressedCallback on_animation_progressed);
  TabSlotAnimationDelegate(const TabSlotAnimationDelegate&) = delete;
  TabSlotAnimationDelegate& operator=(const TabSlotAnimationDelegate&) = delete;
  ~TabSlotAnimationDelegate() override;

  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 protected:
  TabContainer* tab_container() { return tab_container_; }
  TabSlotView* slot_view() { return slot_view_; }

 private:
  const raw_ptr<TabContainer> tab_container_;
  const raw_ptr<TabSlotView> slot_view_;
  OnAnimationProgressedCallback on_animation_progressed_;
};

TabSlotAnimationDelegate::TabSlotAnimationDelegate(
    TabContainer* tab_container,
    TabSlotView* slot_view,
    OnAnimationProgressedCallback on_animation_progressed)
    : tab_container_(tab_container),
      slot_view_(slot_view),
      on_animation_progressed_(on_animation_progressed) {
  slot_view_->set_animating(true);
}

TabSlotAnimationDelegate::~TabSlotAnimationDelegate() = default;

void TabSlotAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  on_animation_progressed_.Run(slot_view());
}

void TabSlotAnimationDelegate::AnimationEnded(const gfx::Animation* animation) {
  slot_view_->set_animating(false);
  AnimationProgressed(animation);
  slot_view_->Layout();
}

void TabSlotAnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate : public TabSlotAnimationDelegate {
 public:
  ResetDraggingStateDelegate(
      TabContainer* tab_container,
      Tab* tab,
      OnAnimationProgressedCallback on_animation_progressed);
  ResetDraggingStateDelegate(const ResetDraggingStateDelegate&) = delete;
  ResetDraggingStateDelegate& operator=(const ResetDraggingStateDelegate&) =
      delete;
  ~ResetDraggingStateDelegate() override;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
};

ResetDraggingStateDelegate::ResetDraggingStateDelegate(
    TabContainer* tab_container,
    Tab* tab,
    OnAnimationProgressedCallback on_animation_progressed)
    : TabSlotAnimationDelegate(tab_container, tab, on_animation_progressed) {}

ResetDraggingStateDelegate::~ResetDraggingStateDelegate() = default;

void ResetDraggingStateDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  static_cast<Tab*>(slot_view())->set_dragging(false);
  TabSlotAnimationDelegate::AnimationEnded(animation);
}

void ResetDraggingStateDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

// A class that calculates a z-value for a TabContainer child view (one of a
// tab, a tab group header, a tab group underline, or a tab group highlight).
// Can be compared with other ZOrderableTabContainerElements to determine paint
// order of their associated views.
class ZOrderableTabContainerElement {
 public:
  ZOrderableTabContainerElement(views::View* const child,
                                absl::optional<const TabGroupUnderline* const>
                                    dragging_tabs_current_group_underline)
      : child_(child),
        z_value_(
            CalculateZValue(child, dragging_tabs_current_group_underline)) {}

  bool operator<(const ZOrderableTabContainerElement& rhs) const {
    return z_value_ < rhs.z_value_;
  }

  views::View* view() const { return child_; }

 private:
  // Determines the 'height' of |child|, which should be used to determine the
  // paint order of TabContainer's children.  Larger z-values should be painted
  // on top of smaller ones.
  static float CalculateZValue(views::View* child,
                               absl::optional<const TabGroupUnderline* const>
                                   dragging_tabs_current_group_underline) {
    Tab* tab = views::AsViewClass<Tab>(child);
    TabGroupHeader* header = views::AsViewClass<TabGroupHeader>(child);
    TabGroupUnderline* underline = views::AsViewClass<TabGroupUnderline>(child);
    TabGroupHighlight* highlight = views::AsViewClass<TabGroupHighlight>(child);
    DCHECK_EQ(1, !!tab + !!header + !!underline + !!highlight);

    // Construct a bitfield that encodes |child|'s z-value. Higher-order bits
    // encode more important properties - see usage below for details on each.
    // The lowest-order |num_bits_reserved_for_tab_style_z_value| bits are
    // reserved for the factors considered by TabStyle, e.g. selection and hover
    // state.
    constexpr int num_bits_reserved_for_tab_style_z_value =
        base::bits::Log2Ceiling(static_cast<int>(TabStyle::kMaximumZValue) + 1);
    enum ZValue {
      kActiveTab = (1u << (num_bits_reserved_for_tab_style_z_value + 4)),
      kDraggedHeader = (1u << (num_bits_reserved_for_tab_style_z_value + 3)),
      kDragRelevantUnderline =
          (1u << (num_bits_reserved_for_tab_style_z_value + 2)),
      kDraggedTab = (1u << (num_bits_reserved_for_tab_style_z_value + 1)),
      kGroupView = (1u << num_bits_reserved_for_tab_style_z_value)
    };

    unsigned int z_value = 0;

    // The active tab is always on top.
    if (tab && tab->IsActive())
      z_value |= kActiveTab;

    // If we're dragging a header, that is painted above non-active tabs.
    if (header && header->dragging())
      z_value |= kDraggedHeader;

    // If we're dragging tabs into a group, or are dragging a group, the
    // underline for that group is painted above non-active dragged tabs.
    if (underline && dragging_tabs_current_group_underline.has_value() &&
        underline == dragging_tabs_current_group_underline.value())
      z_value |= kDragRelevantUnderline;

    // Dragged tabs are painted above anything that isn't part of the drag.
    if (tab && tab->dragging())
      z_value |= kDraggedTab;

    // Group headers, highlights and underlines are painted above non-active,
    // non-dragged tabs. Note that a group highlight is only visible when the
    // associated group is being dragged in a header drag.
    if (header || underline || highlight)
      z_value |= kGroupView;

    // The remaining (non-active, non-dragged) tabs are painted last. They are
    // ordered by their selected or hovered state, which is animated and thus
    // real-valued.
    const float tab_style_z_value = tab ? tab->tab_style()->GetZValue() : 0.0f;
    return z_value + tab_style_z_value;
  }

  views::View* child_;
  float z_value_;
};  // ZOrderableTabStripElement

// Helper class that manages the tab scrolling animation.
class TabScrollingAnimation : public gfx::LinearAnimation,
                              public gfx::AnimationDelegate {
 public:
  explicit TabScrollingAnimation(
      views::View* contents_view,
      gfx::AnimationContainer* bounds_animator_container,
      base::TimeDelta duration,
      const gfx::Rect start_visible_rect,
      const gfx::Rect end_visible_rect)
      : gfx::LinearAnimation(duration,
                             gfx::LinearAnimation::kDefaultFrameRate,
                             this),
        contents_view_(contents_view),
        start_visible_rect_(start_visible_rect),
        end_visible_rect_(end_visible_rect) {
    SetContainer(bounds_animator_container);
  }
  TabScrollingAnimation(const TabScrollingAnimation&) = delete;
  TabScrollingAnimation& operator=(const TabScrollingAnimation&) = delete;
  ~TabScrollingAnimation() override = default;

  void AnimateToState(double state) override {
    gfx::Rect intermediary_rect(
        start_visible_rect_.x() +
            (end_visible_rect_.x() - start_visible_rect_.x()) * state,
        start_visible_rect_.y(), start_visible_rect_.width(),
        start_visible_rect_.height());

    contents_view_->ScrollRectToVisible(intermediary_rect);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    contents_view_->ScrollRectToVisible(end_visible_rect_);
  }

 private:
  const raw_ptr<views::View> contents_view_;
  const gfx::Rect start_visible_rect_;
  const gfx::Rect end_visible_rect_;
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabContainer::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabContainer::RemoveTabDelegate : public TabSlotAnimationDelegate {
 public:
  RemoveTabDelegate(TabContainer* tab_container,
                    Tab* tab,
                    OnAnimationProgressedCallback on_animation_progressed);
  RemoveTabDelegate(const RemoveTabDelegate&) = delete;
  RemoveTabDelegate& operator=(const RemoveTabDelegate&) = delete;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
};

TabContainer::RemoveTabDelegate::RemoveTabDelegate(
    TabContainer* tab_container,
    Tab* tab,
    OnAnimationProgressedCallback on_animation_progressed)
    : TabSlotAnimationDelegate(tab_container, tab, on_animation_progressed) {}

void TabContainer::RemoveTabDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  tab_container()->OnTabCloseAnimationCompleted(static_cast<Tab*>(slot_view()));
}

void TabContainer::RemoveTabDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

TabContainer::TabContainer(TabStripController* controller,
                           TabHoverCardController* hover_card_controller,
                           TabDragContext* drag_context,
                           TabSlotController* tab_slot_controller,
                           views::View* scroll_contents_view)
    : controller_(controller),
      hover_card_controller_(hover_card_controller),
      drag_context_(drag_context),
      tab_slot_controller_(tab_slot_controller),
      scroll_contents_view_(scroll_contents_view),
      bounds_animator_(this),
      layout_helper_(std::make_unique<TabStripLayoutHelper>(
          controller,
          base::BindRepeating(&TabContainer::tabs_view_model,
                              base::Unretained(this)))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  if (!gfx::Animation::ShouldRenderRichAnimation())
    bounds_animator().SetAnimationDuration(base::TimeDelta());

  bounds_animator().AddObserver(this);

  if (g_drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image = GetDropArrowImage(true);
    g_drop_indicator_width = drop_image->width();
    g_drop_indicator_height = drop_image->height();
  }
}

TabContainer::~TabContainer() {
  // The animations may reference the tabs or group views. Shut down the
  // animation before we destroy any animated views.
  StopAnimating(false);

  // Since TabGroupViews expects be able to remove the views it creates, clear
  // |group_views_| before removing the remaining children below.
  group_views_.clear();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  RemoveAllChildViews();
}

void TabContainer::SetAvailableWidthCallback(
    base::RepeatingCallback<int()> available_width_callback) {
  available_width_callback_ = available_width_callback;
}

Tab* TabContainer::AddTab(std::unique_ptr<Tab> tab,
                          int model_index,
                          TabPinned pinned) {
  absl::optional<tab_groups::TabGroupId> group = tab->group();
  Tab* tab_ptr = AddChildViewAt(
      std::move(tab), GetViewInsertionIndex(group, absl::nullopt, model_index));
  tabs_view_model_.Add(tab_ptr, model_index);
  layout_helper_->InsertTabAt(model_index, tab_ptr, pinned);

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (GetTabCount() > 1 && GetWidget() && GetWidget()->IsVisible()) {
    StartInsertTabAnimation(model_index);
  } else {
    CompleteAnimationAndLayout();
  }

  UpdateAccessibleTabIndices();

  return tab_ptr;
}

void TabContainer::MoveTab(int from_model_index, int to_model_index) {
  Tab* tab = GetTabAtModelIndex(from_model_index);
  ReorderChildView(tab, GetViewInsertionIndex(tab->group(), from_model_index,
                                              to_model_index));
  tabs_view_model_.Move(from_model_index, to_model_index);
  layout_helper_->MoveTab(tab->group(), from_model_index, to_model_index);

  layout_helper()->SetTabPinned(to_model_index, tab->data().pinned
                                                    ? TabPinned::kPinned
                                                    : TabPinned::kUnpinned);

  StartBasicAnimation();

  UpdateAccessibleTabIndices();
}

void TabContainer::RemoveTab(int model_index, bool was_active) {
  UpdateClosingModeOnRemovedTab(model_index, was_active);

  Tab* tab = GetTabAtModelIndex(model_index);
  tab->SetClosing(true);

  RemoveTabFromViewModel(model_index);

  StartRemoveTabAnimation(tab, model_index);

  UpdateAccessibleTabIndices();
}

void TabContainer::SetTabPinned(int model_index, TabPinned pinned) {
  layout_helper()->SetTabPinned(model_index, pinned);

  if (GetWidget() && GetWidget()->IsVisible()) {
    ExitTabClosingMode();

    StartBasicAnimation();
  } else {
    CompleteAnimationAndLayout();
  }
}

void TabContainer::ScrollTabToVisible(int model_index) {
  views::ScrollView* scroll_container =
      views::ScrollView::GetScrollViewForContents(scroll_contents_view_);
  if (!scroll_container) {
    return;
  }

  // If the tab strip won't be scrollable after the current tabstrip animations
  // complete, scroll animation wouldn't be meaningful.
  if (tabs_view_model_.ideal_bounds(GetTabCount() - 1).right() <=
      GetAvailableWidthForTabContainer())
    return;

  if (tab_scrolling_animation_)
    tab_scrolling_animation_->Stop();

  gfx::Rect visible_content_rect = scroll_container->GetVisibleRect();
  gfx::Rect active_tab_ideal_bounds =
      tabs_view_model_.ideal_bounds(model_index);

  if ((active_tab_ideal_bounds.x() >= visible_content_rect.x()) &&
      (active_tab_ideal_bounds.right() <= visible_content_rect.right())) {
    return;
  }

  bool scroll_left = active_tab_ideal_bounds.x() < visible_content_rect.x();
  if (scroll_left) {
    // Scroll the left edge of |visible_content_rect| to show the left edge of
    // the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    gfx::Rect start_left_edge(visible_content_rect.x(),
                              visible_content_rect.y(), 0, 0);
    gfx::Rect target_left_edge(active_tab_ideal_bounds.x(),
                               visible_content_rect.y(), 0, 0);
    tab_scrolling_animation_ = std::make_unique<TabScrollingAnimation>(
        scroll_contents_view_, bounds_animator().container(),
        bounds_animator().GetAnimationDuration(), start_left_edge,
        target_left_edge);
    tab_scrolling_animation_->Start();
  } else {
    // Scroll the right edge of |visible_content_rect| to show the right edge
    // of the tab at |model_index|. We can leave the width entirely up to the
    // ScrollView.
    gfx::Rect start_right_edge(visible_content_rect.right(),
                               visible_content_rect.y(), 0, 0);
    gfx::Rect target_right_edge(active_tab_ideal_bounds.right(),
                                visible_content_rect.y(), 0, 0);
    tab_scrolling_animation_ = std::make_unique<TabScrollingAnimation>(
        scroll_contents_view_, bounds_animator().container(),
        bounds_animator().GetAnimationDuration(), start_right_edge,
        target_right_edge);
    tab_scrolling_animation_->Start();
  }
}

void TabContainer::OnGroupCreated(const tab_groups::TabGroupId& group) {
  auto group_view =
      std::make_unique<TabGroupViews>(this, tab_slot_controller_, group);
  layout_helper()->InsertGroupHeader(group, group_view->header());
  group_views()[group] = std::move(group_view);
}

void TabContainer::OnGroupEditorOpened(const tab_groups::TabGroupId& group) {
  // The context menu relies on a Browser object which is not provided in
  // TabStripTest.
  if (controller_->GetBrowser()) {
    group_views()[group]->header()->ShowContextMenuForViewImpl(
        this, gfx::Point(), ui::MENU_SOURCE_NONE);
  }
}

void TabContainer::OnGroupMoved(const tab_groups::TabGroupId& group) {
  DCHECK(group_views()[group]);

  layout_helper()->UpdateGroupHeaderIndex(group);

  TabGroupHeader* group_header = group_views()[group]->header();
  const int first_tab_model_index =
      controller_->GetFirstTabInGroup(group).value();

  MoveGroupHeader(group_header, first_tab_model_index);
}

void TabContainer::OnGroupClosed(const tab_groups::TabGroupId& group) {
  bounds_animator().StopAnimatingView(group_views().at(group).get()->header());
  layout_helper()->RemoveGroupHeader(group);

  StartBasicAnimation();
  group_views().erase(group);
}

void TabContainer::UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) {
  const auto group_views = group_views_.find(group_id);
  if (group_views != group_views_.end())
    group_views->second->UpdateBounds();
}

int TabContainer::GetModelIndexOf(const TabSlotView* slot_view) const {
  return tabs_view_model_.GetIndexOfView(slot_view);
}

Tab* TabContainer::GetTabAtModelIndex(int index) const {
  return tabs_view_model_.view_at(index);
}

int TabContainer::GetTabCount() const {
  return tabs_view_model_.view_size();
}

void TabContainer::UpdateHoverCard(
    Tab* tab,
    TabSlotController::HoverCardUpdateType update_type) {
  // Some operations (including e.g. starting a drag) can cause the tab focus
  // to change at the same time as the tabstrip is starting to animate; the
  // hover card should not be visible at this time.
  // See crbug.com/1220840 for an example case.
  if (bounds_animator_.IsAnimating()) {
    tab = nullptr;
    update_type = TabSlotController::HoverCardUpdateType::kAnimating;
  }

  if (!hover_card_controller_)
    return;

  hover_card_controller_->UpdateHoverCard(tab, update_type);
}

void TabContainer::HandleLongTap(ui::GestureEvent* event) {
  event->target()->ConvertEventToTarget(this, event);
  gfx::Point local_point = event->location();
  Tab* tab = FindTabHitByPoint(local_point);
  if (tab) {
    ConvertPointToScreen(this, &local_point);
    controller_->ShowContextMenuForTab(tab, local_point, ui::MENU_SOURCE_TOUCH);
  }
}

bool TabContainer::IsRectInWindowCaption(const gfx::Rect& rect) {
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
  const int tab_index = tabs_view_model_.GetIndexOfView(v);
  if (IsValidModelIndex(tab_index)) {
    Tab* tab = GetTabAtModelIndex(tab_index);
    gfx::Rect tab_drag_handle = tab->GetMirroredBounds();
    tab_drag_handle.set_height(drag_handle_extension);
    return extend_drag_handle && !tab->IsActive() &&
           tab_drag_handle.Intersects(rect);
  }

  // |v| is some other view (e.g. a close button in a tab) and therefore |rect|
  // is in client area.
  return false;
}

void TabContainer::OnTabSlotAnimationProgressed(TabSlotView* view) {
  // The rightmost tab moving might have changed the tab container's preferred
  // width.
  PreferredSizeChanged();
  if (view->group())
    UpdateTabGroupVisuals(view->group().value());
}

void TabContainer::StartBasicAnimation() {
  PrepareForAnimation();
  UpdateIdealBounds();
  AnimateToIdealBounds();
}

void TabContainer::InvalidateIdealBounds() {
  last_layout_size_ = gfx::Size();
}

void TabContainer::StopAnimating(bool layout) {
  if (!bounds_animator_.IsAnimating())
    return;

  bounds_animator_.Cancel();

  if (layout)
    CompleteAnimationAndLayout();
}

void TabContainer::CompleteAnimationAndLayout() {
  last_available_width_ = GetAvailableWidthForTabContainer();
  last_layout_size_ = size();

  bounds_animator().Cancel();

  UpdateIdealBounds();
  SnapToIdealBounds();

  SetTabSlotVisibility();
  SchedulePaint();
}

int TabContainer::GetAvailableWidthForTabContainer() const {
  // Falls back to views::View::GetAvailableSize() when
  // |available_width_callback_| is not defined, e.g. when tab scrolling is
  // disabled.
  return available_width_callback_
             ? available_width_callback_.Run()
             : parent()->GetAvailableSize(this).width().value();
}

void TabContainer::StartResetDragAnimation(int tab_model_index) {
  Tab* tab = GetTabAtModelIndex(tab_model_index);
  // Install a delegate to reset the dragging state when done. We have to leave
  // dragging true for the tab otherwise it'll draw beneath the new tab button.
  bounds_animator().AnimateViewTo(
      tab, tabs_view_model_.ideal_bounds(tab_model_index),
      std::make_unique<ResetDraggingStateDelegate>(
          this, GetTabAtModelIndex(tab_model_index),
          base::BindRepeating(&TabContainer::OnTabSlotAnimationProgressed,
                              base::Unretained(this))));
}

void TabContainer::EnterTabClosingMode(absl::optional<int> override_width,
                                       CloseTabSource source) {
  in_tab_close_ = true;
  override_available_width_for_tabs_ = override_width;

  resize_layout_timer_.Stop();
  if (source == CLOSE_TAB_FROM_TOUCH)
    StartResizeLayoutTabsFromTouchTimer();
  else
    AddMessageLoopObserver();
}

void TabContainer::ExitTabClosingMode() {
  in_tab_close_ = false;
  override_available_width_for_tabs_.reset();
}

void TabContainer::SetTabSlotVisibility() {
  bool last_tab_visible = false;
  absl::optional<tab_groups::TabGroupId> last_tab_group = absl::nullopt;
  std::vector<Tab*> tabs = layout_helper()->GetTabs();
  for (Tab* tab : base::Reversed(tabs)) {
    absl::optional<tab_groups::TabGroupId> current_group = tab->group();
    if (current_group != last_tab_group && last_tab_group.has_value()) {
      TabGroupViews* group_view =
          group_views().at(last_tab_group.value()).get();
      group_view->header()->SetVisible(last_tab_visible);
      group_view->underline()->SetVisible(last_tab_visible);
    }
    last_tab_visible = ShouldTabBeVisible(tab);
    last_tab_group = tab->closing() ? absl::nullopt : current_group;

    // Collapsed tabs disappear once they've reached their minimum size. This
    // is different than very small non-collapsed tabs, because in that case
    // the tab (and its favicon) must still be visible.
    bool is_collapsed = (current_group.has_value() &&
                         controller_->IsGroupCollapsed(current_group.value()) &&
                         tab->bounds().width() <= TabStyle::GetTabOverlap());
    tab->SetVisible(is_collapsed ? false : last_tab_visible);
  }
}

void TabContainer::Layout() {
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    // With tab scrolling, the tab container is solely responsible for its own
    // width.
    // It should never be larger than its preferred width.
    const int max_width = CalculatePreferredSize().width();
    // It should never be smaller than its minimum width.
    const int min_width = GetMinimumSize().width();
    // If it can, it should fit within the tab strip region.
    const int available_width = GetAvailableWidthForTabContainer();
    // It should be as wide as possible subject to the above constraints.
    const int width = std::min(max_width, std::max(min_width, available_width));
    SetBounds(0, 0, width, GetLayoutConstant(TAB_HEIGHT));
    SetTabSlotVisibility();
  }

  if (bounds_animator_.IsAnimating()) {
    // Hide tabs that have animated at least partially out of the clip region.
    SetTabSlotVisibility();
    return;
  }

  // Only do a layout if our size or the available width changed.
  const int available_width = GetAvailableWidthForTabContainer();
  if (last_layout_size_ == size() && last_available_width_ == available_width)
    return;
  if (IsDragSessionActive())
    return;
  CompleteAnimationAndLayout();
}

void TabContainer::PaintChildren(const views::PaintInfo& paint_info) {
  // Groups that are being dragged by their header, or that contain the dragged
  // tabs, need an adjusted z-value. Find that group, if it exists.
  absl::optional<tab_groups::TabGroupId> dragging_tabs_current_group =
      absl::nullopt;

  for (const Tab* tab : layout_helper()->GetTabs()) {
    if (tab->dragging()) {
      dragging_tabs_current_group = tab->group();
      break;
    }
  }

  absl::optional<const TabGroupUnderline*>
      dragging_tabs_current_group_underline =
          dragging_tabs_current_group.has_value()
              ? absl::optional<const TabGroupUnderline*>(
                    group_views_[dragging_tabs_current_group.value()]
                        ->underline())
              : absl::nullopt;

  std::vector<ZOrderableTabContainerElement> orderable_children;
  for (views::View* child : children())
    orderable_children.emplace_back(child,
                                    dragging_tabs_current_group_underline);

  // Sort in non-descending order. Stable sort breaks z-value ties by index (for
  // tabs).
  std::stable_sort(orderable_children.begin(), orderable_children.end());

  for (const ZOrderableTabContainerElement& child : orderable_children)
    child.view()->Paint(paint_info);
}

gfx::Size TabContainer::GetMinimumSize() const {
  int minimum_width = layout_helper_->CalculateMinimumWidth();

  return gfx::Size(minimum_width, GetLayoutConstant(TAB_HEIGHT));
}

gfx::Size TabContainer::CalculatePreferredSize() const {
  int preferred_width;
  // The tab container needs to always exactly fit the bounds of the tabs so
  // that NTB can be laid out just to the right of the rightmost tab. When the
  // tabs aren't at their ideal bounds (i.e. during animation or a drag), we
  // need to size ourselves to exactly fit wherever the tabs *currently* are.
  if (bounds_animator_.IsAnimating() || IsDragSessionActive()) {
    // The visual order of the tabs can be out of sync with the logical order,
    // so we have to check all of them to find the visually trailing-most one.
    int max_x = 0;
    for (auto* child : children()) {
      max_x = std::max(max_x, child->bounds().right());
    }
    // The tabs span from 0 to |max_x|, so |max_x| is the current width
    // occupied by tabs. We report the current width as our preferred width so
    // that the tab strip is sized to exactly fit the current position of the
    // tabs.
    preferred_width = max_x;
  } else {
    preferred_width = override_available_width_for_tabs_.value_or(
        layout_helper_->CalculatePreferredWidth());
  }

  return gfx::Size(preferred_width, GetLayoutConstant(TAB_HEIGHT));
}

views::View* TabContainer::GetTooltipHandlerForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return nullptr;

  // Return any view that isn't a Tab or this TabContainer immediately. We don't
  // want to interfere.
  views::View* v = View::GetTooltipHandlerForPoint(point);
  if (v && v != this && !views::IsViewClass<Tab>(v))
    return v;

  views::View* tab = FindTabHitByPoint(point);
  if (tab)
    return tab;

  return this;
}

BrowserRootView::DropIndex TabContainer::GetDropIndex(
    const ui::DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());

  std::vector<TabSlotView*> views = layout_helper()->GetTabSlotViews();

  // Loop until we find a tab or group header that intersects |event|'s
  // location.
  for (TabSlotView* view : views) {
    const int max_x = view->x() + view->width();
    if (x >= max_x)
      continue;

    if (view->GetTabSlotViewType() == TabSlotView::ViewType::kTab) {
      Tab* const tab = static_cast<Tab*>(view);
      // Closing tabs should be skipped.
      if (tab->closing())
        continue;

      // GetModelIndexOf is an O(n) operation. Since we will definitely
      // return from the loop at this point, it is only called once.
      // Hence the loop is still O(n). Calling this every loop iteration
      // must be avoided since it will become O(n^2).
      const int model_index = GetModelIndexOf(tab);
      const bool first_in_group =
          tab->group().has_value() &&
          model_index == controller_->GetFirstTabInGroup(tab->group().value());

      // When hovering over the left or right quarter of a tab, the drop
      // indicator will point between tabs.
      const int hot_width = tab->width() / 4;

      if (x >= (max_x - hot_width))
        return {model_index + 1, true /* drop_before */,
                false /* drop_in_group */};
      else if (x < tab->x() + hot_width)
        return {model_index, true /* drop_before */, first_in_group};
      else
        return {model_index, false /* drop_before */,
                false /* drop_in_group */};
    } else {
      TabGroupHeader* const group_header = static_cast<TabGroupHeader*>(view);
      const int first_tab_index =
          controller_->GetFirstTabInGroup(group_header->group().value())
              .value();

      if (x < max_x - group_header->width() / 2)
        return {first_tab_index, true /* drop_before */,
                false /* drop_in_group */};
      else
        return {first_tab_index, true /* drop_before */,
                true /* drop_in_group */};
    }
  }

  // The drop isn't over a tab, add it to the end.
  return {GetTabCount(), true, false};
}

views::View* TabContainer::GetViewForDrop() {
  return this;
}

BrowserRootView::DropTarget* TabContainer::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  if (IsDrawn()) {
    // Allow the drop as long as the mouse is over tab container or vertically
    // before it.
    if (loc_in_local_coords.y() < height())
      return this;
  }

  return nullptr;
}

void TabContainer::HandleDragUpdate(
    const absl::optional<BrowserRootView::DropIndex>& index) {
  SetDropArrow(index);
}

void TabContainer::HandleDragExited() {
  SetDropArrow({});
}

views::View* TabContainer::TargetForRect(views::View* root,
                                         const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  const gfx::Point point(rect.CenterPoint());

  // Return any view that isn't a Tab or this TabStrip immediately. We don't
  // want to interfere.
  views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
  if (v && v != this && !views::IsViewClass<Tab>(v))
    return v;

  views::View* tab = FindTabHitByPoint(point);
  if (tab)
    return tab;

  return this;
}

void TabContainer::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
}

void TabContainer::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  // Send the Container a message to simulate a mouse moved event at the current
  // mouse position. This tickles the Tab the mouse is currently over to show
  // the "hot" state of the close button, or to show the hover card, etc.  Note
  // that this is not required (and indeed may crash!) during a drag session.
  if (!IsDragSessionActive()) {
    // The widget can apparently be null during shutdown.
    views::Widget* widget = GetWidget();
    if (widget)
      widget->SynthesizeMouseMoveEvent();
  }
}

// TabContainer::DropArrow:
// ----------------------------------------------------------

TabContainer::DropArrow::DropArrow(const BrowserRootView::DropIndex& index,
                                   bool point_down,
                                   views::Widget* context)
    : index_(index), point_down_(point_down) {
  arrow_window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.bounds = gfx::Rect(g_drop_indicator_width, g_drop_indicator_height);
  params.context = context->GetNativeWindow();
  arrow_window_->Init(std::move(params));
  arrow_view_ =
      arrow_window_->SetContentsView(std::make_unique<views::ImageView>());
  arrow_view_->SetImage(GetDropArrowImage(point_down_));
  scoped_observation_.Observe(arrow_window_.get());

  arrow_window_->Show();
}

TabContainer::DropArrow::~DropArrow() {
  // Close eventually deletes the window, which deletes arrow_view too.
  if (arrow_window_)
    arrow_window_->Close();
}

void TabContainer::DropArrow::SetPointDown(bool down) {
  if (point_down_ == down)
    return;

  point_down_ = down;
  arrow_view_->SetImage(GetDropArrowImage(point_down_));
}

void TabContainer::DropArrow::SetWindowBounds(const gfx::Rect& bounds) {
  arrow_window_->SetBounds(bounds);
}

void TabContainer::DropArrow::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(scoped_observation_.IsObservingSource(arrow_window_.get()));
  scoped_observation_.Reset();
  arrow_window_ = nullptr;
}

void TabContainer::PrepareForAnimation() {
  if (!IsDragSessionActive() &&
      !TabDragController::IsAttachedTo(drag_context_)) {
    for (int i = 0; i < GetTabCount(); ++i)
      GetTabAtModelIndex(i)->set_dragging(false);
  }
}

void TabContainer::UpdateIdealBounds() {
  if (GetTabCount() == 0)
    return;  // Should only happen during creation/destruction, ignore.

  // Update |last_available_width_| in case there is a different amount of
  // available width than there was in the last layout (e.g. if the tabstrip
  // is currently hidden).
  last_available_width_ = GetAvailableWidthForTabContainer();

  const int available_width_for_tabs = CalculateAvailableWidthForTabs();
  layout_helper()->UpdateIdealBounds(available_width_for_tabs);
}

void TabContainer::AnimateToIdealBounds() {
  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kAnimating);

  for (int i = 0; i < GetTabCount(); ++i) {
    // If the tab is being dragged manually, skip it.
    Tab* tab = GetTabAtModelIndex(i);
    if (tab->dragging() && !bounds_animator().IsAnimating(tab))
      continue;

    // Also skip tabs already being animated to the same ideal bounds.  Calling
    // AnimateViewTo() again restarts the animation, which changes the timing of
    // how the tab animates, leading to hitches.
    const gfx::Rect& target_bounds = tabs_view_model_.ideal_bounds(i);
    if (bounds_animator().GetTargetBounds(tab) == target_bounds)
      continue;

    // Set an animation delegate for the tab so it will clip appropriately.
    // Don't do this if dragging() is true.  In this case the tab was
    // previously being dragged and is now animating back to its ideal
    // bounds; it already has an associated ResetDraggingStateDelegate that
    // will reset this dragging state. Replacing this delegate would mean
    // this code would also need to reset the dragging state immediately,
    // and that could allow the new tab button to be drawn atop this tab.
    if (bounds_animator().IsAnimating(tab) && tab->dragging()) {
      bounds_animator().SetTargetBounds(tab, target_bounds);
    } else {
      bounds_animator().AnimateViewTo(
          tab, target_bounds,
          std::make_unique<TabSlotAnimationDelegate>(
              this, tab,
              base::BindRepeating(&TabContainer::OnTabSlotAnimationProgressed,
                                  base::Unretained(this))));
    }
  }

  for (const auto& header_pair : group_views()) {
    TabGroupHeader* const header = header_pair.second->header();

    // If the header is being dragged manually, skip it.
    if (header->dragging() && !bounds_animator().IsAnimating(header))
      continue;

    bounds_animator().AnimateViewTo(
        header,
        layout_helper()->group_header_ideal_bounds().at(header_pair.first),
        std::make_unique<TabSlotAnimationDelegate>(
            this, header,
            base::BindRepeating(&TabContainer::OnTabSlotAnimationProgressed,
                                base::Unretained(this))));
  }

  // Because the preferred size of the tabstrip depends on the IsAnimating()
  // condition, but starting an animation doesn't necessarily invalidate the
  // existing preferred size and layout (which may now be incorrect), we need to
  // signal this explicitly.
  PreferredSizeChanged();
}

void TabContainer::SnapToIdealBounds() {
  for (int i = 0; i < GetTabCount(); ++i)
    GetTabAtModelIndex(i)->SetBoundsRect(tabs_view_model_.ideal_bounds(i));

  for (const auto& header_pair : group_views()) {
    header_pair.second->header()->SetBoundsRect(
        layout_helper()->group_header_ideal_bounds().at(header_pair.first));
    header_pair.second->UpdateBounds();
  }

  PreferredSizeChanged();
}

int TabContainer::CalculateAvailableWidthForTabs() const {
  return override_available_width_for_tabs_.value_or(
      GetAvailableWidthForTabContainer());
}

void TabContainer::StartInsertTabAnimation(int model_index) {
  ExitTabClosingMode();

  gfx::Rect bounds = GetTabAtModelIndex(model_index)->bounds();
  bounds.set_height(GetLayoutConstant(TAB_HEIGHT));

  // Adjust the starting bounds of the new tab.
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (model_index > 0) {
    // If we have a tab to our left, start at its right edge.
    bounds.set_x(GetTabAtModelIndex(model_index - 1)->bounds().right() -
                 tab_overlap);
  } else if (model_index + 1 < GetTabCount()) {
    // Otherwise, if we have a tab to our right, start at its left edge.
    bounds.set_x(GetTabAtModelIndex(model_index + 1)->bounds().x());
  } else {
    NOTREACHED() << "First tab inserted into the tabstrip should not animate.";
  }

  // Start at the width of the overlap in order to animate at the same speed
  // the surrounding tabs are moving, since at this width the subsequent tab
  // is naturally positioned at the same X coordinate.
  bounds.set_width(tab_overlap);
  GetTabAtModelIndex(model_index)->SetBoundsRect(bounds);

  // Animate in to the full width.
  StartBasicAnimation();
}

void TabContainer::StartRemoveTabAnimation(Tab* tab, int former_model_index) {
  if (in_tab_close_ && GetTabCount() > 0 &&
      override_available_width_for_tabs_ >
          tabs_view_model_.ideal_bounds(GetTabCount() - 1).right()) {
    // Tab closing mode is no longer constraining tab widths - they're at full
    // size. Exit tab closing mode so that it doesn't artificially inflate our
    // bounds.
    ExitTabClosingMode();
  }

  StartBasicAnimation();

  const int tab_overlap = TabStyle::GetTabOverlap();

  // TODO(pkasting): When closing multiple tabs, we get repeated RemoveTabAt()
  // calls, each of which closes a new tab and thus generates different ideal
  // bounds.  We should update the animations of any other tabs that are
  // currently being closed to reflect the new ideal bounds, or else change from
  // removing one tab at a time to animating the removal of all tabs at once.

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
  bounds_animator().AnimateViewTo(
      tab, target_bounds,
      std::make_unique<RemoveTabDelegate>(
          this, tab,
          base::BindRepeating(&TabContainer::OnTabSlotAnimationProgressed,
                              base::Unretained(this))));
}

void TabContainer::RemoveTabFromViewModel(int index) {
  Tab* tab = GetTabAtModelIndex(index);
  bool tab_was_active = tab->IsActive();

  UpdateHoverCard(nullptr, TabSlotController::HoverCardUpdateType::kTabRemoved);

  tabs_view_model_.Remove(index);
  layout_helper_->RemoveTabAt(index, tab);

  if (tab_was_active)
    tab->ActiveStateChanged();
}

void TabContainer::OnTabCloseAnimationCompleted(Tab* tab) {
  DCHECK(tab->closing());

  std::unique_ptr<Tab> deleter(tab);
  layout_helper_->OnTabDestroyed(tab);
}

void TabContainer::UpdateClosingModeOnRemovedTab(int model_index,
                                                 bool was_active) {
  // The tab at |model_index| has already been removed from the model, but is
  // still in |tabs_view_model_|.  Index math with care!
  const int model_count = GetTabCount() - 1;
  const int tab_overlap = TabStyle::GetTabOverlap();
  if (in_tab_close() && model_count > 0 && model_index != model_count) {
    // The user closed a tab other than the last tab. Set
    // override_available_width_for_tabs_ so that as the user closes tabs with
    // the mouse a tab continues to fall under the mouse.
    int next_active_index = controller_->GetActiveIndex();
    DCHECK(IsValidModelIndex(next_active_index));
    if (model_index <= next_active_index) {
      // At this point, model's internal state has already been updated.
      // |contents| has been detached from model and the active index has been
      // updated. But the tab for |contents| isn't removed yet. Thus, we need to
      // fix up next_active_index based on it.
      next_active_index++;
    }
    Tab* next_active_tab = GetTabAtModelIndex(next_active_index);
    Tab* tab_being_removed = GetTabAtModelIndex(model_index);

    int size_delta = tab_being_removed->width();
    if (!tab_being_removed->data().pinned && was_active &&
        layout_helper_->active_tab_width() >
            layout_helper_->inactive_tab_width()) {
      // When removing an active, non-pinned tab, an inactive tab will be made
      // active and thus given the active width. Thus the width being removed
      // from the container is really the current width of whichever inactive
      // tab will be made active.
      size_delta = next_active_tab->width();
    }

    override_available_width_for_tabs_ =
        tabs_view_model_.ideal_bounds(model_count).right() - size_delta +
        tab_overlap;
  }
}

void TabContainer::MoveGroupHeader(TabGroupHeader* group_header,
                                   int first_tab_model_index) {
  const int header_index = GetIndexOf(group_header);
  const int first_tab_view_index =
      GetViewIndexForModelIndex(first_tab_model_index);

  // The header should be just before the first tab. If it isn't, reorder the
  // header such that it is. Note that the index to reorder to is different
  // depending on whether the header is before or after the tab, since the
  // header itself occupies an index.
  if (header_index < first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index - 1);
  if (header_index > first_tab_view_index - 1)
    ReorderChildView(group_header, first_tab_view_index);
}

void TabContainer::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (GetTabCount() == 0)
    return;

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
  StartBasicAnimation();
}

void TabContainer::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!IsDragSessionActive())
    ResizeLayoutTabs();
  else
    StartResizeLayoutTabsFromTouchTimer();
}

void TabContainer::StartResizeLayoutTabsFromTouchTimer() {
  // Amount of time we delay before resizing after a close from a touch.
  constexpr auto kTouchResizeLayoutTime = base::Seconds(2);

  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(FROM_HERE, kTouchResizeLayoutTime, this,
                             &TabContainer::ResizeLayoutTabsFromTouch);
}

bool TabContainer::IsDragSessionActive() const {
  // |drag_context_| may be null in tests.
  return drag_context_ && drag_context_->IsDragSessionActive();
}

void TabContainer::AddMessageLoopObserver() {
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
            this, gfx::Insets::TLBR(
                      0, base::i18n::IsRTL() ? kTabStripAnimationHSlop : 0,
                      kTabStripAnimationVSlop,
                      base::i18n::IsRTL() ? 0 : kTabStripAnimationHSlop)),
        this);
  }
  mouse_watcher_->Start(GetWidget()->GetNativeWindow());
}

void TabContainer::RemoveMessageLoopObserver() {
  mouse_watcher_ = nullptr;
}

int TabContainer::GetViewInsertionIndex(
    absl::optional<tab_groups::TabGroupId> group,
    absl::optional<int> from_model_index,
    int to_model_index) const {
  // -1 is treated a sentinel value to indicate a tab is newly added to the
  // beginning of the tab strip.
  if (to_model_index < 0)
    return 0;

  // If to_model_index is beyond the end of the tab strip, then the tab is
  // newly added to the end of the tab strip. In that case we can just return
  // one beyond the view index of the last existing tab.
  if (to_model_index >= GetTabCount())
    return (GetTabCount() ? GetViewIndexForModelIndex(GetTabCount() - 1) + 1
                          : 0);

  // If there is no from_model_index, then the tab is newly added in the
  // middle of the tab strip. In that case we treat it as coming from the end
  // of the tab strip, since new views are ordered at the end by default.
  if (!from_model_index.has_value())
    from_model_index = GetTabCount();

  DCHECK_NE(to_model_index, from_model_index.value());

  // Since we don't have an absolute mapping from model index to view index,
  // we anchor on the last known view index at the given to_model_index.
  Tab* other_tab = GetTabAtModelIndex(to_model_index);
  int other_view_index = GetViewIndexForModelIndex(to_model_index);

  if (other_view_index <= 0)
    return 0;

  // When moving to the right, just use the anchor index because the tab will
  // replace that position in both the model and the view. This happens
  // because the tab itself occupies a lower index that the other tabs will
  // shift into.
  if (to_model_index > from_model_index.value())
    return other_view_index;

  // When moving to the left, the tab may end up on either the left or right
  // side of a group header, depending on if it's in that group. This affects
  // its view index but not its model index, so we adjust the former only.
  if (other_tab->group().has_value() && other_tab->group() != group)
    return other_view_index - 1;

  return other_view_index;
}

int TabContainer::GetViewIndexForModelIndex(int model_index) const {
  return GetIndexOf(GetTabAtModelIndex(model_index));
}

bool TabContainer::IsPointInTab(Tab* tab,
                                const gfx::Point& point_in_tabstrip_coords) {
  if (!tab->GetVisible())
    return false;
  gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
  View::ConvertPointToTarget(this, tab, &point_in_tab_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

Tab* TabContainer::FindTabHitByPoint(const gfx::Point& point) {
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
    if (!IsPointInTab(tab, point))
      continue;

    // Selected tabs render atop unselected ones, and active tabs render atop
    // everything.  Check whether the next tab renders atop this one and |point|
    // is in the overlap region.
    Tab* next_tab = i < (all_tabs.size() - 1) ? all_tabs[i + 1] : nullptr;
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

bool TabContainer::ShouldTabBeVisible(const Tab* tab) const {
  // When the tabstrip is scrollable, it can grow to accommodate any number of
  // tabs, so tabs can never become clipped.
  // N.B. Tabs can still be not-visible because they're in a collapsed group,
  // but that's handled elsewhere.
  // N.B. This is separate from the tab being potentially scrolled offscreen -
  // this solely determines whether the tab should be clipped for the
  // pre-scrolling overflow behavior.
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip))
    return true;

  // Detached tabs should always be invisible (as they close).
  if (tab->detached())
    return false;

  // If the tab would be clipped by the trailing edge of the strip, even if the
  // tabstrip were resized to its greatest possible width, it shouldn't be
  // visible.
  int right_edge = tab->bounds().right();
  const int tabstrip_right = tab->dragging()
                                 ? drag_context_->GetTabDragAreaWidth()
                                 : GetAvailableWidthForTabContainer();
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
  if (controller_->GetActiveIndex() <= GetModelIndexOf(tab))
    return true;

  // We need to check what would happen if the active tab were to move to this
  // tab or before. If animating, we want to use the target bounds in this
  // calculation.
  if (bounds_animator_.IsAnimating())
    right_edge = bounds_animator_.GetTargetBounds(tab).right();
  return (right_edge + layout_helper_->active_tab_width() -
          layout_helper_->inactive_tab_width()) <= tabstrip_right;
}

gfx::Rect TabContainer::GetDropBounds(int drop_index,
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

  const int overlap = TabStyle::GetTabOverlap();
  if (!drop_before || !first_in_group || drop_in_group) {
    // Dropping between tabs, or between a group header and the group's first
    // tab.
    center_x = tab->x();
    const int width = tab->width();
    if (drop_index < GetTabCount())
      center_x += drop_before ? (overlap / 2) : (width / 2);
    else
      center_x += width - (overlap / 2);
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
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());

  return drop_bounds;
}

void TabContainer::SetDropArrow(
    const absl::optional<BrowserRootView::DropIndex>& index) {
  if (!index) {
    controller_->OnDropIndexUpdate(-1, false);
    drop_arrow_.reset();
    return;
  }

  // Let the controller know of the index update.
  controller_->OnDropIndexUpdate(index->value, index->drop_before);

  if (drop_arrow_ && (index == drop_arrow_->index()))
    return;

  bool is_beneath;
  gfx::Rect drop_bounds = GetDropBounds(index->value, index->drop_before,
                                        index->drop_in_group, &is_beneath);

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(*index, !is_beneath, GetWidget());
  } else {
    drop_arrow_->set_index(*index);
    drop_arrow_->SetPointDown(!is_beneath);
  }

  // Reposition the window.
  drop_arrow_->SetWindowBounds(drop_bounds);
}

void TabContainer::UpdateAccessibleTabIndices() {
  const int num_tabs = GetTabCount();
  for (int i = 0; i < num_tabs; ++i) {
    GetTabAtModelIndex(i)->GetViewAccessibility().OverridePosInSet(i + 1,
                                                                   num_tabs);
  }
}

bool TabContainer::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

BEGIN_METADATA(TabContainer, views::View)
ADD_READONLY_PROPERTY_METADATA(int, AvailableWidthForTabContainer)
END_METADATA
