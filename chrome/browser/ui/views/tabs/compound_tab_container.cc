// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/compound_tab_container.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_container_impl.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_scrolling_animation.h"
#include "chrome/browser/ui/views/tabs/tab_slot_animation_delegate.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
class PinnedTabContainerController final : public TabContainerController {
 public:
  explicit PinnedTabContainerController(
      TabContainerController& base_controller,
      CompoundTabContainer& compound_tab_container)
      : base_controller_(base_controller),
        compound_tab_container_(compound_tab_container) {}

  ~PinnedTabContainerController() override = default;

  bool IsValidModelIndex(int index) const override {
    return base_controller_->IsValidModelIndex(index) &&
           index < NumPinnedTabsInModel();
  }

  std::optional<int> GetActiveIndex() const override {
    const std::optional<int> active_index = base_controller_->GetActiveIndex();
    if (active_index.has_value() && !IsValidModelIndex(active_index.value())) {
      return std::nullopt;
    }
    return base_controller_->GetActiveIndex();
  }

  int NumPinnedTabsInModel() const override {
    return base_controller_->NumPinnedTabsInModel();
  }

  void OnDropIndexUpdate(const std::optional<int> index,
                         const bool drop_before) override {
    base_controller_->OnDropIndexUpdate(index, drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    NOTREACHED();  // Pinned container can't have groups.
  }

  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override {
    NOTREACHED();  // Pinned container can't have groups.
  }

  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override {
    NOTREACHED();  // Pinned container can't have groups.
  }

  bool CanExtendDragHandle() const override {
    return base_controller_->CanExtendDragHandle();
  }

  const views::View* GetTabClosingModeMouseWatcherHostView() const override {
    return base_controller_->GetTabClosingModeMouseWatcherHostView();
  }

  bool IsAnimatingInTabStrip() const override {
    return base_controller_->IsAnimatingInTabStrip();
  }

  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             gfx::Rect target_bounds) override {
    compound_tab_container_->UpdateAnimationTarget(tab_slot_view, target_bounds,
                                                   TabPinned::kPinned);
  }

 private:
  const raw_ref<TabContainerController> base_controller_;
  const raw_ref<CompoundTabContainer> compound_tab_container_;
};

class UnpinnedTabContainerController final : public TabContainerController {
 public:
  explicit UnpinnedTabContainerController(
      TabContainerController& base_controller,
      CompoundTabContainer& compound_tab_container)
      : base_controller_(base_controller),
        compound_tab_container_(compound_tab_container) {}

  ~UnpinnedTabContainerController() override = default;

  bool IsValidModelIndex(int index) const override {
    return ContainerToModelIndex(index).has_value();
  }

  std::optional<int> GetActiveIndex() const override {
    const std::optional<int> base_model_active_index =
        base_controller_->GetActiveIndex();
    if (base_model_active_index.has_value()) {
      return ModelToContainerIndex(base_model_active_index.value());
    }
    return std::nullopt;
  }

  int NumPinnedTabsInModel() const override { return 0; }

  void OnDropIndexUpdate(const std::optional<int> index,
                         const bool drop_before) override {
    // We can't use ContainerIndexToModelIndex here because `index` might be
    // after the last tab (i.e. the drop would open a new tab at the end of the
    // tabstrip).
    std::optional<int> model_index = std::nullopt;
    if (index.has_value() && index > 0) {
      model_index = index.value() + base_controller_->NumPinnedTabsInModel();
      // The adjusted index must be either a valid index in the model, or be the
      // next index after the end of the model.
      CHECK(base_controller_->IsValidModelIndex(model_index.value()) ||
            base_controller_->IsValidModelIndex(model_index.value() - 1));
    }

    base_controller_->OnDropIndexUpdate(model_index, drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    return base_controller_->IsGroupCollapsed(group);
  }

  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override {
    const std::optional<int> model_index =
        base_controller_->GetFirstTabInGroup(group);
    if (!model_index) {
      return std::nullopt;
    }
    return ModelToContainerIndex(model_index.value());
  }

  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override {
    const gfx::Range model_range = base_controller_->ListTabsInGroup(group);
    return gfx::Range(ModelToContainerIndex(model_range.start()).value(),
                      ModelToContainerIndex(model_range.end() - 1).value());
  }

  bool CanExtendDragHandle() const override {
    return base_controller_->CanExtendDragHandle();
  }

  const views::View* GetTabClosingModeMouseWatcherHostView() const override {
    return base_controller_->GetTabClosingModeMouseWatcherHostView();
  }

  bool IsAnimatingInTabStrip() const override {
    return base_controller_->IsAnimatingInTabStrip();
  }

  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             gfx::Rect target_bounds) override {
    compound_tab_container_->UpdateAnimationTarget(tab_slot_view, target_bounds,
                                                   TabPinned::kUnpinned);
  }

