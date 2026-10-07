// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_closing_helper.h"

#include <algorithm>
#include <ranges>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr auto kTouchResizeLayoutTime = base::Seconds(2);
// Expand the watched region downwards below the bottom of the tabstrip so users
// do not accidentally exit closing mode if they drift vertically out of the
// strip.
constexpr int kMouseWatcherVerticalSlop = 40;
// Expand the watched region to the trailing side to cover the new tab button.
constexpr int kMouseWatcherHorizontalSlop = 60;

std::optional<int> FindInactiveTabWidth(
    const TabCollectionNode& parent,
    const TabCollectionNode* closing_tab_node) {
  for (const auto& child : parent.children()) {
    if (child.get() == closing_tab_node || !child->view()->GetVisible()) {
      continue;
    }
    if (child->type() == TabCollectionNode::Type::TAB) {
      TabView* other_tab_view = views::AsViewClass<TabView>(child->view());
      if (!other_tab_view->IsActive() && other_tab_view->width() > 0) {
        return other_tab_view->width();
      }
    } else if (child->type() == TabCollectionNode::Type::SPLIT ||
               child->type() == TabCollectionNode::Type::GROUP) {
      if (auto width = FindInactiveTabWidth(*child, closing_tab_node)) {
        return width;
      }
    }
  }
  return std::nullopt;
}

const TabCollectionNode* GetLastVisibleTabInNode(
    const TabCollectionNode& parent) {
  for (const auto& child : std::views::reverse(parent.children())) {
    if (!child->view()->GetVisible()) {
      continue;
    }
    if (child->type() == TabCollectionNode::Type::TAB) {
      return child.get();
    } else if (child->type() == TabCollectionNode::Type::SPLIT ||
               child->type() == TabCollectionNode::Type::GROUP) {
      if (const auto* tab = GetLastVisibleTabInNode(*child)) {
        return tab;
      }
    }
  }
  return nullptr;
}

int GetVisibleTabCountInNode(const TabCollectionNode& parent) {
  int count = 0;
  for (const auto& child : parent.children()) {
    if (!child->view()->GetVisible()) {
      continue;
    }
    if (child->type() == TabCollectionNode::Type::TAB) {
      ++count;
    } else if (child->type() == TabCollectionNode::Type::SPLIT ||
               child->type() == TabCollectionNode::Type::GROUP) {
      count += GetVisibleTabCountInNode(*child);
    }
  }
  return count;
}
}  // namespace

HorizontalTabClosingHelper::HorizontalTabClosingHelper(
    RootTabCollectionNode& root_node)
    : root_node_(root_node) {
  on_child_will_be_removed_subscription_ =
      root_node_->RegisterOnChildWillBeRemovedCallback(
          base::BindRepeating(&HorizontalTabClosingHelper::OnChildWillBeRemoved,
                              base::Unretained(this)));
  on_children_removed_subscription_ =
      root_node_->RegisterOnChildRemovedCallback(
          base::BindRepeating(&HorizontalTabClosingHelper::OnChildrenRemoved,
                              base::Unretained(this)));
  on_children_added_subscription_ = root_node_->RegisterOnChildrenAddedCallback(
      base::BindRepeating(&HorizontalTabClosingHelper::OnChildrenAdded,
                          base::Unretained(this)));
  on_child_moved_subscription_ =
      root_node_->RegisterOnChildMovedCallback(base::BindRepeating(
          &HorizontalTabClosingHelper::OnChildMoved, base::Unretained(this)));
}

HorizontalTabClosingHelper::~HorizontalTabClosingHelper() = default;

void HorizontalTabClosingHelper::MaybeEnterTabClosingMode(
    std::optional<int> override_width,
    CloseTabSource source) {
  if (source == CloseTabSource::kFromNonUIEvent) {
    return;
  }

  // If there is only 1 visible tab (or none), closing it will leave 0 tabs, so
  // tab closing mode is not meaningful.
  if (GetVisibleTabCount() <= 1) {
    return;
  }

  // If tabs are already at their unconstrained preferred size, they cannot
  // expand any further when a tab is closed or a group is collapsed. So we
  // don't need to enter tab closing mode.
  if (GetUnpinnedContainerWidth() >=
      GetUnpinnedContainerTotalPreferredWidth()) {
    return;
  }

  in_tab_close_ = true;

  if (override_width.has_value()) {
    override_available_width_for_tabs_ = std::max(0, override_width.value());
  }

  touch_resize_timer_.Stop();
  if (source == CloseTabSource::kFromTouch) {
    touch_resize_timer_.Start(
        FROM_HERE, kTouchResizeLayoutTime,
        base::BindOnce(&HorizontalTabClosingHelper::OnTouchTimerFired,
                       base::Unretained(this)));
  } else {
    StartMouseWatcher();
  }
}

