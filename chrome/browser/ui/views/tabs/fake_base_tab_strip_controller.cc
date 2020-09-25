// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

FakeBaseTabStripController::FakeBaseTabStripController() {}

FakeBaseTabStripController::~FakeBaseTabStripController() {
}

void FakeBaseTabStripController::AddTab(int index, bool is_active) {
  num_tabs_++;
  tab_strip_->AddTabAt(index, TabRendererData(), is_active);
  if (is_active) {
    SelectTab(index,
              ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                             base::TimeTicks::Now(), 0, 0));
  }
}

void FakeBaseTabStripController::AddPinnedTab(int index, bool is_active) {
  TabRendererData data;
  data.pinned = true;
  num_tabs_++;
  tab_strip_->AddTabAt(index, std::move(data), is_active);
  if (is_active)
    active_index_ = index;
}

void FakeBaseTabStripController::MoveTab(int from_index, int to_index) {
  base::Optional<tab_groups::TabGroupId> prev_group;
  if (from_index < int{tab_groups_.size()}) {
    prev_group = tab_groups_[from_index];
    tab_groups_.erase(tab_groups_.begin() + from_index);
  }
  if (to_index >= int{tab_groups_.size()})
    tab_groups_.resize(to_index + 1);
  tab_groups_.insert(tab_groups_.begin() + to_index, prev_group);
  tab_strip_->MoveTab(from_index, to_index, TabRendererData());
}
void FakeBaseTabStripController::MoveGroup(const tab_groups::TabGroupId& group,
                                           int to_index) {}

bool FakeBaseTabStripController::ToggleTabGroupCollapsedState(
    const tab_groups::TabGroupId group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  fake_group_data_ = tab_groups::TabGroupVisualData(
      fake_group_data_.title(), fake_group_data_.color(),
      !fake_group_data_.is_collapsed());
  return true;
}

void FakeBaseTabStripController::RemoveTab(int index) {
  num_tabs_--;
  // RemoveTabAt() expects the controller state to have been updated already.
  const bool was_active = index == active_index_;
  if (was_active) {
    active_index_ = std::min(active_index_, num_tabs_ - 1);
    selection_model_.SetSelectedIndex(active_index_);
  } else if (active_index_ > index) {
    --active_index_;
  }
  tab_strip_->RemoveTabAt(nullptr, index, was_active);
  if (was_active && IsValidIndex(active_index_))
    tab_strip_->SetSelection(selection_model_);
}

base::string16 FakeBaseTabStripController::GetGroupTitle(
    const tab_groups::TabGroupId& group_id) const {
  return fake_group_data_.title();
}

base::string16 FakeBaseTabStripController::GetGroupContentString(
    const tab_groups::TabGroupId& group_id) const {
  return base::string16();
}

tab_groups::TabGroupColorId FakeBaseTabStripController::GetGroupColorId(
    const tab_groups::TabGroupId& group_id) const {
  return fake_group_data_.color();
}

bool FakeBaseTabStripController::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  return fake_group_data_.is_collapsed();
}

void FakeBaseTabStripController::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  fake_group_data_ = visual_data;
}

void FakeBaseTabStripController::AddTabToGroup(
    int model_index,
    const tab_groups::TabGroupId& group) {
  MoveTabIntoGroup(model_index, group);
}

void FakeBaseTabStripController::RemoveTabFromGroup(int model_index) {
  MoveTabIntoGroup(model_index, base::nullopt);
}