 private:
  std::optional<int> ModelToContainerIndex(int model_index) const {
    if (model_index < base_controller_->NumPinnedTabsInModel() ||
        !base_controller_->IsValidModelIndex(model_index)) {
      return std::nullopt;
    }
    return model_index - base_controller_->NumPinnedTabsInModel();
  }

  std::optional<int> ContainerToModelIndex(int container_index) const {
    if (container_index < 0) {
      return std::nullopt;
    }
    const int model_index =
        container_index + base_controller_->NumPinnedTabsInModel();
    if (!base_controller_->IsValidModelIndex(model_index)) {
      return std::nullopt;
    }
    return model_index;
  }

  const raw_ref<TabContainerController> base_controller_;
  const raw_ref<CompoundTabContainer> compound_tab_container_;
};

// Animates tabs being pinned or unpinned, then hands them back to
// |tab_container_|.
class PinUnpinAnimationDelegate : public TabSlotAnimationDelegate {
 public:
  PinUnpinAnimationDelegate(TabContainer* tab_container, TabSlotView* slot_view)
      : TabSlotAnimationDelegate(tab_container, slot_view) {}
  PinUnpinAnimationDelegate(const PinUnpinAnimationDelegate&) = delete;
  PinUnpinAnimationDelegate& operator=(const PinUnpinAnimationDelegate&) =
      delete;
  ~PinUnpinAnimationDelegate() override = default;

  void AnimationEnded(const gfx::Animation* animation) override {
    TabSlotAnimationDelegate::AnimationEnded(animation);
    tab_container()->ReturnTabSlotView(slot_view());
  }
};
}  // namespace

CompoundTabContainer::CompoundTabContainer(
    TabContainerController& controller,
    TabHoverCardController* hover_card_controller,
    TabDragContextBase* drag_context,
    TabSlotController& tab_slot_controller,
    views::View* scroll_contents_view)
    : controller_(controller),
      pinned_tab_container_controller_(
          std::make_unique<PinnedTabContainerController>(controller, *this)),
      pinned_tab_container_(*AddChildView(std::make_unique<TabContainerImpl>(
          *(pinned_tab_container_controller_.get()),
          hover_card_controller,
          drag_context,
          tab_slot_controller,
          scroll_contents_view))),
      unpinned_tab_container_controller_(
          std::make_unique<UnpinnedTabContainerController>(controller, *this)),
      unpinned_tab_container_(*AddChildView(std::make_unique<TabContainerImpl>(
          *(unpinned_tab_container_controller_.get()),
          hover_card_controller,
          drag_context,
          tab_slot_controller,
          scroll_contents_view))),
      hover_card_controller_(hover_card_controller),
      scroll_contents_view_(scroll_contents_view),
      bounds_animator_(this) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    bounds_animator_.SetAnimationDuration(base::TimeDelta());
  }
}

CompoundTabContainer::~CompoundTabContainer() {
  // Tabs call back up to the TabStrip during animation end and destruction.
  // Ensure that happens now so we aren't in a half-destructed state when they
  // do so.
  CancelAnimation();
  RemoveChildViewT(base::to_address(pinned_tab_container_));
  RemoveChildViewT(base::to_address(unpinned_tab_container_));
}

void CompoundTabContainer::SetAvailableWidthCallback(
    base::RepeatingCallback<int()> available_width_callback) {
  // Store this ourselves, and let our child containers fall back to calling
  // GetAvailableSize.
  available_width_callback_ = available_width_callback;
}

Tab* CompoundTabContainer::AddTab(std::unique_ptr<Tab> tab,
                                  int model_index,
                                  TabPinned pinned) {
  if (pinned == TabPinned::kPinned) {
    CHECK_LE(model_index, NumPinnedTabs());
    return pinned_tab_container_->AddTab(std::move(tab), model_index, pinned);
  }
  CHECK_GE(model_index, NumPinnedTabs());
  return unpinned_tab_container_->AddTab(std::move(tab),
                                         model_index - NumPinnedTabs(), pinned);
}

void CompoundTabContainer::MoveTab(int from_model_index, int to_model_index) {
  const bool prev_pinned = from_model_index < NumPinnedTabs();
  // The tab's TabData has already been updated at this point to reflect its
  // next pinned status. Consistency with `to_model_index` is verified below.
  const bool next_pinned = GetTabAtModelIndex(from_model_index)->data().pinned;

  // If the tab was pinned/unpinned as part of this move, we will need to
  // transfer it between our TabContainers.
  if (prev_pinned != next_pinned) {
    TransferTabBetweenContainers(from_model_index, to_model_index);
  } else if (prev_pinned) {
    CHECK_LT(to_model_index, NumPinnedTabs());
    pinned_tab_container_->MoveTab(from_model_index, to_model_index);
  } else {  // !prev_pinned
    CHECK_GE(to_model_index, NumPinnedTabs());
    unpinned_tab_container_->MoveTab(from_model_index - NumPinnedTabs(),
                                     to_model_index - NumPinnedTabs());
  }
}

