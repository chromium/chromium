// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"

#include <variant>

#include "base/callback_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/safe_invoke/safe_invoke.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

BaseTabStripRegionView::BaseTabStripRegionView(
    BrowserView* browser_view,
    actions::ActionItem* root_action_item,
    TabStripOrientation orientation)
    : orientation_(orientation),
      browser_view_(browser_view),
      root_action_item_(root_action_item),
      tab_strip_model_(browser_view->browser()->GetTabStripModel()) {}

BaseTabStripRegionView::~BaseTabStripRegionView() {
  on_children_added_subscription_.reset();
  on_children_removed_subscription_.reset();
  on_child_moved_subscription_.reset();
  on_active_tab_changed_subscription_.reset();

  if (root_node_) {
    root_node_->SetController(nullptr);
  }
  tab_strip_controller_.reset();

  if (drag_handler_) {
    auto* drag_handler = drag_handler_.get();
    drag_handler_ = nullptr;
    RemoveChildViewT(drag_handler->GetDragContext());
  }

  if (hover_tab_selector_) {
    hover_tab_selector_->CancelTabTransition();
  }

  hover_card_controller_.reset();
  root_node_.reset();
}

void BaseTabStripRegionView::InitializeTabStrip() {
  if (root_node_) {
    return;
  }

  hover_card_controller_ =
      std::make_unique<TabHoverCardController>(this, browser_view_->browser());
  hover_tab_selector_ = std::make_unique<HoverTabSelector>(tab_strip_model_);

  root_node_ = std::make_unique<RootTabCollectionNode>(
      tab_strip_model_,
      base::BindRepeating(&BaseTabStripRegionView::SetTabStripView,
                          base::Unretained(this)),
      base::BindRepeating(&BaseTabStripRegionView::ClearTabStripView,
                          base::Unretained(this)),
      orientation_);

  TabStripModel* tab_strip_model = browser_view_->browser()->GetTabStripModel();
  CHECK(tab_strip_model);
  auto drag_handler = std::make_unique<TabDragHandlerImpl>(
      *tab_strip_model, *root_node_.get(), *this);
  drag_handler_ = drag_handler.get();

  CHECK(!tab_strip_controller_);
  tab_strip_controller_ = std::make_unique<TabStripCollectionController>(
      tab_strip_model, browser_view_, *root_node_.get(),
      *AddChildView(std::move(drag_handler)), hover_card_controller_.get(),
      orientation_);

  root_node_->SetController(tab_strip_controller_.get());

  root_node_->Init();

  new_tab_button_pressed_start_time_ = std::nullopt;
  on_children_added_subscription_ =
      root_node_->RegisterOnChildrenAddedCallback(base::BindRepeating(
          &BaseTabStripRegionView::OnChildrenAdded, base::Unretained(this)));
  on_children_removed_subscription_ =
      root_node_->RegisterOnChildRemovedCallback(base::BindRepeating(
          &BaseTabStripRegionView::OnChildrenRemoved, base::Unretained(this)));
  on_child_moved_subscription_ =
      root_node_->RegisterOnChildMovedCallback(base::BindRepeating(
          &BaseTabStripRegionView::OnChildMoved, base::Unretained(this)));
}

void BaseTabStripRegionView::ResetTabStrip() {
  if (!root_node_) {
    return;
  }

  on_children_added_subscription_.reset();
  on_children_removed_subscription_.reset();
  on_child_moved_subscription_.reset();

  root_node_->Reset();

  root_node_->SetController(nullptr);
  tab_strip_controller_.reset();

  if (drag_handler_) {
    auto* drag_handler = drag_handler_.get();
    drag_handler_ = nullptr;
    RemoveChildViewT(drag_handler->GetDragContext());
  }

  hover_tab_selector_->CancelTabTransition();
  hover_tab_selector_.reset();
  hover_card_controller_.reset();

  root_node_.reset();
}

PinnedTabContainerView* BaseTabStripRegionView::GetPinnedTabsContainer() {
  return tab_strip_view_ ? tab_strip_view_->GetPinnedTabsContainer() : nullptr;
}

UnpinnedTabContainerView* BaseTabStripRegionView::GetUnpinnedTabsContainer() {
  return tab_strip_view_ ? tab_strip_view_->GetUnpinnedTabsContainer()
                         : nullptr;
}

bool BaseTabStripRegionView::IsTabStripEditable() const {
  return tab_strip_editable_for_testing_ &&
         (!drag_handler_ ||
          !drag_handler_->GetDragContext()->GetDragController());
}

