// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/compound_tab_container.h"
#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_container_impl.h"
#include "tab_strip_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace {
class PinnedTabContainerController final : public TabContainerController {
 public:
  explicit PinnedTabContainerController(
      raw_ref<TabContainerController> base_controller)
      : base_controller_(base_controller) {}

  ~PinnedTabContainerController() override = default;

  bool IsValidModelIndex(int index) const override {
    return base_controller_->IsValidModelIndex(index) &&
           index < NumPinnedTabsInModel();
  }

  int GetActiveIndex() const override {
    const int active_index = base_controller_->GetActiveIndex();
    if (!IsValidModelIndex(active_index))
      return TabStripModel::kNoTab;
    return active_index;
  }

  int NumPinnedTabsInModel() const override {
    return base_controller_->NumPinnedTabsInModel();
  }

  void OnDropIndexUpdate(int index, bool drop_before) override {
    base_controller_->OnDropIndexUpdate(index, drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    NOTREACHED();
    return false;
  }

  absl::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override {
    NOTREACHED();
    return absl::nullopt;
  }

  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override {
    NOTREACHED();
    return gfx::Range();
  }

  bool CanExtendDragHandle() const override {
    return base_controller_->CanExtendDragHandle();
  }

 private:
  const raw_ref<TabContainerController> base_controller_;
};

class UnpinnedTabContainerController final : public TabContainerController {
 public:
  explicit UnpinnedTabContainerController(
      raw_ref<TabContainerController> base_controller)
      : base_controller_(base_controller) {}

  ~UnpinnedTabContainerController() override = default;

  bool IsValidModelIndex(int index) const override {
    return index >= 0 &&
           base_controller_->IsValidModelIndex(ContainerToModelIndex(index));
  }

  int GetActiveIndex() const override {
    const absl::optional<int> active_index =
        ModelToContainerIndex(base_controller_->GetActiveIndex());
    // TODO(crbug.com/1346023): Maybe optional instead.
    return active_index.value_or(TabStripModel::kNoTab);
  }

  int NumPinnedTabsInModel() const override { return 0; }

  void OnDropIndexUpdate(int index, bool drop_before) override {
    base_controller_->OnDropIndexUpdate(ContainerToModelIndex(index),
                                        drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    return base_controller_->IsGroupCollapsed(group);
  }

  absl::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override {
    const absl::optional<int> model_index =
        base_controller_->GetFirstTabInGroup(group);
    if (!model_index)
      return absl::nullopt;
    return ModelToContainerIndex(model_index.value());
  }

  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override {
    const gfx::Range model_range = base_controller_->ListTabsInGroup(group);
    return gfx::Range(ModelToContainerIndex(model_range.start()).value(),
                      ModelToContainerIndex(model_range.end()).value());
  }

  bool CanExtendDragHandle() const override {
    return base_controller_->CanExtendDragHandle();
  }

 private:
  absl::optional<int> ModelToContainerIndex(int model_index) const {
    if (model_index < base_controller_->NumPinnedTabsInModel() ||
        !base_controller_->IsValidModelIndex(model_index))
      return absl::nullopt;
    return model_index - base_controller_->NumPinnedTabsInModel();
  }

  int ContainerToModelIndex(int container_index) const {
    if (container_index < 0)
      return TabStripModel::kNoTab;
    const int model_index =
        container_index + base_controller_->NumPinnedTabsInModel();
    if (!base_controller_->IsValidModelIndex(model_index))
      return TabStripModel::kNoTab;
    return model_index;
  }

  const raw_ref<TabContainerController> base_controller_;
};
}  // namespace

CompoundTabContainer::CompoundTabContainer(
    const raw_ref<TabContainerController> controller,
    TabHoverCardController* hover_card_controller,
    TabDragContextBase* drag_context,
    TabSlotController& tab_slot_controller,
    views::View* scroll_contents_view)
    : controller_(controller),
      pinned_tab_container_controller_(
          std::make_unique<PinnedTabContainerController>(controller)),
      pinned_tab_container_(*AddChildView(std::make_unique<TabContainerImpl>(
          *(pinned_tab_container_controller_.get()),
          hover_card_controller,
          drag_context,
          tab_slot_controller,
          scroll_contents_view))),
      unpinned_tab_container_controller_(
          std::make_unique<UnpinnedTabContainerController>(controller)),
      unpinned_tab_container_(*AddChildView(std::make_unique<TabContainerImpl>(
          *(unpinned_tab_container_controller_.get()),
          hover_card_controller,
          drag_context,
          tab_slot_controller,
          scroll_contents_view))) {
  const views::FlexSpecification tab_container_flex_spec =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred);
  pinned_tab_container_->SetProperty(views::kFlexBehaviorKey,
                                     tab_container_flex_spec);
  unpinned_tab_container_->SetProperty(views::kFlexBehaviorKey,
                                       tab_container_flex_spec);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
}