void CompoundTabContainer::RemoveTab(int index, bool was_active) {
  CHECK(IsValidViewModelIndex(index)) << index;
  if (index < NumPinnedTabs()) {
    pinned_tab_container_->RemoveTab(index, was_active);
  } else {
    unpinned_tab_container_->RemoveTab(index - NumPinnedTabs(), was_active);
  }
}

void CompoundTabContainer::SetTabPinned(int model_index, TabPinned pinned) {
  // This method does not support reorders, so the tab must already be at a
  // location that can hold either a pinned or an unpinned tab, i.e. the border
  // between the pinned and unpinned subsets.
  CHECK_EQ(model_index,
           pinned == TabPinned::kPinned ? NumPinnedTabs() : NumPinnedTabs() - 1)
      << "Cannot " << (pinned == TabPinned::kPinned ? "pin" : "unpin")
      << " the tab at model index " << model_index << " when there are "
      << NumPinnedTabs() << " pinned tabs without moving that tab."
      << " Use MoveTab to move and (un)pin a tab at the same time.";
  // The tab's data must already have been updated.
  DCHECK_EQ(pinned == TabPinned::kPinned,
            GetTabAtModelIndex(model_index)->data().pinned);
  TransferTabBetweenContainers(model_index, model_index);
}

void CompoundTabContainer::SetActiveTab(
    std::optional<size_t> prev_active_index,
    std::optional<size_t> new_active_index) {
  std::optional<size_t> prev_pinned_active_index;
  std::optional<size_t> new_pinned_active_index;
  std::optional<size_t> prev_unpinned_active_index;
  std::optional<size_t> new_unpinned_active_index;
  if (prev_active_index.has_value()) {
    if (prev_active_index < static_cast<size_t>(NumPinnedTabs())) {
      prev_pinned_active_index = prev_active_index;
    } else {
      prev_unpinned_active_index = prev_active_index.value() - NumPinnedTabs();
    }
  }
  if (new_active_index.has_value()) {
    if (new_active_index < static_cast<size_t>(NumPinnedTabs())) {
      new_pinned_active_index = new_active_index;
    } else {
      new_unpinned_active_index = new_active_index.value() - NumPinnedTabs();
    }
  }

  pinned_tab_container_->SetActiveTab(prev_pinned_active_index,
                                      new_pinned_active_index);
  unpinned_tab_container_->SetActiveTab(prev_unpinned_active_index,
                                        new_unpinned_active_index);
}

Tab* CompoundTabContainer::RemoveTabFromViewModel(int model_index) {
  // TODO(crbug.com/40882151): This only needs to be implemented in
  // TabContainerImpl.
  NOTREACHED();
}

Tab* CompoundTabContainer::AddTabToViewModel(Tab* tab,
                                             int model_index,
                                             TabPinned pinned) {
  // TODO(crbug.com/40882151): This only needs to be implemented in
  // TabContainerImpl.
  NOTREACHED();
}

void CompoundTabContainer::ReturnTabSlotView(TabSlotView* view) {
  GetTabContainerFor(view).ReturnTabSlotView(view);
}

void CompoundTabContainer::ScrollTabToVisible(int model_index) {
  // TODO(crbug.com/40060338): Implement. I guess.
}