void HorizontalTabClosingHelper::ExitTabClosingMode() {
  if (!in_tab_close_) {
    return;
  }

  in_tab_close_ = false;
  override_available_width_for_tabs_.reset();
  StopMouseWatcher();
  touch_resize_timer_.Stop();
  InvalidateLayout();
}

void HorizontalTabClosingHelper::SetOverrideAvailableWidth(int override_width) {
  if (!in_tab_close_) {
    return;
  }
  override_available_width_for_tabs_ = std::max(0, override_width);
  InvalidateLayout();
}

void HorizontalTabClosingHelper::PauseMouseWatcher() {
  is_paused_ = true;
  StopMouseWatcher();
}

void HorizontalTabClosingHelper::ResumeMouseWatcher() {
  is_paused_ = false;
  if (in_tab_close_) {
    StartMouseWatcher();
  }
}

void HorizontalTabClosingHelper::MouseMovedOutOfHost() {
  ExitTabClosingMode();
}

void HorizontalTabClosingHelper::StartMouseWatcher() {
  if (is_paused_) {
    return;
  }
  views::View* watched_view = root_node_->view();
  if (!mouse_watcher_ && watched_view && watched_view->GetWidget()) {
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<views::MouseWatcherViewHost>(
            watched_view,
            gfx::Insets::TLBR(
                0, base::i18n::IsRTL() ? kMouseWatcherHorizontalSlop : 0,
                kMouseWatcherVerticalSlop,
                base::i18n::IsRTL() ? 0 : kMouseWatcherHorizontalSlop)),
        this);
    mouse_watcher_->Start(watched_view->GetWidget()->GetNativeWindow());
  }
}

void HorizontalTabClosingHelper::StopMouseWatcher() {
  mouse_watcher_.reset();
}

void HorizontalTabClosingHelper::OnTouchTimerFired() {
  ExitTabClosingMode();
}

void HorizontalTabClosingHelper::OnChildrenAdded(
    const tabs::TabCollectionNodes& handles) {
  ExitTabClosingMode();
}

void HorizontalTabClosingHelper::OnChildMoved(TabCollectionNode* moved_node) {
  ExitTabClosingMode();
}