bool BaseTabStripRegionView::IsTabStripCloseable() const {
  if (!drag_handler_) {
    return true;
  }
  if (auto* drag_controller =
          drag_handler_->GetDragContext()->GetDragController()) {
    return drag_controller->IsMovingLastTab();
  }
  return true;
}

void BaseTabStripRegionView::UpdateLoadingAnimations(
    const base::TimeDelta& elapsed_time) {
  if (!root_node_) {
    return;
  }
  for (tabs::TabInterface* tab : *tab_strip_model_) {
    SafeInvoke(root_node_->GetNodeForHandle(tab->GetHandle()))
        .Then(&TabCollectionNode::view)
        .Then(Overload<views::View*>(&views::AsViewClass<TabView>))
        .Then(&TabView::StepLoadingAnimation, elapsed_time);
  }
}

std::optional<int> BaseTabStripRegionView::GetFocusedTabIndex() const {
  const views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager || !root_node_) {
    return std::nullopt;
  }

  const views::View* focused_view = focus_manager->GetFocusedView();
  if (!focused_view) {
    return std::nullopt;
  }

  for (int i = 0; i < tab_strip_model_->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(i);
    const TabCollectionNode* node =
        root_node_->GetNodeForHandle(tab->GetHandle());
    if (node && node->view() == focused_view) {
      return i;
    }
  }

  return std::nullopt;
}

const tabs::TabData& BaseTabStripRegionView::GetTabData(
    const tabs::TabHandle& tab) {
  CHECK(root_node_);
  const TabCollectionNode* node = root_node_->GetNodeForHandle(tab);
  CHECK(node);

  TabView* tab_view = views::AsViewClass<TabView>(node->view());
  CHECK(tab_view);

  return tab_view->data();
}

views::View* BaseTabStripRegionView::GetTabAnchorView(
    const tabs::TabHandle& tab) {
  if (!root_node_) {
    return nullptr;
  }
  const TabCollectionNode* node = root_node_->GetNodeForHandle(tab);
  return node ? node->view() : nullptr;
}

views::View* BaseTabStripRegionView::GetTabGroupAnchorView(
    const tab_groups::TabGroupId& group) {
  if (!tab_strip_model_->SupportsTabGroups() || !root_node_) {
    return nullptr;
  }

  if (const TabGroup* tab_group =
          tab_strip_model_->group_model()->GetTabGroup(group)) {
    return root_node_->GetNodeForHandle(tab_group->GetCollectionHandle())
        ->view();
  }

  return nullptr;
}

TabDragContext* BaseTabStripRegionView::GetDragContext() {
  return drag_handler_ ? drag_handler_->GetDragContext() : nullptr;
}

std::optional<BrowserRootView::DropIndex> BaseTabStripRegionView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  // Check pinned tabs.
  PinnedTabContainerView* pinned_container = GetPinnedTabsContainer();
  if (pinned_container && !pinned_container->children().empty()) {
    gfx::Point loc_in_pinned = views::View::ConvertPointToTarget(
        this, pinned_container, event.location());
    if (orientation_ == TabStripOrientation::kHorizontal) {
      if (loc_in_pinned.x() < 0) {
        return pinned_container->GetLinkDropIndex(gfx::Point(0, 0));
      } else if (loc_in_pinned.x() >= 0 &&
                 loc_in_pinned.x() < pinned_container->width()) {
        return pinned_container->GetLinkDropIndex(loc_in_pinned);
      }
    } else {
      // If the point is above the pinned container, return the beginning of the
      // container.
      if (loc_in_pinned.y() < 0) {
        return pinned_container->GetLinkDropIndex(gfx::Point(0, 0));
      } else if (loc_in_pinned.y() >= 0 &&
                 loc_in_pinned.y() < pinned_container->height()) {
        return pinned_container->GetLinkDropIndex(loc_in_pinned);
      }
    }
  }

  // Check unpinned tabs.
  UnpinnedTabContainerView* unpinned_container = GetUnpinnedTabsContainer();
  if (unpinned_container && !unpinned_container->children().empty()) {
    gfx::Point loc_in_unpinned = views::View::ConvertPointToTarget(
        this, unpinned_container, event.location());
    if (orientation_ == TabStripOrientation::kHorizontal) {
      if (loc_in_unpinned.x() < 0) {
        return unpinned_container->GetLinkDropIndex(gfx::Point(0, 0));
      } else if (loc_in_unpinned.x() >= 0 &&
                 loc_in_unpinned.x() < unpinned_container->width()) {
        return unpinned_container->GetLinkDropIndex(loc_in_unpinned);
      }
    } else {
      // If the point is above the unpinned container, return the beginning of
      // the container.
      if (loc_in_unpinned.y() < 0) {
        return unpinned_container->GetLinkDropIndex(gfx::Point(0, 0));
      } else if (loc_in_unpinned.y() >= 0 &&
                 loc_in_unpinned.y() < unpinned_container->height()) {
        return unpinned_container->GetLinkDropIndex(loc_in_unpinned);
      }
    }
  }

  // If it's at the end, return the end of the unpinned container.
  if (unpinned_container) {
    if (orientation_ == TabStripOrientation::kHorizontal) {
      return unpinned_container->GetLinkDropIndex(
          gfx::Point(unpinned_container->width(), 0));
    } else {
      return unpinned_container->GetLinkDropIndex(
          gfx::Point(0, unpinned_container->height()));
    }
  }

  return std::nullopt;
}