void CompoundTabContainer::ScrollTabContainerByOffset(int offset) {
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

void CompoundTabContainer::OnGroupCreated(const tab_groups::TabGroupId& group) {
  unpinned_tab_container_->OnGroupCreated(group);
}

void CompoundTabContainer::OnGroupEditorOpened(
    const tab_groups::TabGroupId& group) {
  unpinned_tab_container_->OnGroupEditorOpened(group);
}

void CompoundTabContainer::OnGroupMoved(const tab_groups::TabGroupId& group) {
  unpinned_tab_container_->OnGroupMoved(group);
}

void CompoundTabContainer::OnGroupContentsChanged(
    const tab_groups::TabGroupId& group) {
  unpinned_tab_container_->OnGroupContentsChanged(group);
}

void CompoundTabContainer::OnGroupVisualsChanged(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData* old_visuals,
    const tab_groups::TabGroupVisualData* new_visuals) {
  unpinned_tab_container_->OnGroupVisualsChanged(group, old_visuals,
                                                 new_visuals);
}

void CompoundTabContainer::ToggleTabGroup(
    const tab_groups::TabGroupId& group,
    bool is_collapsing,
    ToggleTabGroupCollapsedStateOrigin origin) {
  unpinned_tab_container_->ToggleTabGroup(group, is_collapsing, origin);
}

void CompoundTabContainer::OnGroupClosed(const tab_groups::TabGroupId& group) {
  unpinned_tab_container_->OnGroupClosed(group);
}

void CompoundTabContainer::UpdateTabGroupVisuals(
    tab_groups::TabGroupId group_id) {
  unpinned_tab_container_->UpdateTabGroupVisuals(group_id);
}

void CompoundTabContainer::NotifyTabGroupEditorBubbleOpened() {
  unpinned_tab_container_->NotifyTabGroupEditorBubbleOpened();
}

void CompoundTabContainer::NotifyTabGroupEditorBubbleClosed() {
  unpinned_tab_container_->NotifyTabGroupEditorBubbleClosed();
}

std::optional<int> CompoundTabContainer::GetModelIndexOf(
    const TabSlotView* slot_view) const {
  const std::optional<int> unpinned_index =
      unpinned_tab_container_->GetModelIndexOf(slot_view);
  if (unpinned_index.has_value()) {
    return unpinned_index.value() + NumPinnedTabs();
  }
  return pinned_tab_container_->GetModelIndexOf(slot_view);
}

Tab* CompoundTabContainer::GetTabAtModelIndex(int index) const {
  CHECK_LT(index, GetTabCount());
  const int num_pinned_tabs = NumPinnedTabs();
  if (index < num_pinned_tabs) {
    return pinned_tab_container_->GetTabAtModelIndex(index);
  }
  return unpinned_tab_container_->GetTabAtModelIndex(index - num_pinned_tabs);
}

int CompoundTabContainer::GetTabCount() const {
  return pinned_tab_container_->GetTabCount() +
         unpinned_tab_container_->GetTabCount();
}

std::optional<int> CompoundTabContainer::GetModelIndexOfFirstNonClosingTab(
    Tab* tab) const {
  if (tab->data().pinned) {
    const std::optional<int> pinned_index =
        pinned_tab_container_->GetModelIndexOfFirstNonClosingTab(tab);

    // If there are no non-closing pinned tabs after `tab`, return the first
    // non-closing unpinned tab, if there is one (if the unpinned container is
    // empty or only has closing tabs, GetTabCount will be 0).
    if (!pinned_index.has_value() &&
        unpinned_tab_container_->GetTabCount() > 0) {
      return NumPinnedTabs();
    }
    return pinned_index;
  } else {
    const std::optional<int> unpinned_index =
        unpinned_tab_container_->GetModelIndexOfFirstNonClosingTab(tab);
    if (unpinned_index.has_value()) {
      return unpinned_index.value() + NumPinnedTabs();
    }
    return std::nullopt;
  }
}

void CompoundTabContainer::UpdateHoverCard(
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

void CompoundTabContainer::HandleLongTap(ui::GestureEvent* const event) {
  TabContainer* const tab_container = GetTabContainerAt(event->location());
  if (!tab_container) {
    return;
  }

  ConvertEventToTarget(tab_container, event);
  tab_container->HandleLongTap(event);
}

bool CompoundTabContainer::IsRectInContentArea(const gfx::Rect& rect) {
  if (pinned_tab_container_->IsRectInContentArea(ToEnclosingRect(
          ConvertRectToTarget(this, base::to_address(pinned_tab_container_),
                              gfx::RectF(rect))))) {
    return true;
  }

  return unpinned_tab_container_->IsRectInContentArea(
      ToEnclosingRect(ConvertRectToTarget(
          this, base::to_address(unpinned_tab_container_), gfx::RectF(rect))));
}

std::optional<ZOrderableTabContainerElement>
CompoundTabContainer::GetLeadingElementForZOrdering() const {
  // TODO(crbug.com/40882151): This only needs to be implemented in
  // TabContainerImpl.
  NOTREACHED();
}
std::optional<ZOrderableTabContainerElement>
CompoundTabContainer::GetTrailingElementForZOrdering() const {
  // TODO(crbug.com/40882151): This only needs to be implemented in
  // TabContainerImpl.
  NOTREACHED();
}

void CompoundTabContainer::OnTabSlotAnimationProgressed(TabSlotView* view) {
  GetTabContainerFor(view).OnTabSlotAnimationProgressed(view);
}

void CompoundTabContainer::OnTabCloseAnimationCompleted(Tab* tab) {
  // TODO(crbug.com/40882151): This only needs to be implemented in
  // TabContainerImpl.
  NOTREACHED();
}

void CompoundTabContainer::InvalidateIdealBounds() {
  pinned_tab_container_->InvalidateIdealBounds();
  unpinned_tab_container_->InvalidateIdealBounds();
}

void CompoundTabContainer::AnimateToIdealBounds() {
  // `pinned_tab_container_` must plan its animation first so
  // `unpinned_tab_container_` has up-to-date available width.
  pinned_tab_container_->AnimateToIdealBounds();
  unpinned_tab_container_->AnimateToIdealBounds();

  // Animate the pinning or unpinning tabs too.
  for (views::View* child : children()) {
    Tab* tab = views::AsViewClass<Tab>(child);
    if (!tab) {
      continue;
    }

    const std::optional<int> model_index = GetModelIndexOf(tab);
    // The tab may have been closed during a pin/unpin animation, in which case
    // it a) has no model index and b) is already animating to its correct
    // bounds because that will have been updated in `UpdateAnimationTarget()`.
    if (!model_index.has_value()) {
      continue;
    }

    AnimateTabTo(tab, GetIdealBounds(model_index.value()));
  }
}

bool CompoundTabContainer::IsAnimating() const {
  return bounds_animator_.IsAnimating() ||
         pinned_tab_container_->IsAnimating() ||
         unpinned_tab_container_->IsAnimating();
}

void CompoundTabContainer::CancelAnimation() {
  pinned_tab_container_->CancelAnimation();
  unpinned_tab_container_->CancelAnimation();
}

void CompoundTabContainer::CompleteAnimationAndLayout() {
  bounds_animator_.Complete();
  pinned_tab_container_->CompleteAnimationAndLayout();
  unpinned_tab_container_->CompleteAnimationAndLayout();
  DeprecatedLayoutImmediately();
}

int CompoundTabContainer::GetAvailableWidthForTabContainer() const {
  // Falls back to views::View::GetAvailableSize() when
  // |available_width_callback_| is not defined, e.g. when tab scrolling is
  // disabled.
  return available_width_callback_
             ? available_width_callback_.Run()
             : parent()->GetAvailableSize(this).width().value();
}

void CompoundTabContainer::EnterTabClosingMode(
    std::optional<int> override_width,
    CloseTabSource source) {
  if (override_width.has_value()) {
    override_width = override_width.value() -
                     pinned_tab_container_->GetPreferredSize().width();
  }

  // The pinned container can't be in closing mode, as pinned tabs don't resize.
  unpinned_tab_container_->EnterTabClosingMode(override_width, source);
}

void CompoundTabContainer::ExitTabClosingMode() {
  // The pinned container can't be in closing mode, as pinned tabs don't resize.
  unpinned_tab_container_->ExitTabClosingMode();
}

void CompoundTabContainer::SetTabSlotVisibility() {
  // TODO(crbug.com/40060338): Impl
}

bool CompoundTabContainer::InTabClose() {
  // The pinned container can't be in closing mode, as pinned tabs don't resize.
  return unpinned_tab_container_->InTabClose();
}

TabGroupViews* CompoundTabContainer::GetGroupViews(
    tab_groups::TabGroupId group_id) const {
  return unpinned_tab_container_->GetGroupViews(group_id);
}

const std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
CompoundTabContainer::get_group_views_for_testing() const {
  // Only the unpinned container can have groups.
  return unpinned_tab_container_->get_group_views_for_testing();  // IN-TEST
}

int CompoundTabContainer::GetActiveTabWidth() const {
  // Only the unpinned container has variable-width tabs.
  return unpinned_tab_container_->GetActiveTabWidth();
}

int CompoundTabContainer::GetInactiveTabWidth() const {
  // Only the unpinned container has variable-width tabs.
  return unpinned_tab_container_->GetInactiveTabWidth();
}

gfx::Rect CompoundTabContainer::GetIdealBounds(int model_index) const {
  // Ideal bounds for pinned tabs are fine as-is.
  if (model_index < NumPinnedTabs()) {
    return pinned_tab_container_->GetIdealBounds(model_index);
  }

  return ConvertUnpinnedContainerIdealBoundsToLocal(
      unpinned_tab_container_->GetIdealBounds(model_index - NumPinnedTabs()));
}

gfx::Rect CompoundTabContainer::GetIdealBounds(
    tab_groups::TabGroupId group) const {
  return ConvertUnpinnedContainerIdealBoundsToLocal(
      unpinned_tab_container_->GetIdealBounds(group));
}

gfx::Size CompoundTabContainer::GetMinimumSize() const {
  return GetCombinedSizeForTabContainerSizes(
      pinned_tab_container_->GetMinimumSize(),
      unpinned_tab_container_->GetMinimumSize());
}

views::SizeBounds CompoundTabContainer::GetAvailableSize(
    const views::View* child) const {
  if (child == base::to_address(pinned_tab_container_)) {
    return views::SizeBounds(GetAvailableWidthForTabContainer(),
                             views::SizeBound());
  }

  CHECK_EQ(child, base::to_address(unpinned_tab_container_));
  return views::SizeBounds(GetAvailableWidthForUnpinnedTabContainer(),
                           views::SizeBound());
}

gfx::Size CompoundTabContainer::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetCombinedSizeForTabContainerSizes(
      pinned_tab_container_->GetPreferredSize(),
      unpinned_tab_container_->GetPreferredSize());
}

views::View* CompoundTabContainer::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  TabContainer* const sub_container = GetTabContainerAt(point);
  return sub_container ? sub_container->GetTooltipHandlerForPoint(
                             ConvertPointToTarget(this, sub_container, point))
                       : this;
}