CompoundTabContainer::~CompoundTabContainer() = default;

void CompoundTabContainer::SetAvailableWidthCallback(
    base::RepeatingCallback<int()> available_width_callback) {
  // The pinned container lays out independently of its available width because
  // it doesn't have variable-width tabs. It doesn't matter what we give it here
  // - it will call its callback but ultimately end up effectively ignoring the
  // result deep in TabStripLayoutHelper (because all of its tabs are pinned).
  pinned_tab_container_->SetAvailableWidthCallback(
      base::BindRepeating([]() { return 0; }));
  unpinned_tab_container_->SetAvailableWidthCallback(base::BindRepeating(
      &CompoundTabContainer::GetAvailableWidthForUnpinnedTabContainer,
      base::Unretained(this), available_width_callback));
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
    CHECK(to_model_index < NumPinnedTabs());
    pinned_tab_container_->MoveTab(from_model_index, to_model_index);
  } else {  // !prev_pinned
    CHECK(to_model_index >= NumPinnedTabs());
    unpinned_tab_container_->MoveTab(from_model_index - NumPinnedTabs(),
                                     to_model_index - NumPinnedTabs());
  }
}

void CompoundTabContainer::RemoveTab(int index, bool was_active) {
  CHECK(controller_->IsValidModelIndex(index));
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
  TransferTabBetweenContainers(model_index, model_index);
}