BrowserRootView::DropTarget* BaseTabStripRegionView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  if (tab_strip_view_ && IsTabStripEditable() &&
      GetLocalBounds().Contains(loc_in_local_coords)) {
    return this;
  }
  return nullptr;
}

views::View* BaseTabStripRegionView::GetViewForDrop() {
  return this;
}

bool BaseTabStripRegionView::CanDrop(const OSExchangeData& data) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->CanDrop(data);
  }
  return false;
}

bool BaseTabStripRegionView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->GetDropFormats(formats,
                                                           format_types);
  }
  return false;
}

void BaseTabStripRegionView::OnDragEntered(const ui::DropTargetEvent& event) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    drag_handler_->GetDragContext()->OnDragEntered(event);
  }
}

int BaseTabStripRegionView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    return drag_handler_->GetDragContext()->OnDragUpdated(event);
  }
  return 0;
}

void BaseTabStripRegionView::OnDragExited() {
  if (drag_handler_ && drag_handler_->GetDragContext()) {
    drag_handler_->GetDragContext()->OnDragExited();
  }
}

void BaseTabStripRegionView::SetTabStripObserver(TabStripObserver* observer) {
  // Do nothing.
}

views::View* BaseTabStripRegionView::GetTabStripView() {
  return tab_strip_view_;
}

TabHoverCardController* BaseTabStripRegionView::GetHoverCardController() {
  return hover_card_controller_.get();
}

bool BaseTabStripRegionView::TraverseUsingUpDownKeys() {
  return orientation_ == TabStripOrientation::kVertical;
}

std::unique_ptr<ExpandOnHoverLock> BaseTabStripRegionView::GetExpandOnHoverLock(
    ExpandOnHoverLockType lock_type) {
  return nullptr;
}

void BaseTabStripRegionView::HandleDragUpdate(
    const std::optional<BrowserRootView::DropIndex>& index) {
  SetLinkDropArrow(index);
}

void BaseTabStripRegionView::HandleDragExited() {
  SetLinkDropArrow(std::nullopt);
}

void BaseTabStripRegionView::DisableTabStripEditingForTesting() {
  tab_strip_editable_for_testing_ = false;
}

gfx::Rect BaseTabStripRegionView::GetLinkDropBoundsForTesting(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  return GetLinkDropBounds(drop_index, direction);  // IN-TEST
}

TabDragTarget* BaseTabStripRegionView::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  if (!drag_handler_) {
    return nullptr;
  }
  gfx::Rect tab_strip_draggable_bounds = GetTabStripDraggableBounds();
  if (!tab_strip_draggable_bounds.Contains(point_in_screen)) {
    return nullptr;
  }

  // Note: if the drag has not attached to this tab strip yet, it doesn't matter
  // which container is used because the first drag loop iteration just attaches
  // it.
  if (drag_handler_->IsDraggingPinnedTabs()) {
    return &GetPinnedTabsContainer()->GetTabDragTarget(point_in_screen);
  }
  return &GetUnpinnedTabsContainer()->GetTabDragTarget(point_in_screen);
}