void CompoundTabContainer::Layout(PassKey) {
  // Pinned container gets however much space it wants.
  pinned_tab_container_->SetBoundsRect(
      gfx::Rect(pinned_tab_container_->GetPreferredSize()));

  // Unpinned container can have whatever is left over.
  const int unpinned_container_leading_x = std::max(
      0, pinned_tab_container_->width() - TabStyle::Get()->GetTabOverlap());
  const int available_width = width() - unpinned_container_leading_x;

  const gfx::Size pref_size = unpinned_tab_container_->GetPreferredSize();

  unpinned_tab_container_->SetBounds(
      unpinned_container_leading_x, 0,
      std::min(available_width, pref_size.width()), pref_size.height());
}

void CompoundTabContainer::PaintChildren(const views::PaintInfo& paint_info) {
  TRACE_EVENT1("views", "View::PaintChildren", "class", GetClassName());

  // N.B. We override PaintChildren only to define paint order for our children.
  // We do this instead of GetChildrenInZOrder for consistency with
  // TabContainerImpl.

  // Paint our containers first, ordered based on their overlapping elements.
  // I.e., the last tab in `pinned_tab_container_` will overlap the first tab
  // (or group header) in `unpinned_tab_container_`, and to paint them in the
  // right order, we have to paint their containers in the same order.
  // N.B. if either are nullopt, it doesn't matter what order we paint in
  // because that whole container must be empty and therefore won't paint
  // anything at all.
  std::optional<ZOrderableTabContainerElement> trailing_pinned_element =
      pinned_tab_container_->GetTrailingElementForZOrdering();
  std::optional<ZOrderableTabContainerElement> leading_unpinned_element =
      unpinned_tab_container_->GetLeadingElementForZOrdering();
  if (trailing_pinned_element < leading_unpinned_element) {
    pinned_tab_container_->Paint(paint_info);
    unpinned_tab_container_->Paint(paint_info);
  } else {
    unpinned_tab_container_->Paint(paint_info);
    pinned_tab_container_->Paint(paint_info);
  }

  // Then paint all tabs animating between pinned and unpinned, ordered based on
  // their individual z-values.
  std::vector<ZOrderableTabContainerElement> orderable_children;
  for (views::View* const child : children()) {
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

void CompoundTabContainer::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::optional<BrowserRootView::DropIndex> CompoundTabContainer::GetDropIndex(
    const ui::DropTargetEvent& event) {
  TabContainer* sub_drop_target = GetTabContainerForDrop(event.location());
  CHECK(sub_drop_target);
  CHECK(sub_drop_target->GetDropTarget(
      ConvertPointToTarget(this, sub_drop_target, event.location())));

  // Convert to `sub_drop_target`'s local coordinate space.
  const gfx::Point loc_in_sub_target = ConvertPointToTarget(
      this, sub_drop_target->GetViewForDrop(), event.location());
  const ui::DropTargetEvent adjusted_event = ui::DropTargetEvent(
      event.data(), gfx::PointF(loc_in_sub_target),
      gfx::PointF(loc_in_sub_target), event.source_operations());

  if (sub_drop_target == base::to_address(pinned_tab_container_)) {
    // Pinned tab container shares an index and coordinate space, so no
    // adjustments needed.
    return sub_drop_target->GetDropIndex(adjusted_event);
  } else {
    // For the unpinned container, we need to transform the output to the
    // correct index space.
    const std::optional<BrowserRootView::DropIndex> sub_target_index =
        sub_drop_target->GetDropIndex(adjusted_event);
    return BrowserRootView::DropIndex{
        .index = sub_target_index->index + NumPinnedTabs(),
        .relative_to_index = sub_target_index->relative_to_index,
        .group_inclusion = sub_target_index->group_inclusion};
  }
}

BrowserRootView::DropTarget* CompoundTabContainer::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  TabContainer* const sub_drop_target =
      GetTabContainerForDrop(loc_in_local_coords);

  if (sub_drop_target == nullptr ||
      !sub_drop_target->GetDropTarget(
          ConvertPointToTarget(this, sub_drop_target, loc_in_local_coords))) {
    return nullptr;
  }
  return this;
}

views::View* CompoundTabContainer::GetViewForDrop() {
  return this;
}

void CompoundTabContainer::HandleDragUpdate(
    const std::optional<BrowserRootView::DropIndex>& index) {
  // Update `current_text_drop_target_`.
  TabContainer* next_drop_target = nullptr;
  if (index.has_value()) {
    next_drop_target = base::to_address(index->index < NumPinnedTabs()
                                            ? pinned_tab_container_
                                            : unpinned_tab_container_);
  }
  if (next_drop_target != current_text_drop_target_) {
    if (current_text_drop_target_) {
      current_text_drop_target_->HandleDragExited();
    }
    current_text_drop_target_ = next_drop_target;
  }

  if (current_text_drop_target_ == nullptr) {  // I.e. if `index` is nullopt.
    return;
  }

  // Forward to `current_text_drop_target_`, adjusting if needed.
  if (current_text_drop_target_ == base::to_address(pinned_tab_container_)) {
    pinned_tab_container_->HandleDragUpdate(index);
  } else {
    BrowserRootView::DropIndex adjusted_index{
        .index = index->index - NumPinnedTabs(),
        .relative_to_index = index->relative_to_index,
        .group_inclusion = index->group_inclusion};
    unpinned_tab_container_->HandleDragUpdate(adjusted_index);
  }
}

void CompoundTabContainer::HandleDragExited() {
  if (current_text_drop_target_) {
    current_text_drop_target_->HandleDragExited();
    current_text_drop_target_ = nullptr;
  }
}

views::View* CompoundTabContainer::TargetForRect(views::View* root,
                                                 const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect)) {
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }

  const gfx::Point point(rect.CenterPoint());
  TabContainer* const sub_container = GetTabContainerAt(point);
  if (sub_container == nullptr) {
    return this;
  }

  return sub_container->GetEventHandlerForRect(ToEnclosingRect(
      ConvertRectToTarget(this, sub_container, gfx::RectF(rect))));
}