void CompoundTabContainer::SetActiveTab(
    absl::optional<size_t> prev_active_index,
    absl::optional<size_t> new_active_index) {
  absl::optional<size_t> prev_pinned_active_index;
  absl::optional<size_t> new_pinned_active_index;
  absl::optional<size_t> prev_unpinned_active_index;
  absl::optional<size_t> new_unpinned_active_index;
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

std::unique_ptr<Tab> CompoundTabContainer::TransferTabOut(int model_index) {
  NOTREACHED();
  return nullptr;
}

void CompoundTabContainer::StoppedDraggingView(TabSlotView* view) {
  NOTREACHED();
}

void CompoundTabContainer::ScrollTabToVisible(int model_index) {
  // TODO(crbug.com/1346023): Implement. I guess.
}

void CompoundTabContainer::ScrollTabContainerByOffset(int offset) {
  // TODO(crbug.com/1346023): ditto
}

void CompoundTabContainer::OnGroupCreated(const tab_groups::TabGroupId& group) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::OnGroupEditorOpened(
    const tab_groups::TabGroupId& group) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::OnGroupMoved(const tab_groups::TabGroupId& group) {
  NOTREACHED();
}

void CompoundTabContainer::OnGroupContentsChanged(
    const tab_groups::TabGroupId& group) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::OnGroupClosed(const tab_groups::TabGroupId& group) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::UpdateTabGroupVisuals(
    tab_groups::TabGroupId group_id) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::NotifyTabGroupEditorBubbleOpened() {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::NotifyTabGroupEditorBubbleClosed() {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

int CompoundTabContainer::GetModelIndexOf(const TabSlotView* slot_view) const {
  int pinned_index = pinned_tab_container_->GetModelIndexOf(slot_view);
  if (pinned_index != TabStripModel::kNoTab)  // TODO(crbug.com/1346023): Maybe
                                              // optional instead.
    return pinned_index;
  return unpinned_tab_container_->GetModelIndexOf(slot_view) + NumPinnedTabs();
}

Tab* CompoundTabContainer::GetTabAtModelIndex(int index) const {
  CHECK(controller_->IsValidModelIndex(index));
  if (index < NumPinnedTabs())
    return pinned_tab_container_->GetTabAtModelIndex(index);
  return unpinned_tab_container_->GetTabAtModelIndex(index - NumPinnedTabs());
}

int CompoundTabContainer::GetTabCount() const {
  return pinned_tab_container_->GetTabCount() +
         unpinned_tab_container_->GetTabCount();
}

int CompoundTabContainer::GetModelIndexOfFirstNonClosingTab(Tab* tab) const {
  NOTREACHED();
  // TODO(crbug.com/1346023): Impl
  // If the tab is pinned, ask pinned container
  // If that returns kNoTab, ask unpinned container for the first nonclosing tab
  // Otherwise, just forward to unpinned container
  return TabStripModel::kNoTab;
}

void CompoundTabContainer::UpdateHoverCard(
    Tab* tab,
    TabSlotController::HoverCardUpdateType update_type) {
  // TODO(crbug.com/1346023): probably hover card controller ownership is wrong
}

void CompoundTabContainer::HandleLongTap(ui::GestureEvent* event) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

bool CompoundTabContainer::IsRectInWindowCaption(const gfx::Rect& rect) {
  gfx::RectF rect_in_child_coords(rect);
  ConvertRectToTarget(this, base::to_address(pinned_tab_container_),
                      &rect_in_child_coords);
  if (!pinned_tab_container_->IsRectInWindowCaption(
          ToEnclosingRect(rect_in_child_coords))) {
    return false;
  }

  ConvertRectToTarget(base::to_address(pinned_tab_container_),
                      base::to_address(unpinned_tab_container_),
                      &rect_in_child_coords);
  return unpinned_tab_container_->IsRectInWindowCaption(
      ToEnclosingRect(rect_in_child_coords));
}

void CompoundTabContainer::OnTabSlotAnimationProgressed(TabSlotView* view) {
  NOTREACHED();
}

void CompoundTabContainer::OnTabCloseAnimationCompleted(Tab* tab) {
  NOTREACHED();
}

void CompoundTabContainer::StartBasicAnimation() {
  // TODO(crbug.com/1346023): Implement this, or better, remove callers.
}

void CompoundTabContainer::InvalidateIdealBounds() {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

bool CompoundTabContainer::IsAnimating() const {
  return pinned_tab_container_->IsAnimating() ||
         unpinned_tab_container_->IsAnimating();
}

void CompoundTabContainer::CancelAnimation() {
  // TODO(crbug.com/1346023): Impl
}

void CompoundTabContainer::CompleteAnimationAndLayout() {
  // TODO(crbug.com/1346023): Impl
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
    absl::optional<int> override_width,
    CloseTabSource source) {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}
void CompoundTabContainer::ExitTabClosingMode() {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
}

void CompoundTabContainer::SetTabSlotVisibility() {
  // TODO(crbug.com/1346023): Impl
}

bool CompoundTabContainer::InTabClose() {
  // Only unpinned TabContainer can be in tab closing mode.
  return unpinned_tab_container_->InTabClose();
}

std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroupViews>>&
CompoundTabContainer::GetGroupViews() {
  // Only the unpinned container can have groups.
  return unpinned_tab_container_->GetGroupViews();
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
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
  return gfx::Rect();
}

gfx::Rect CompoundTabContainer::GetIdealBounds(
    tab_groups::TabGroupId group) const {
  // TODO(crbug.com/1346023): Impl
  NOTREACHED();
  return gfx::Rect();
}

void CompoundTabContainer::Layout() {
  // TODO(crub.com/1346023): Probably need something special in here for drag at
  // end of tabstrip scenarios.
  views::View::Layout();
}

void CompoundTabContainer::PaintChildren(const views::PaintInfo& paint_info) {
  // TODO(crbug.com/1346023): paint in the right order depending on tab paint
  // order.
  views::View::PaintChildren(paint_info);
}

gfx::Size CompoundTabContainer::GetMinimumSize() const {
  // TODO(crub.com/1346023): Probably need something special in here for drag at
  // end of tabstrip scenarios.
  return views::View::GetMinimumSize();
}

gfx::Size CompoundTabContainer::CalculatePreferredSize() const {
  // TODO(crub.com/1346023): Probably need something special in here for drag at
  // end of tabstrip scenarios.
  return views::View::CalculatePreferredSize();
}

BrowserRootView::DropIndex CompoundTabContainer::GetDropIndex(
    const ui::DropTargetEvent& event) {
  NOTREACHED();
  return BrowserRootView::DropIndex();
}

BrowserRootView::DropTarget* CompoundTabContainer::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  return GetTabContainerAt(loc_in_local_coords);
}

views::View* CompoundTabContainer::GetViewForDrop() {
  NOTREACHED();
  return nullptr;
}

void CompoundTabContainer::HandleDragUpdate(
    const absl::optional<BrowserRootView::DropIndex>& index) {
  NOTREACHED();
}

void CompoundTabContainer::HandleDragExited() {
  NOTREACHED();
}

int CompoundTabContainer::NumPinnedTabs() const {
  return pinned_tab_container_->GetTabCount();
}

void CompoundTabContainer::TransferTabBetweenContainers(int from_model_index,
                                                        int to_model_index) {
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

    std::unique_ptr<Tab> tab = unpinned_tab_container_->TransferTabOut(
        from_model_index - before_num_pinned_tabs);
    pinned_tab_container_->AddTab(std::move(tab), to_model_index,
                                  TabPinned::kPinned);
  } else {
    // We are going from `pinned_tab_container_` to `unpinned_tab_container_`.
    // Indices must be valid for those containers. If `from_model_index` ==
    // `to_model_index`, we're unpinning the last pinned tab.
    CHECK_LT(from_model_index, before_num_pinned_tabs);
    CHECK_GE(to_model_index, after_num_pinned_tabs);

    std::unique_ptr<Tab> tab =
        pinned_tab_container_->TransferTabOut(from_model_index);
    unpinned_tab_container_->AddTab(std::move(tab),
                                    to_model_index - after_num_pinned_tabs,
                                    TabPinned::kUnpinned);
  }

  // TODO(crbug.com/1346023): Remove this jank-reducing band-aid when handoff is
  // properly animated.
  Layout();
}

TabContainer* CompoundTabContainer::GetTabContainerAt(
    gfx::Point point_in_local_coords) {
  if (pinned_tab_container_->bounds().Contains(point_in_local_coords))
    return base::to_address(pinned_tab_container_);
  if (unpinned_tab_container_->bounds().Contains(point_in_local_coords))
    return base::to_address(unpinned_tab_container_);
  NOTREACHED() << "point_in_local_coords " << point_in_local_coords.ToString()
               << " is not in pinned at: "
               << pinned_tab_container_->bounds().ToString()
               << " or unpinned at: "
               << unpinned_tab_container_->bounds().ToString();
  return nullptr;
}

int CompoundTabContainer::GetAvailableWidthForUnpinnedTabContainer(
    base::RepeatingCallback<int()> available_width_callback) {
  // The unpinned container gets the width the pinned container doesn't want.
  // TODO(crbug.com/1346023): Pinned container's preferred width might be a)
  // not correct during animations and b) expensive to call, maybe triggering
  // TabStrip relayout sometimes. Could be the cause of the lag spike issue.
  return available_width_callback.Run() -
         pinned_tab_container_->CalculatePreferredSize().width();
}

BEGIN_METADATA(CompoundTabContainer, views::View)
END_METADATA