views::View* BaseTabStripRegionView::SetTabStripView(
    std::unique_ptr<views::View> view) {
  CHECK(views::IsViewClass<TabStripView>(view.get()));
  tab_strip_view_ = static_cast<TabStripView*>(view.get());

  AddChildView(std::move(view));

  tab_strip_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  on_active_tab_changed_subscription_ =
      root_node_->RegisterOnActiveTabChangedCallback(base::BindRepeating(
          &BaseTabStripRegionView::OnActiveTabChanged, base::Unretained(this)));

  OnTabStripViewSet();
  return tab_strip_view_;
}

void BaseTabStripRegionView::ClearTabStripView(views::View* view) {
  CHECK(tab_strip_view_);
  CHECK(tab_strip_view_ == view);
  on_active_tab_changed_subscription_.reset();
  OnTabStripViewWillClear();
  RemoveChildViewT(std::exchange(tab_strip_view_, nullptr));
}

void BaseTabStripRegionView::RecordNewTabButtonPressed() {
  new_tab_button_pressed_start_time_ = base::TimeTicks::Now();

  base::RecordAction(base::UserMetricsAction("NewTab_Button"));
}

void BaseTabStripRegionView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
}

void BaseTabStripRegionView::RemovedFromWidget() {
  widget_observation_.Reset();
}

void BaseTabStripRegionView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                       bool visible) {
  if (visible && is_first_window_presentation_) {
    is_first_window_presentation_ = false;
    // Only scroll-in the active tab for the first window presentation.
    if (tab_strip_view()) {
      tab_strip_view()->OnTabChanged(
          root_node_->GetController()->GetActiveTab());
    }
  }
}

void BaseTabStripRegionView::OnChildrenAdded(
    const tabs::TabCollectionNodes& handles) {
  if (new_tab_button_pressed_start_time_.has_value()) {
    base::UmaHistogramTimes(
        "TabStrip.TimeToCreateNewTabFromPress",
        base::TimeTicks::Now() - new_tab_button_pressed_start_time_.value());
    new_tab_button_pressed_start_time_.reset();
  }
  hover_tab_selector_->CancelTabTransition();

  const tabs::TabInterface* active_tab = tab_strip_model_->GetActiveTab();
  if (!active_tab) {
    return;
  }

  const tabs::TabInterface* last_new_tab = nullptr;
  for (const auto& handle : handles) {
    if (const auto* tab_handle = std::get_if<tabs::TabHandle>(&handle)) {
      tabs::TabInterface* new_tab = tab_handle->Get();
      if (new_tab && new_tab != active_tab) {
        last_new_tab = new_tab;
      }
    }
  }

  if (last_new_tab && tab_strip_view_ && !is_first_window_presentation_) {
    tab_strip_view_->ScrollToFitTabs(active_tab, last_new_tab);
  }
}

void BaseTabStripRegionView::OnChildrenRemoved() {
  hover_tab_selector_->CancelTabTransition();
}

void BaseTabStripRegionView::OnChildMoved(TabCollectionNode* moved_node) {
  hover_tab_selector_->CancelTabTransition();
  if (tab_strip_view_ && !is_first_window_presentation_) {
    tab_strip_view_->OnChildMoved(moved_node);
  }
}

void BaseTabStripRegionView::OnActiveTabChanged(
    const tabs::TabInterface* active_tab) {
  if (hover_card_controller_) {
    hover_card_controller_->UpdateHoverCard(
        nullptr, TabSlotController::HoverCardUpdateType::kSelectionChanged);
  }
  if (tab_strip_view_ && !is_first_window_presentation_) {
    tab_strip_view_->OnTabChanged(active_tab);
  }
}


void BaseTabStripRegionView::SetLinkDropArrow(
    const std::optional<BrowserRootView::DropIndex>& index) {
  if (!tab_strip_controller_) {
    return;
  }

  if (!index.has_value()) {
    hover_tab_selector_->CancelTabTransition();
    drop_arrow_.reset();
    return;
  }

  if (index->relative_to_index ==
      BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex) {
    hover_tab_selector_->CancelTabTransition();
  } else {
    hover_tab_selector_->StartTabTransition(index->index);
  }

  if (!drop_arrow_) {
    drop_arrow_ = std::make_unique<DropArrow>(
        *index, GetWidget()->GetNativeWindow(),
        base::BindRepeating(&BaseTabStripRegionView::GetLinkDropBounds,
                            base::Unretained(this)));
  } else if (index != drop_arrow_->index()) {
    drop_arrow_->SetIndex(*index);
  }
}