void CompoundTabContainer::UpdateAnimationTarget(TabSlotView* tab_slot_view,
                                                 gfx::Rect target_bounds,
                                                 TabPinned pinned) {
  if (pinned == TabPinned::kUnpinned) {
    target_bounds = ConvertUnpinnedContainerIdealBoundsToLocal(target_bounds);
  }

  if (tab_slot_view->parent() != this) {
    controller_->UpdateAnimationTarget(tab_slot_view, target_bounds);
    return;
  }

  // We should only have tabs to deal with here, as groups cannot be pinned.
  DCHECK(views::IsViewClass<Tab>(tab_slot_view));
  if (bounds_animator_.IsAnimating(tab_slot_view)) {
    bounds_animator_.SetTargetBounds(tab_slot_view, target_bounds);
  }
}

int CompoundTabContainer::NumPinnedTabs() const {
  return pinned_tab_container_->GetTabCount();
}

bool CompoundTabContainer::IsValidViewModelIndex(int index) const {
  const int total_num_tabs = pinned_tab_container_->GetTabCount() +
                             unpinned_tab_container_->GetTabCount();
  return index >= 0 && index < total_num_tabs;
}

void CompoundTabContainer::TransferTabBetweenContainers(int from_model_index,
                                                        int to_model_index) {
  // If the tab at `from_model_index` is already being transferred, complete
  // all pending transfers before we embark upon this one to avoid conflicts.
  if (bounds_animator_.IsAnimating(GetTabAtModelIndex(from_model_index))) {
    // We are out of sync with the model right now (because we're handling a
    // model update), so we need to be careful here. We can complete our
    // directly managed animations, but we can't ask the sub-containers to do
    // the same, as their ideal bounds calculations assume the model and
    // viewmodel are in sync.
    bounds_animator_.Complete();
  }

  const bool prev_pinned = from_model_index < NumPinnedTabs();
  const bool next_pinned = !prev_pinned;

  const int before_num_pinned_tabs = NumPinnedTabs();
  const int after_num_pinned_tabs =
      before_num_pinned_tabs + (next_pinned ? 1 : -1);

  if (next_pinned) {
    // We are going from `unpinned_tab_container_` to `pinned_tab_container_`.
    // Indices must be valid for those containers. If `from_model_index` ==
    // `to_model_index`, we're pinning the first unpinned tab.
    CHECK_GE(from_model_index, before_num_pinned_tabs);
    CHECK_LT(to_model_index, after_num_pinned_tabs);
  } else {
    // We are going from `pinned_tab_container_` to `unpinned_tab_container_`.
    // Indices must be valid for those containers. If `from_model_index` ==
    // `to_model_index`, we're unpinning the last pinned tab.
    CHECK_LT(from_model_index, before_num_pinned_tabs);
    CHECK_GE(to_model_index, after_num_pinned_tabs);
  }

  TabContainer& from_container =
      *(prev_pinned ? pinned_tab_container_ : unpinned_tab_container_);
  const int from_container_index =
      prev_pinned ? from_model_index
                  : from_model_index - before_num_pinned_tabs;
  TabContainer& to_container =
      *(next_pinned ? pinned_tab_container_ : unpinned_tab_container_);
  const int to_container_index =
      next_pinned ? to_model_index : to_model_index - after_num_pinned_tabs;

  // Take `tab` ourselves, so we can animate it. Save and restore its bounds to
  // ensure it doesn't move visually from its current starting bounds.
  const gfx::RectF initial_tab_bounds = ConvertRectToTarget(
      &from_container, this,
      gfx::RectF(
          from_container.GetTabAtModelIndex(from_container_index)->bounds()));
  Tab* const tab =
      AddChildView(from_container.RemoveTabFromViewModel(from_container_index));
  tab->SetBoundsRect(ToEnclosingRect(initial_tab_bounds));

  // Let `to_container` update its layout data structures.
  to_container.AddTabToViewModel(
      tab, to_container_index,
      next_pinned ? TabPinned::kPinned : TabPinned::kUnpinned);

  AnimateToIdealBounds();
}

