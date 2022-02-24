// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"

#include "base/bits.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_utils.h"

namespace {

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
                           TabHoverCardController* hover_card_controller)
    : controller_(controller),
      hover_card_controller_(hover_card_controller),
      bounds_animator_(this),
      layout_helper_(std::make_unique<TabStripLayoutHelper>(
          controller,
          base::BindRepeating(&TabContainer::tabs_view_model,
                              base::Unretained(this)))) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

TabContainer::~TabContainer() {
  // Since TabGroupViews expects be able to remove the views it creates, clear
  // |group_views_| before removing the remaining children below.
  group_views_.clear();

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
  return tab_ptr;
}

void TabContainer::MoveTab(Tab* tab, int from_model_index, int to_model_index) {
  ReorderChildView(tab, GetViewInsertionIndex(tab->group(), from_model_index,
                                              to_model_index));
  tabs_view_model_.Move(from_model_index, to_model_index);
  layout_helper_->MoveTab(tab->group(), from_model_index, to_model_index);
}

void TabContainer::RemoveTabFromViewModel(int index) {
  UpdateHoverCard(nullptr, TabController::HoverCardUpdateType::kTabRemoved);

  Tab* tab = GetTabAtModelIndex(index);
  tabs_view_model_.Remove(index);
  layout_helper_->RemoveTabAt(index, tab);
}

void TabContainer::OnGroupCreated(const tab_groups::TabGroupId& group,
                                  TabStrip* tab_strip) {
  auto group_view = std::make_unique<TabGroupViews>(this, tab_strip, group);
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

void TabContainer::UpdateTabGroupVisuals(tab_groups::TabGroupId group_id) {
  const auto group_views = group_views_.find(group_id);
  if (group_views != group_views_.end())
    group_views->second->UpdateBounds();
}

int TabContainer::GetModelIndexOf(const TabSlotView* slot_view) {
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
    TabController::HoverCardUpdateType update_type) {
  // Some operations (including e.g. starting a drag) can cause the tab focus
  // to change at the same time as the tabstrip is starting to animate; the
  // hover card should not be visible at this time.
  // See crbug.com/1220840 for an example case.
  if (bounds_animator_.IsAnimating()) {
    tab = nullptr;
    update_type = TabController::HoverCardUpdateType::kAnimating;
  }

  if (!hover_card_controller_)
    return;

  hover_card_controller_->UpdateHoverCard(tab, update_type);
}

void TabContainer::UpdateAccessibleTabIndices() {
  const int num_tabs = GetTabCount();
  for (int i = 0; i < num_tabs; ++i)
    GetTabAtModelIndex(i)->GetViewAccessibility().OverridePosInSet(i + 1,
                                                                   num_tabs);
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

void TabContainer::AnimateToIdealBounds() {
  UpdateHoverCard(nullptr, TabController::HoverCardUpdateType::kAnimating);

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

int TabContainer::CalculateAvailableWidthForTabs() const {
  return override_available_width_for_tabs_.value_or(
      GetAvailableWidthForTabContainer());
}

int TabContainer::GetAvailableWidthForTabContainer() const {
  // Falls back to views::View::GetAvailableSize() when
  // |available_width_callback_| is not defined, e.g. when tab scrolling is
  // disabled.
  return available_width_callback_
             ? available_width_callback_.Run()
             : parent()->GetAvailableSize(this).width().value();
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

void TabContainer::AnimateTabClosed(Tab* tab, int former_model_index) {
  if (in_tab_close_ && GetTabCount() > 0 &&
      override_available_width_for_tabs_ >
          tabs_view_model_.ideal_bounds(GetTabCount() - 1).right()) {
    // Tab closing mode is no longer constraining tab widths - they're at full
    // size. Exit tab closing mode so that it doesn't artificially inflate our
    // bounds.
    ExitTabClosingMode();
  }

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

void TabContainer::EnterTabClosingMode(absl::optional<int> override_width) {
  in_tab_close_ = true;
  override_available_width_for_tabs_ = override_width;
}

void TabContainer::ExitTabClosingMode() {
  in_tab_close_ = false;
  override_available_width_for_tabs_.reset();
}

void TabContainer::OnTabWillBeRemovedAt(int model_index, bool was_active) {
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

void TabContainer::OnTabCloseAnimationCompleted(Tab* tab) {
  DCHECK(tab->closing());

  std::unique_ptr<Tab> deleter(tab);
  layout_helper_->OnTabDestroyed(tab);
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

bool TabContainer::IsValidModelIndex(int model_index) const {
  return controller_->IsValidIndex(model_index);
}

BEGIN_METADATA(TabContainer, views::View)
ADD_READONLY_PROPERTY_METADATA(int, AvailableWidthForTabContainer)
END_METADATA
