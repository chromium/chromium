// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"

#include <memory>
#include <set>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
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
#include "tab_container_controller.h"
#include "ui/gfx/range/range.h"
#include "ui/views/view_model.h"

namespace {

// The types of TabSlotView that can be referenced by TabSlot.
enum class ViewType {
  kTab,
  kGroupHeader,
};

}  // namespace

struct TabStripLayoutHelper::TabSlot {
  static TabStripLayoutHelper::TabSlot CreateForTab(Tab* tab,
                                                    TabOpen open,
                                                    TabPinned pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kTab;
    slot.view = tab;
    slot.state = TabLayoutState(open, pinned, TabActive::kInactive);
    return slot;
  }

  static TabStripLayoutHelper::TabSlot CreateForGroupHeader(
      tab_groups::TabGroupId group,
      TabGroupHeader* header,
      TabPinned pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kGroupHeader;
    slot.view = header;
    slot.state = TabLayoutState(TabOpen::kOpen, pinned, TabActive::kInactive);
    return slot;
  }

  ViewType type;
  raw_ptr<TabSlotView, DanglingUntriaged> view;
  TabLayoutState state;
};

TabStripLayoutHelper::TabStripLayoutHelper(
    const TabContainerController& controller,
    GetTabsCallback get_tabs_callback)
    : controller_(controller),
      get_tabs_callback_(get_tabs_callback),
      active_tab_width_(TabStyle::Get()->GetStandardWidth()),
      inactive_tab_width_(TabStyle::Get()->GetStandardWidth()) {}

TabStripLayoutHelper::~TabStripLayoutHelper() = default;

std::vector<Tab*> TabStripLayoutHelper::GetTabs() const {
  std::vector<Tab*> tabs;
  for (const TabSlot& slot : slots_) {
    if (slot.type == ViewType::kTab)
      tabs.push_back(static_cast<Tab*>(slot.view));
  }

  return tabs;
}

std::vector<TabSlotView*> TabStripLayoutHelper::GetTabSlotViews() const {
  std::vector<TabSlotView*> views;
  for (const TabSlot& slot : slots_)
    views.push_back(slot.view);
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
                TabSlot::CreateForTab(tab, TabOpen::kOpen, pinned));
}

void TabStripLayoutHelper::MarkTabAsClosing(int model_index, Tab* tab) {
  const int slot_index = GetSlotIndexForExistingTab(model_index);
  slots_[slot_index].state =
      slots_[slot_index].state.WithOpen(TabOpen::kClosed);
}

void TabStripLayoutHelper::RemoveTab(Tab* tab) {
  auto it = base::ranges::find_if(slots_, [tab](const TabSlot& slot) {
    return slot.type == ViewType::kTab && slot.view == tab;
  });
  if (it != slots_.end())
    slots_.erase(it);
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

  if (moving_tab_group.has_value())
    UpdateGroupHeaderIndex(moving_tab_group.value());
}

void TabStripLayoutHelper::SetTabPinned(int model_index, TabPinned pinned) {
  const int slot_index = GetSlotIndexForExistingTab(model_index);
  slots_[slot_index].state = slots_[slot_index].state.WithPinned(pinned);
}

void TabStripLayoutHelper::InsertGroupHeader(tab_groups::TabGroupId group,
                                             TabGroupHeader* header) {
  gfx::Range tabs_in_group = controller_->ListTabsInGroup(group);
  const int header_slot_index =
      GetSlotInsertionIndexForNewTab(tabs_in_group.start(), group);
  slots_.insert(
      slots_.begin() + header_slot_index,
      TabSlot::CreateForGroupHeader(group, header, TabPinned::kUnpinned));

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
  const int slot_index = GetSlotIndexForGroupHeader(group);
  TabSlot header_slot = slots_[slot_index];

  slots_.erase(slots_.begin() + slot_index);
  std::optional<int> first_tab = GetFirstTabSlotForGroup(group);
  CHECK(first_tab);
  slots_.insert(slots_.begin() + first_tab.value(), header_slot);
}

void TabStripLayoutHelper::SetActiveTab(
    std::optional<size_t> prev_active_index,
    std::optional<size_t> new_active_index) {
  if (prev_active_index.has_value()) {
    const int prev_slot_index =
        GetSlotIndexForExistingTab(prev_active_index.value());
    slots_[prev_slot_index].state =
        slots_[prev_slot_index].state.WithActive(TabActive::kInactive);
  }
  if (new_active_index.has_value()) {
    const int new_slot_index =
        GetSlotIndexForExistingTab(new_active_index.value());
    slots_[new_slot_index].state =
        slots_[new_slot_index].state.WithActive(TabActive::kActive);
  }
}