void CompoundTabContainer::AnimateTabTo(Tab* tab, gfx::Rect ideal_bounds) {
  if (bounds_animator_.IsAnimating(tab)) {
    bounds_animator_.SetTargetBounds(tab, ideal_bounds);
  } else {
    bounds_animator_.AnimateViewTo(tab, ideal_bounds,
                                   std::make_unique<PinUnpinAnimationDelegate>(
                                       &GetTabContainerFor(tab), tab));
  }
}

gfx::Rect CompoundTabContainer::ConvertUnpinnedContainerIdealBoundsToLocal(
    gfx::Rect ideal_bounds) const {
  const int unpinned_container_ideal_leading_x =
      GetUnpinnedContainerIdealLeadingX();
  ideal_bounds.Offset(unpinned_container_ideal_leading_x, 0);
  return ideal_bounds;
}

TabContainer& CompoundTabContainer::GetTabContainerFor(
    TabSlotView* view) const {
  if (view->GetTabSlotViewType() == TabSlotView::ViewType::kTabGroupHeader) {
    return unpinned_tab_container_.get();
  }

  Tab* tab = views::AsViewClass<Tab>(view);
  return tab->data().pinned ? pinned_tab_container_.get()
                            : unpinned_tab_container_.get();
}

TabContainer* CompoundTabContainer::GetTabContainerForDrop(
    gfx::Point point_in_local_coords) const {
  const int cutoff_x = (pinned_tab_container_->bounds().right() +
                        unpinned_tab_container_->bounds().x()) /
                       2;

  if (point_in_local_coords.x() < cutoff_x) {
    return base::to_address(pinned_tab_container_);
  }
  return base::to_address(unpinned_tab_container_);
}