views::View* BaseTabStripRegionView::GetTabViewAt(int tab_index) const {
  if (!root_node_ || !tab_strip_model_ || tab_index < 0 ||
      tab_index >= tab_strip_model_->count()) {
    return nullptr;
  }
  tabs::TabInterface* tab = tab_strip_model_->GetTabAtIndex(tab_index);
  if (!tab) {
    return nullptr;
  }
  TabCollectionNode* node = root_node_->GetNodeForHandle(tab->GetHandle());
  return node ? node->view() : nullptr;
}

bool BaseTabStripRegionView::IsDropBeforeGroupHeader(
    const BrowserRootView::DropIndex& drop_index,
    const tabs::TabInterface* tab) const {
  if (!tab || !tab->GetGroup().has_value()) {
    return false;
  }
  if (drop_index.relative_to_index ==
          BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex &&
      drop_index.group_inclusion ==
          BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup) {
    return tab_strip_model_->group_model()
               ->GetTabGroup(tab->GetGroup().value())
               ->GetFirstTab() == tab;
  }
  return false;
}

views::View* BaseTabStripRegionView::GetGroupHeaderView(
    const tab_groups::TabGroupId& group_id) const {
  if (!root_node_) {
    return nullptr;
  }
  TabCollectionNode* group_node =
      root_node_->GetNodeForHandle(tab_strip_model_->group_model()
                                       ->GetTabGroup(group_id)
                                       ->GetCollectionHandle());
  if (!group_node || !group_node->view()) {
    return nullptr;
  }
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  return group_view ? group_view->group_header() : nullptr;
}

bool BaseTabStripRegionView::IsDragging() const {
  return drag_handler_ && drag_handler_->IsDragging();
}

gfx::Rect BaseTabStripRegionView::GetLinkDropBounds(
    const BrowserRootView::DropIndex& drop_index,
    DropArrow::Direction* direction) {
  CHECK_GE(drop_index.index, 0);

  if (tab_strip_model_->count() == 0) {
    return gfx::Rect();
  }

  gfx::Point arrow_position = GetLinkDropArrowPosition(drop_index, direction);
  gfx::Rect drop_bounds =
      GetLinkDropBoundsFromPosition(arrow_position, *direction);

  // If the rect doesn't fit on the monitor, push the arrow to the other side.
  display::Screen* screen = display::Screen::Get();
  gfx::Rect display_bounds =
      screen->GetDisplayNearestView(GetWidget()->GetNativeView()).bounds();

  DropArrow::MaybeAdjustDisplayBounds(display_bounds);

  if (!display_bounds.Contains(drop_bounds)) {
    if (orientation_ == TabStripOrientation::kHorizontal) {
      const bool is_beneath = drop_bounds.y() < display_bounds.y();
      if (is_beneath) {
        *direction = DropArrow::Direction::kUp;
        drop_bounds.Offset(0, drop_bounds.height() + height());
      }
    } else {
      if (base::i18n::IsRTL() && *direction == DropArrow::Direction::kLeft) {
        *direction = DropArrow::Direction::kRight;
        drop_bounds.Offset(-GetBoundsInScreen().width() - DropArrow::kSize, 0);
      } else if (!base::i18n::IsRTL() &&
                 *direction == DropArrow::Direction::kRight) {
        *direction = DropArrow::Direction::kLeft;
        drop_bounds.Offset(GetBoundsInScreen().width() + DropArrow::kSize, 0);
      }
    }
  }

  return drop_bounds;
}

gfx::Rect BaseTabStripRegionView::GetLinkDropBoundsFromPosition(
    gfx::Point position,
    DropArrow::Direction direction) {
  if (direction == DropArrow::Direction::kRight) {
    position.Offset(-DropArrow::kSize, -DropArrow::kSize / 2);
  } else if (direction == DropArrow::Direction::kLeft) {
    position.Offset(DropArrow::kSize, -DropArrow::kSize / 2);
  } else {
    position.Offset(-DropArrow::kSize / 2, -DropArrow::kSize);
  }

  return gfx::Rect(position, gfx::Size(DropArrow::kSize, DropArrow::kSize));
}

void BaseTabStripRegionView::OnGlassFrameEligibilityChanged(bool is_eligible) {
  SchedulePaint();
  // The parent of the Tab views are layer backed, so we need to explicitly
  // schedule a repaint on them.
  SafeInvoke(GetUnpinnedTabsContainer()).Then(&views::View::SchedulePaint);
  SafeInvoke(GetPinnedTabsContainer()).Then(&views::View::SchedulePaint);
}

BEGIN_METADATA(BaseTabStripRegionView)
END_METADATA
