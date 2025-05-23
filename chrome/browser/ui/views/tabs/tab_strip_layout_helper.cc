// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_layout_state.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "components/tabs/public/split_tab_id.h"
#include "tab_container_controller.h"
#include "ui/gfx/range/range.h"
#include "ui/views/view_model.h"

struct TabStripLayoutHelper::TabSlot {
  static TabStripLayoutHelper::TabSlot CreateForTabSlotView(TabSlotView* view,
                                                            TabPinned pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = view->GetTabSlotViewType();
    slot.view = view;
    slot.state = TabLayoutState(TabOpen::kOpen, pinned, TabActive::kInactive,
                                view->split());
    return slot;
  }

  TabSlotView::ViewType type;
  raw_ptr<TabSlotView, DanglingUntriaged> view;
  TabLayoutState state;
};

TabStripLayoutHelper::TabStripLayoutHelper(
    const TabContainerController& controller,
    GetTabsCallback get_tabs_callback)
    : controller_(controller),
      get_tabs_callback_(get_tabs_callback),
      tab_strip_layout_domain_(LayoutDomain::kInactiveWidthEqualsActiveWidth) {}

TabStripLayoutHelper::~TabStripLayoutHelper() = default;

std::vector<Tab*> TabStripLayoutHelper::GetTabs() const {
  std::vector<Tab*> tabs;
  for (const TabSlot& slot : slots_) {
    if (slot.type == TabSlotView::ViewType::kTab) {
      tabs.push_back(static_cast<Tab*>(slot.view));
    }
  }

  return tabs;
}

std::vector<TabSlotView*> TabStripLayoutHelper::GetTabSlotViews() const {
  std::vector<TabSlotView*> views;
  for (const TabSlot& slot : slots_) {
    views.push_back(slot.view);
  }
  return views;
}

size_t TabStripLayoutHelper::GetPinnedTabCount() const {
  size_t pinned_count = 0;
  for (const TabSlot& slot : slots_) {
    if (slot.state.pinned() == TabPinned::kPinned && !slot.state.IsClosed()) {
      pinned_count++;
    }
  }
  return pinned_count;
}

void TabStripLayoutHelper::InsertTabAt(int model_index,
                                       Tab* tab,
                                       TabPinned pinned) {
  const int slot_index =
      GetSlotInsertionIndexForNewTab(model_index, tab->group());
  slots_.insert(slots_.begin() + slot_index,
                TabSlot::CreateForTabSlotView(tab, pinned));
}

void TabStripLayoutHelper::MarkTabAsClosing(int model_index, Tab* tab) {
  const int slot_index = GetSlotIndexForExistingTab(model_index);
  slots_[slot_index].state.set_open(TabOpen::kClosed);
}

void TabStripLayoutHelper::RemoveTab(Tab* tab) {
  auto it = std::ranges::find_if(slots_, [tab](const TabSlot& slot) {
    return slot.type == TabSlotView::ViewType::kTab && slot.view == tab;
  });
  if (it != slots_.end()) {
    slots_.erase(it);
  }
}

void TabStripLayoutHelper::MoveTab(
    std::optional<tab_groups::TabGroupId> moving_tab_group,
    int prev_index,
    int new_index) {
  const int prev_slot_index = GetSlotIndexForExistingTab(prev_index);
  TabSlot moving_tab = slots_[prev_slot_index];
  slots_.erase(slots_.begin() + prev_slot_index);

  const int new_slot_index =
      GetSlotInsertionIndexForNewTab(new_index, moving_tab_group);
  slots_.insert(slots_.begin() + new_slot_index, moving_tab);

  if (moving_tab_group.has_value()) {
    UpdateGroupHeaderIndex(moving_tab_group.value());
  }
}

void TabStripLayoutHelper::SetTabPinned(int model_index, TabPinned pinned) {
  const int slot_index = GetSlotIndexForExistingTab(model_index);
  slots_[slot_index].state.set_pinned(pinned);
}

