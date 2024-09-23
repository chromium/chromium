// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

#include <utility>

#include "base/containers/contains.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/range/range.h"

FakeBaseTabStripController::FakeBaseTabStripController() = default;

FakeBaseTabStripController::~FakeBaseTabStripController() = default;

void FakeBaseTabStripController::AddTab(int index,
                                        TabActive is_active,
                                        TabPinned is_pinned) {
  num_tabs_++;
  tab_groups_.insert(tab_groups_.begin() + index, std::nullopt);

  TabRendererData data;
  if (is_pinned == TabPinned::kPinned) {
    num_pinned_tabs_++;
    data.pinned = true;
  }
  if (tab_strip_)
    tab_strip_->AddTabAt(index, std::move(data));
  if (is_active == TabActive::kActive) {
    SetActiveIndex(index);
  } else if (active_index_.has_value() && index <= active_index_) {
    SetActiveIndex(active_index_.value() + 1);
  }
}

void FakeBaseTabStripController::MoveTab(int from_index, int to_index) {
  std::optional<tab_groups::TabGroupId> prev_group = tab_groups_[from_index];
  tab_groups_.erase(tab_groups_.begin() + from_index);
  tab_groups_.insert(tab_groups_.begin() + to_index, prev_group);
  if (tab_strip_)
    tab_strip_->MoveTab(from_index, to_index, TabRendererData());
}

void FakeBaseTabStripController::MoveGroup(const tab_groups::TabGroupId& group,
                                           int to_index) {}

void FakeBaseTabStripController::ToggleTabGroupCollapsedState(
    const tab_groups::TabGroupId group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  fake_group_data_ = tab_groups::TabGroupVisualData(
      fake_group_data_.title(), fake_group_data_.color(),
      !fake_group_data_.is_collapsed());
}

void FakeBaseTabStripController::RemoveTab(int index) {
  DCHECK(IsValidIndex(index));
  num_tabs_--;
  if (index < num_pinned_tabs_)
    num_pinned_tabs_--;
  tab_groups_.erase(tab_groups_.begin() + index);

  const bool was_active = index == active_index_;

  // RemoveTabAt() expects the controller state to have been updated already.
  if (active_index_.has_value()) {
    if (was_active) {
      if (num_tabs_ > 0) {
        active_index_ = std::min(active_index_.value(), num_tabs_ - 1);
      } else {
        active_index_ = std::nullopt;
      }
    } else if (active_index_ > index) {
      active_index_ = active_index_.value() - 1;
    }
    selection_model_.SetSelectedIndex(active_index_);
  }

  if (tab_strip_) {
    tab_strip_->RemoveTabAt(nullptr, index, was_active);
    if (active_index_.has_value() && IsValidIndex(active_index_.value()))
      tab_strip_->SetSelection(selection_model_);
  }
}

std::u16string FakeBaseTabStripController::GetGroupTitle(
    const tab_groups::TabGroupId& group_id) const {
  return fake_group_data_.title();
}

std::u16string FakeBaseTabStripController::GetGroupContentString(
    const tab_groups::TabGroupId& group_id) const {
  return std::u16string();
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
  MoveTabIntoGroup(model_index, std::nullopt);
}

void FakeBaseTabStripController::MoveTabIntoGroup(
    int index,
    std::optional<tab_groups::TabGroupId> new_group) {
  bool group_exists = base::Contains(tab_groups_, new_group);
  std::optional<tab_groups::TabGroupId> old_group = tab_groups_[index];

  tab_groups_[index] = new_group;

  if (tab_strip_ && old_group.has_value()) {
    tab_strip_->AddTabToGroup(std::nullopt, index);
    if (!base::Contains(tab_groups_, old_group))
      tab_strip_->OnGroupClosed(old_group.value());
    else
      tab_strip_->OnGroupContentsChanged(old_group.value());
  }
  if (tab_strip_ && new_group.has_value()) {
    if (!group_exists)
      tab_strip_->OnGroupCreated(new_group.value());
    tab_strip_->AddTabToGroup(new_group.value(), index);
    tab_strip_->OnGroupContentsChanged(new_group.value());
  }
}

std::optional<int> FakeBaseTabStripController::GetFirstTabInGroup(
    const tab_groups::TabGroupId& group) const {
  for (size_t i = 0; i < tab_groups_.size(); ++i) {
    if (tab_groups_[i] == group)
      return i;
  }

  return std::nullopt;
}

gfx::Range FakeBaseTabStripController::ListTabsInGroup(
    const tab_groups::TabGroupId& group) const {
  int first_tab = -1;
  int last_tab = -1;
  for (size_t i = 0; i < tab_groups_.size(); i++) {
    if (tab_groups_[i] != group)
      continue;

    if (first_tab == -1) {
      first_tab = i;
      last_tab = i + 1;
      continue;
    }

    DCHECK_EQ(static_cast<int>(i), last_tab) << "group is not contiguous";
    last_tab = i + 1;
  }

  return first_tab > -1 ? gfx::Range(first_tab, last_tab) : gfx::Range();
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

std::optional<int> FakeBaseTabStripController::GetActiveIndex() const {
  return active_index_;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return GetSelectionModel().IsSelected(index);
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return index < num_pinned_tabs_;
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

void FakeBaseTabStripController::OnCloseTab(
    int index,
    CloseTabSource source,
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

void FakeBaseTabStripController::ToggleTabAudioMute(int index) {}

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

void FakeBaseTabStripController::OnDropIndexUpdate(std::optional<int> index,
                                                   bool drop_before) {}

void FakeBaseTabStripController::CreateNewTab() {
  AddTab(num_tabs_, TabActive::kActive);
}

void FakeBaseTabStripController::CreateNewTabWithLocation(
    const std::u16string& location) {}

void FakeBaseTabStripController::OnStartedDragging(bool dragging_window) {}

void FakeBaseTabStripController::OnStoppedDragging() {}

void FakeBaseTabStripController::OnKeyboardFocusedTabChanged(
    std::optional<int> index) {}

bool FakeBaseTabStripController::IsFrameCondensed() const {
  return false;
}

bool FakeBaseTabStripController::HasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::EverHasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::CanDrawStrokes() const {
  return false;
}

bool FakeBaseTabStripController::IsFrameButtonsRightAligned() const {
  return false;
}

SkColor FakeBaseTabStripController::GetFrameColor(
    BrowserFrameActiveState active_state) const {
  return gfx::kPlaceholderColor;
}

std::optional<int> FakeBaseTabStripController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return std::nullopt;
}

std::u16string FakeBaseTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return std::u16string();
}

Profile* FakeBaseTabStripController::GetProfile() const {
  return nullptr;
}

const Browser* FakeBaseTabStripController::GetBrowser() const {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool FakeBaseTabStripController::IsLockedForOnTask() {
  return on_task_locked_;
}
#endif

void FakeBaseTabStripController::SetActiveIndex(int new_index) {
  DCHECK(IsValidIndex(new_index));
  active_index_ = new_index;
  selection_model_.SetSelectedIndex(active_index_);
  if (tab_strip_)
    tab_strip_->SetSelection(selection_model_);
}