void HorizontalTabClosingHelper::OnChildWillBeRemoved(
    TabCollectionNode* child_node) {
  if (!in_tab_close_) {
    return;
  }

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const TabCollectionNode* tab_node = child_node;
  int extra_removed_width = 0;
  if (child_node->type() == TabCollectionNode::Type::GROUP ||
      child_node->type() == TabCollectionNode::Type::SPLIT) {
    if (child_node->children().size() != 1 ||
        child_node->children()[0]->type() != TabCollectionNode::Type::TAB) {
      return;
    }
    tab_node = child_node->children()[0].get();
    if (child_node->type() == TabCollectionNode::Type::GROUP) {
      const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
      extra_removed_width =
          std::max(0, child_node->view()->width() - tab_node->view()->width() +
                          (tab_overlap - header_overlap));
    }
  } else if (child_node->type() != TabCollectionNode::Type::TAB) {
    return;
  }

  // Pinned or hidden tabs do not affect the unpinned container's width.
  TabView* tab_view = views::AsViewClass<TabView>(tab_node->view());
  if (tab_view->data().pinned || !tab_view->GetVisible()) {
    // Exit if closing a pinned or not visible tab without an active width lock.
    if (!override_available_width_for_tabs_.has_value()) {
      ExitTabClosingMode();
    }
    return;
  }

  // If we're closing the last visible tab, tab closing mode is no longer
  // meaningful.
  if (GetVisibleTabCount() <= 1) {
    ExitTabClosingMode();
    return;
  }

  // Closing the trailingmost visible tab allows remaining visible tabs to
  // expand, unless the container is currently overflowing.
  if (!IsUnpinnedContainerOverflowing() && IsLastVisibleTab(tab_node)) {
    // Exit if closing the last visible tab when not overflowing without an
    // active width lock.
    if (!override_available_width_for_tabs_.has_value()) {
      ExitTabClosingMode();
    }
    return;
  }

  int size_delta = tab_view->width();

  // When removing an active, non-pinned tab, the next active tab will be
  // given the active width (unless it's pinned). So the width being
  // removed from the container is really the current width of whichever
  // inactive tab will be made active. Iterate forward to find the first
  // unpinned tab that is visible.
  if (tab_view->IsActive()) {
    if (TabCollectionNode* unpinned_node =
            root_node_->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED)) {
      if (auto width = FindInactiveTabWidth(*unpinned_node, tab_node)) {
        size_delta = *width;
      }
    }
  }

  size_delta += extra_removed_width;

  // Reduce the override width by the removed tab's net width (accounting for
  // tab overlap) to keep remaining tabs frozen at their current sizes.
  int current_override =
      override_available_width_for_tabs_.value_or(GetUnpinnedContainerWidth());

  // When only one tab is present (or the container width is less than or equal
  // to the removed tab's width), removing it removes no overlap, leaving 0
  // width (avoiding leaving a residual `tab_overlap` remnant).
  const int new_override =
      current_override <= size_delta
          ? 0
          : std::max(0, current_override - size_delta + tab_overlap);

  SetOverrideAvailableWidth(new_override);
}

void HorizontalTabClosingHelper::OnChildrenRemoved() {
  // Exit if remaining tabs have reached their unconstrained preferred width.
  if (in_tab_close_ && override_available_width_for_tabs_.has_value() &&
      *override_available_width_for_tabs_ >=
          GetUnpinnedContainerTotalPreferredWidth()) {
    ExitTabClosingMode();
  }
}

const TabCollectionNode* HorizontalTabClosingHelper::GetLastVisibleTab() const {
  TabCollectionNode* unpinned_node =
      root_node_->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  return unpinned_node ? GetLastVisibleTabInNode(*unpinned_node) : nullptr;
}

bool HorizontalTabClosingHelper::IsLastVisibleTab(
    const TabCollectionNode* child_node) const {
  return child_node && child_node == GetLastVisibleTab();
}

bool HorizontalTabClosingHelper::IsUnpinnedContainerOverflowing() const {
  if (UnpinnedTabContainerView* unpinned_container = GetUnpinnedContainer()) {
    if (const auto* scroll_view =
            views::ScrollView::GetScrollViewForContents(unpinned_container)) {
      return scroll_view->IsHorizontalContentOverflowing();
    }
  }
  return false;
}

int HorizontalTabClosingHelper::GetVisibleTabCount() const {
  TabCollectionNode* unpinned_node =
      root_node_->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  return unpinned_node ? GetVisibleTabCountInNode(*unpinned_node) : 0;
}

void HorizontalTabClosingHelper::InvalidateLayout() {
  if (UnpinnedTabContainerView* unpinned_container = GetUnpinnedContainer()) {
    unpinned_container->InvalidateLayout();
  }
}

UnpinnedTabContainerView* HorizontalTabClosingHelper::GetUnpinnedContainer()
    const {
  TabCollectionNode* unpinned_node =
      root_node_->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  return unpinned_node ? views::AsViewClass<UnpinnedTabContainerView>(
                             unpinned_node->view())
                       : nullptr;
}

int HorizontalTabClosingHelper::GetUnpinnedContainerWidth() const {
  if (UnpinnedTabContainerView* unpinned_container = GetUnpinnedContainer()) {
    return unpinned_container->width();
  }
  return 0;
}

int HorizontalTabClosingHelper::GetUnpinnedContainerTotalPreferredWidth()
    const {
  if (UnpinnedTabContainerView* unpinned_container = GetUnpinnedContainer()) {
    return unpinned_container->GetUnconstrainedPreferredWidth();
  }
  return 0;
}