TabContainer* CompoundTabContainer::GetTabContainerAt(
    gfx::Point point_in_local_coords) const {
  const bool in_pinned =
      pinned_tab_container_->bounds().Contains(point_in_local_coords);
  const bool in_unpinned =
      unpinned_tab_container_->bounds().Contains(point_in_local_coords);

  if (in_pinned && in_unpinned) {
    const int cutoff_x = (pinned_tab_container_->bounds().right() +
                          unpinned_tab_container_->bounds().x()) /
                         2;
    if (point_in_local_coords.x() < cutoff_x) {
      return base::to_address(pinned_tab_container_);
    }
    return base::to_address(unpinned_tab_container_);
  }

  if (in_pinned) {
    return base::to_address(pinned_tab_container_);
  }
  if (in_unpinned) {
    return base::to_address(unpinned_tab_container_);
  }

  // `point_in_local_coords` might be in neither sub container if our layout is
  // (transiently) stale, e.g. during window creation.
  return nullptr;
}

int CompoundTabContainer::GetUnpinnedContainerIdealLeadingX() const {
  return NumPinnedTabs() > 0
             ? pinned_tab_container_->GetIdealBounds(NumPinnedTabs() - 1)
                       .right() -
                   TabStyle::Get()->GetTabOverlap()
             : 0;
}

int CompoundTabContainer::GetAvailableWidthForUnpinnedTabContainer() const {
  // The unpinned container gets the width the pinned container doesn't want.
  return GetAvailableWidthForTabContainer() -
         GetUnpinnedContainerIdealLeadingX();
}

gfx::Size CompoundTabContainer::GetCombinedSizeForTabContainerSizes(
    const gfx::Size pinned_size,
    const gfx::Size unpinned_size) const {
  gfx::Size largest_container = pinned_size;
  largest_container.SetToMax(unpinned_size);

  const int width_with_overlap = pinned_size.width() + unpinned_size.width() -
                                 TabStyle::Get()->GetTabOverlap();
  return gfx::Size(std::max(width_with_overlap, largest_container.width()),
                   largest_container.height());
}

std::optional<gfx::Rect> CompoundTabContainer::GetVisibleContentRect() const {
  const views::ScrollView* const scroll_container =
      views::ScrollView::GetScrollViewForContents(scroll_contents_view_);
  if (!scroll_container) {
    return std::nullopt;
  }

  return scroll_container->GetVisibleRect();
}

void CompoundTabContainer::AnimateScrollToShowXCoordinate(
    const int start_edge,
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

BEGIN_METADATA(CompoundTabContainer)
END_METADATA