void TabStripLayoutHelper::InsertGroupHeader(tab_groups::TabGroupId group,
                                             TabGroupHeader* header) {
  gfx::Range tabs_in_group = controller_->ListTabsInGroup(group);
  const int header_slot_index =
      GetSlotInsertionIndexForNewTab(tabs_in_group.start(), group);
  slots_.insert(slots_.begin() + header_slot_index,
                TabSlot::CreateForTabSlotView(header, TabPinned::kUnpinned));

  // Set the starting location of the header to something reasonable for the
  // animation.
  slots_[header_slot_index].view->SetBoundsRect(
      GetTabs()[tabs_in_group.start()]->bounds());
}

void TabStripLayoutHelper::RemoveGroupHeader(tab_groups::TabGroupId group) {
  const int slot_index = GetSlotIndexForGroupHeader(group);
  slots_.erase(slots_.begin() + slot_index);
}

void TabStripLayoutHelper::UpdateGroupHeaderIndex(
    tab_groups::TabGroupId group) {
  if (!GetFirstTabSlotForGroup(group).has_value()) {
    return;
  }

  const int slot_index = GetSlotIndexForGroupHeader(group);
  TabSlot header_slot = slots_[slot_index];

  slots_.erase(slots_.begin() + slot_index);

  // Recalculate the first tab index after removing the header as it
  // helps with the header insertion index calculation.
  std::optional<int> first_tab = GetFirstTabSlotForGroup(group);

  slots_.insert(slots_.begin() + first_tab.value(), header_slot);
}

void TabStripLayoutHelper::SetActiveTab(
    std::optional<size_t> prev_active_index,
    std::optional<size_t> new_active_index) {
  if (prev_active_index.has_value()) {
    const int prev_slot_index =
        GetSlotIndexForExistingTab(prev_active_index.value());
    slots_[prev_slot_index].state.set_active(TabActive::kInactive);
    // We are assuming that when a tabs active state is changed, it can't be in
    // the middle of a move or something that would make them non-contiguous.
    std::optional<int> adjacent_split = GetAdjacentSplitTab(prev_slot_index);
    if (adjacent_split.has_value()) {
      slots_[adjacent_split.value()].state.set_active(TabActive::kInactive);
    }
  }
  if (new_active_index.has_value()) {
    const int new_slot_index =
        GetSlotIndexForExistingTab(new_active_index.value());
    slots_[new_slot_index].state.set_active(TabActive::kActive);
    std::optional<int> adjacent_split = GetAdjacentSplitTab(new_slot_index);
    if (adjacent_split.has_value()) {
      slots_[adjacent_split.value()].state.set_active(TabActive::kActive);
    }
  }
}