int TabStripLayoutHelper::CalculateMinimumWidth() {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(0);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::CalculatePreferredWidth() {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(std::nullopt);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::UpdateIdealBounds(int available_width) {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(available_width);
  DCHECK_EQ(slots_.size(), bounds.size());

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  const std::optional<int> active_tab_model_index =
      controller_->GetActiveIndex();
  const std::optional<int> active_tab_slot_index =
      active_tab_model_index.has_value()
          ? std::optional<int>(
                GetSlotIndexForExistingTab(active_tab_model_index.value()))
          : std::nullopt;

  int current_tab_model_index = 0;
  for (int i = 0; i < static_cast<int>(bounds.size()); ++i) {
    const TabSlot& slot = slots_[i];
    switch (slot.type) {
      case ViewType::kTab:
        if (!slot.state.IsClosed()) {
          tabs->set_ideal_bounds(current_tab_model_index, bounds[i]);
          UpdateCachedTabWidth(i, bounds[i].width(),
                               i == active_tab_slot_index);
          ++current_tab_model_index;
        }
        break;
      case ViewType::kGroupHeader:
        group_header_ideal_bounds_[slot.view->group().value()] = bounds[i];
        break;
    }
  }

  return bounds.back().right();
}

std::vector<gfx::Rect> TabStripLayoutHelper::CalculateIdealBounds(
    std::optional<int> available_width) {
  std::optional<int> tabstrip_width = available_width;

  const std::optional<int> active_tab_model_index =
      controller_->GetActiveIndex();
  const std::optional<int> active_tab_slot_index =
      active_tab_model_index.has_value()
          ? std::optional<int>(
                GetSlotIndexForExistingTab(active_tab_model_index.value()))
          : std::nullopt;
  const int pinned_tab_count = GetPinnedTabCount();
  const std::optional<int> last_pinned_tab_slot_index =
      pinned_tab_count > 0
          ? std::optional<int>(GetSlotIndexForExistingTab(pinned_tab_count - 1))
          : std::nullopt;

  TabLayoutConstants layout_constants = {GetLayoutConstant(TAB_STRIP_HEIGHT),
                                         TabStyle::Get()->GetTabOverlap()};
  std::vector<TabWidthConstraints> tab_widths;
  for (int i = 0; i < static_cast<int>(slots_.size()); i++) {
    auto active =
        i == active_tab_slot_index ? TabActive::kActive : TabActive::kInactive;
    auto pinned = last_pinned_tab_slot_index.has_value() &&
                          i <= last_pinned_tab_slot_index
                      ? TabPinned::kPinned
                      : TabPinned::kUnpinned;

    // A collapsed tab animates closed like a closed tab.
    auto open = (slots_[i].state.IsClosed() || SlotIsCollapsedTab(i))
                    ? TabOpen::kClosed
                    : TabOpen::kOpen;
    TabLayoutState state = TabLayoutState(open, pinned, active);
    TabSizeInfo size_info = slots_[i].view->GetTabSizeInfo();

    tab_widths.emplace_back(state, layout_constants, size_info);
  }

  return CalculateTabBounds(layout_constants, tab_widths, tabstrip_width);
}

int TabStripLayoutHelper::GetSlotIndexForExistingTab(int model_index) const {
  const int original_slot_index =
      GetFirstSlotIndexForTabModelIndex(model_index);
  CHECK_LT(original_slot_index, static_cast<int>(slots_.size()))
      << "model_index = " << model_index
      << " does not represent an existing tab";

  int slot_index = original_slot_index;

  if (slots_[slot_index].type == ViewType::kTab) {
    CHECK(!slots_[slot_index].state.IsClosed());
    return slot_index;
  }

  // If |slot_index| is a group header we must return the next slot that
  // is not animating closed.
  if (slots_[slot_index].type == ViewType::kGroupHeader) {
    // Skip all slots animating closed.
    do {
      slot_index += 1;
    } while (slot_index < static_cast<int>(slots_.size()) &&
             slots_[slot_index].state.IsClosed());

    // Double check we arrived at a tab.
    CHECK_LT(slot_index, static_cast<int>(slots_.size()))
        << "group header at " << original_slot_index
        << " not followed by an open tab";
    CHECK_EQ(slots_[slot_index].type, ViewType::kTab);
  }

  return slot_index;
}

int TabStripLayoutHelper::GetSlotInsertionIndexForNewTab(
    int new_model_index,
    std::optional<tab_groups::TabGroupId> group) const {
  int slot_index = GetFirstSlotIndexForTabModelIndex(new_model_index);

  if (slot_index == static_cast<int>(slots_.size()))
    return slot_index;

  // If |slot_index| points to a group header and the new tab's |group|
  // matches, the tab goes to the right of the header to keep it
  // contiguous.
  if (slots_[slot_index].type == ViewType::kGroupHeader &&
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

    if (slots_[slot_index].type == ViewType::kTab &&
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
    if (slots_[slot_index].state.IsClosed())
      continue;

    if (model_index == current_model_index)
      return slot_index;

    if (slots_[slot_index].type == ViewType::kTab)
      current_model_index += 1;
  }

  // If there's no slot in |slots_| corresponding to |model_index|, then
  // |model_index| may represent the first tab past the end of the
  // tabstrip. In this case we should return the first-past-the-end
  // index in |slots_|.
  CHECK_EQ(current_model_index, model_index) << "model_index is too large";
  return slots_.size();
}

int TabStripLayoutHelper::GetSlotIndexForGroupHeader(
    tab_groups::TabGroupId group) const {
  const auto it = base::ranges::find_if(slots_, [group](const auto& slot) {
    return slot.type == ViewType::kGroupHeader &&
           static_cast<TabGroupHeader*>(slot.view)->group() == group;
  });
  CHECK(it != slots_.end());
  return it - slots_.begin();
}

void TabStripLayoutHelper::UpdateCachedTabWidth(int tab_index,
                                                int tab_width,
                                                bool active) {
  // If the slot is collapsed, its width should never be reported as the
  // current active or inactive tab width - it's not even visible.
  if (SlotIsCollapsedTab(tab_index))
    return;
  if (active)
    active_tab_width_ = tab_width;
  else
    inactive_tab_width_ = tab_width;
}

bool TabStripLayoutHelper::SlotIsCollapsedTab(int i) const {
  // The slot can only be collapsed if it is a tab and in a collapsed group.
  // If the slot is indeed a tab and in a group, check the collapsed state of
  // the group to determine if it is collapsed.
  const std::optional<tab_groups::TabGroupId> id = slots_[i].view->group();
  return slots_[i].type == ViewType::kTab && id.has_value() &&
         controller_->IsGroupCollapsed(id.value());
}