void FakeBaseTabStripController::MoveTabIntoGroup(
    int index,
    base::Optional<tab_groups::TabGroupId> new_group) {
  bool group_exists = base::Contains(tab_groups_, new_group);
  base::Optional<tab_groups::TabGroupId> old_group;
  if (index >= int{tab_groups_.size()})
    tab_groups_.resize(index + 1);
  else
    old_group = tab_groups_[index];

  tab_groups_[index] = new_group;

  if (old_group.has_value()) {
    tab_strip_->AddTabToGroup(base::nullopt, index);
    if (!base::Contains(tab_groups_, old_group))
      tab_strip_->OnGroupClosed(old_group.value());
    else
      tab_strip_->OnGroupContentsChanged(old_group.value());
  }
  if (new_group.has_value()) {
    if (!group_exists)
      tab_strip_->OnGroupCreated(new_group.value());
    tab_strip_->AddTabToGroup(new_group.value(), index);
    tab_strip_->OnGroupContentsChanged(new_group.value());
  }
}

std::vector<int> FakeBaseTabStripController::ListTabsInGroup(
    const tab_groups::TabGroupId& group) const {
  std::vector<int> result;
  for (size_t i = 0; i < tab_groups_.size(); i++) {
    if (tab_groups_[i] == group)
      result.push_back(i);
  }
  return result;
}

const ui::ListSelectionModel&
FakeBaseTabStripController::GetSelectionModel() const {
  return selection_model_;
}

int FakeBaseTabStripController::GetCount() const {
  return num_tabs_;
}

bool FakeBaseTabStripController::IsValidIndex(int index) const {
  return index >= 0 && index < num_tabs_;
}

bool FakeBaseTabStripController::IsActiveTab(int index) const {
  if (!IsValidIndex(index))
    return false;
  return active_index_ == index;
}

int FakeBaseTabStripController::GetActiveIndex() const {
  return active_index_;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return false;
}

void FakeBaseTabStripController::SelectTab(int index, const ui::Event& event) {
  if (!IsValidIndex(index) || active_index_ == index)
    return;

  SetActiveIndex(index);
}

void FakeBaseTabStripController::ExtendSelectionTo(int index) {
}

void FakeBaseTabStripController::ToggleSelected(int index) {
}

void FakeBaseTabStripController::AddSelectionFromAnchorTo(int index) {
}

bool FakeBaseTabStripController::BeforeCloseTab(int index,
                                                CloseTabSource source) {
  return true;
}

void FakeBaseTabStripController::CloseTab(int index) {
  RemoveTab(index);
}

void FakeBaseTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
}

int FakeBaseTabStripController::HasAvailableDragActions() const {
  return 0;
}

void FakeBaseTabStripController::OnDropIndexUpdate(int index,
                                                   bool drop_before) {
}

void FakeBaseTabStripController::CreateNewTab() {
  AddTab(num_tabs_, true);
}

void FakeBaseTabStripController::CreateNewTabWithLocation(
    const base::string16& location) {
}

void FakeBaseTabStripController::StackedLayoutMaybeChanged() {
}

void FakeBaseTabStripController::OnStartedDragging(bool dragging_window) {}

void FakeBaseTabStripController::OnStoppedDragging() {}

void FakeBaseTabStripController::OnKeyboardFocusedTabChanged(
    base::Optional<int> index) {}

bool FakeBaseTabStripController::IsFrameCondensed() const {
  return false;
}

bool FakeBaseTabStripController::HasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::EverHasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::ShouldPaintAsActiveFrame() const {
  return true;
}

bool FakeBaseTabStripController::CanDrawStrokes() const {
  return false;
}

SkColor FakeBaseTabStripController::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  return gfx::kPlaceholderColor;
}

SkColor FakeBaseTabStripController::GetToolbarTopSeparatorColor() const {
  return gfx::kPlaceholderColor;
}

base::Optional<int> FakeBaseTabStripController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return base::nullopt;
}

base::string16 FakeBaseTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return base::string16();
}

Profile* FakeBaseTabStripController::GetProfile() const {
  return nullptr;
}

const Browser* FakeBaseTabStripController::GetBrowser() const {
  return nullptr;
}

void FakeBaseTabStripController::SetActiveIndex(int new_index) {
  active_index_ = new_index;
  selection_model_.SetSelectedIndex(active_index_);
  if (IsValidIndex(active_index_))
    tab_strip_->SetSelection(selection_model_);
}