int TabStripLayoutHelper::CalculateMinimumWidth() {
  auto [bounds, layout_domain] = CalculateIdealBounds(0);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::CalculatePreferredWidth() {
  auto [bounds, layout_domain] = CalculateIdealBounds(std::nullopt);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::UpdateIdealBounds(int available_width) {
  auto [bounds, layout_domain] = CalculateIdealBounds(available_width);
  DCHECK_EQ(slots_.size(), bounds.size());
  tab_strip_layout_domain_ = layout_domain;

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  const std::optional<int> active_tab_model_index =
      controller_->GetActiveIndex();
  const std::optional<int> active_tab_slot_index =
      active_tab_model_index.has_value()
          ? std::optional<int>(
                GetSlotIndexForExistingTab(active_tab_model_index.value()))
          : std::nullopt;

  // Store the active split (if applicable) for determining whether other tabs
  // in the split should be active.
  std::optional<split_tabs::SplitTabId> active_split_id = std::nullopt;
  if (active_tab_slot_index.has_value()) {
    const TabSlot& active_slot = slots_[active_tab_slot_index.value()];
    active_split_id = active_slot.view->split();
  }

  int current_tab_model_index = 0;
  for (int i = 0; i < static_cast<int>(bounds.size()); ++i) {
    const TabSlot& slot = slots_[i];
    switch (slot.type) {
      case TabSlotView::ViewType::kTab:
        if (!slot.state.IsClosed()) {
          tabs->set_ideal_bounds(current_tab_model_index, bounds[i]);
          ++current_tab_model_index;
        }
        break;
      case TabSlotView::ViewType::kTabGroupHeader:
        group_header_ideal_bounds_[slot.view->group().value()] = bounds[i];
        break;
    }
  }

  return bounds.back().right();
}

std::pair<std::vector<gfx::Rect>, LayoutDomain>
TabStripLayoutHelper::CalculateIdealBounds(std::optional<int> available_width) {
  const std::optional<int> active_tab_model_index =
      controller_->GetActiveIndex();
  const std::optional<int> active_tab_slot_index =
      active_tab_model_index.has_value()
          ? std::optional<int>(
                GetSlotIndexForExistingTab(active_tab_model_index.value()))
          : std::nullopt;
  const std::optional<int> active_split_tab_slot_index =
      active_tab_slot_index.has_value()
          ? GetAdjacentSplitTab(active_tab_model_index.value())
          : std::nullopt;
  const int pinned_tab_count = GetPinnedTabCount();
  const std::optional<int> last_pinned_tab_slot_index =
      pinned_tab_count > 0
          ? std::optional<int>(GetSlotIndexForExistingTab(pinned_tab_count - 1))
          : std::nullopt;

  std::vector<TabWidthConstraints> tab_widths;
  for (int i = 0; i < static_cast<int>(slots_.size()); i++) {
    auto active =
        (i == active_tab_slot_index || i == active_split_tab_slot_index)
            ? TabActive::kActive
            : TabActive::kInactive;
    auto pinned = last_pinned_tab_slot_index.has_value() &&
                          i <= last_pinned_tab_slot_index
                      ? TabPinned::kPinned
                      : TabPinned::kUnpinned;

    // A collapsed tab animates closed like a closed tab.
    auto open = (slots_[i].state.IsClosed() || SlotIsCollapsedTab(i))
                    ? TabOpen::kClosed
                    : TabOpen::kOpen;
    TabLayoutState state =
        TabLayoutState(open, pinned, active, slots_[i].view->split());
    TabSizeInfo size_info = slots_[i].view->GetTabSizeInfo();

    tab_widths.emplace_back(state, size_info);
  }

  return CalculateTabBounds(tab_widths, available_width);
}

int TabStripLayoutHelper::GetSlotIndexForExistingTab(int model_index) const {
  const int original_slot_index =
      GetFirstSlotIndexForTabModelIndex(model_index);
  CHECK_LT(original_slot_index, static_cast<int>(slots_.size()))
      << "model_index = " << model_index
      << " does not represent an existing tab";

  int slot_index = original_slot_index;

  if (slots_[slot_index].type == TabSlotView::ViewType::kTab) {
    CHECK(!slots_[slot_index].state.IsClosed());
    return slot_index;
  }

  // If `slot_index` is a group header we must return the next slot that
  // is not animating closed.
  if (slots_[slot_index].type == TabSlotView::ViewType::kTabGroupHeader) {
    // Skip all slots animating closed.
    do {
      slot_index += 1;
    } while (slot_index < static_cast<int>(slots_.size()) &&
             slots_[slot_index].state.IsClosed());

    // Double check we arrived at a tab.
    CHECK_LT(slot_index, static_cast<int>(slots_.size()))
        << "group header at " << original_slot_index
        << " not followed by an open tab";
    CHECK_EQ(slots_[slot_index].type, TabSlotView::ViewType::kTab);
  }

  return slot_index;
}

int TabStripLayoutHelper::GetSlotInsertionIndexForNewTab(
    int new_model_index,
    std::optional<tab_groups::TabGroupId> group) const {
  int slot_index = GetFirstSlotIndexForTabModelIndex(new_model_index);

  if (slot_index == static_cast<int>(slots_.size())) {
    return slot_index;
  }

  // If `slot_index` points to a group header and the new tab's `group`
  // matches, the tab goes to the right of the header to keep it
  // contiguous.
  if (slots_[slot_index].type == TabSlotView::ViewType::kTabGroupHeader &&
      static_cast<const TabGroupHeader*>(slots_[slot_index].view)->group() ==
          group) {
    return slot_index + 1;
  }

  return slot_index;
}

std::optional<int> TabStripLayoutHelper::GetFirstTabSlotForGroup(
    tab_groups::TabGroupId group) const {
  for (int slot_index = 0; slot_index < static_cast<int>(slots_.size());
       ++slot_index) {
    if (slots_[slot_index].state.IsClosed()) {
      continue;
    }

    if (slots_[slot_index].type == TabSlotView::ViewType::kTab &&
        slots_[slot_index].view->group().has_value() &&
        slots_[slot_index].view->group().value() == group) {
      return slot_index;
    }
  }

  return std::nullopt;
}

int TabStripLayoutHelper::GetFirstSlotIndexForTabModelIndex(
    int model_index) const {
  int current_model_index = 0;

  // Conceptually we assign a model index to each slot equal to the
  // number of open tabs preceeding it. Group headers will have the same
  // index as the tab before it, and each open tab will have the index
  // of the previous slot plus 1. Closing tabs are not counted, and are
  // skipped altogether.
  //
  // We simply return the first slot that has a matching model index.
  for (int slot_index = 0; slot_index < static_cast<int>(slots_.size());
       ++slot_index) {
    if (slots_[slot_index].state.IsClosed()) {
      continue;
    }

    if (model_index == current_model_index) {
      return slot_index;
    }

    if (slots_[slot_index].type == TabSlotView::ViewType::kTab) {
      current_model_index += 1;
    }
  }

  // If there's no slot in `slots_` corresponding to `model_index`, then
  // `model_index` may represent the first tab past the end of the
  // tabstrip. In this case we should return the first-past-the-end
  // index in `slots_`.
  CHECK_EQ(current_model_index, model_index) << "model_index is too large";
  return slots_.size();
}

int TabStripLayoutHelper::GetSlotIndexForGroupHeader(
    tab_groups::TabGroupId group) const {
  const auto it = std::ranges::find_if(slots_, [group](const auto& slot) {
    return slot.type == TabSlotView::ViewType::kTabGroupHeader &&
           static_cast<TabGroupHeader*>(slot.view)->group() == group;
  });
  CHECK(it != slots_.end());
  return it - slots_.begin();
}

std::optional<int> TabStripLayoutHelper::GetAdjacentSplitTab(
    int tab_index) const {
  const std::optional<split_tabs::SplitTabId> split_id =
      slots_[tab_index].view->split();
  if (!split_id.has_value()) {
    return std::nullopt;
  } else if (tab_index > 0 && slots_[tab_index - 1].view->split() == split_id) {
    return tab_index - 1;
  } else if (tab_index < static_cast<int>(slots_.size()) - 1 &&
             slots_[tab_index + 1].view->split() == split_id) {
    return tab_index + 1;
  }
  return std::nullopt;
}

bool TabStripLayoutHelper::SlotIsCollapsedTab(int i) const {
  // The slot can only be collapsed if it is a tab and in a collapsed group.
  // If the slot is indeed a tab and in a group, check the collapsed state of
  // the group to determine if it is collapsed.
  const std::optional<tab_groups::TabGroupId> id = slots_[i].view->group();
  return slots_[i].type == TabSlotView::ViewType::kTab && id.has_value() &&
         controller_->IsGroupCollapsed(id.value());
}
