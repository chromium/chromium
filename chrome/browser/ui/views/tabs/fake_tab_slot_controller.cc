// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"

#include "chrome/browser/ui/views/tabs/tab_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"

FakeTabSlotController::FakeTabSlotController(
    TabStripController* tab_strip_controller)
    : tab_strip_controller_(tab_strip_controller) {}

const ui::ListSelectionModel& FakeTabSlotController::GetSelectionModel() const {
  return selection_model_;
}

Tab* FakeTabSlotController::tab_at(int index) const {
  return tab_container_->GetTabAtModelIndex(index);
}

void FakeTabSlotController::ToggleTabGroupCollapsedState(
    const tab_groups::TabGroupId group,
    ToggleTabGroupCollapsedStateOrigin origin) {}

bool FakeTabSlotController::IsActiveTab(const Tab* tab) const {
  return active_tab_ == tab;
}

bool FakeTabSlotController::IsTabSelected(const Tab* tab) const {
  return false;
}

bool FakeTabSlotController::IsTabPinned(const Tab* tab) const {
  return false;
}

bool FakeTabSlotController::IsTabFirst(const Tab* tab) const {
  return false;
}

bool FakeTabSlotController::IsFocusInTabs() const {
  return false;
}

bool FakeTabSlotController::ShouldCompactLeadingEdge() const {
  return true;
}

TabSlotController::Liveness FakeTabSlotController::ContinueDrag(
    views::View* view,
    const ui::LocatedEvent& event) {
  return Liveness::kAlive;
}

bool FakeTabSlotController::EndDrag(EndDragReason reason) {
  return false;
}

Tab* FakeTabSlotController::GetTabAt(const gfx::Point& point) {
  return nullptr;
}

const Tab* FakeTabSlotController::GetAdjacentTab(const Tab* tab, int offset) {
  return nullptr;
}

bool FakeTabSlotController::HoverCardIsShowingForTab(Tab* tab) {
  return false;
}

int FakeTabSlotController::GetBackgroundOffset() const {
  return 0;
}

int FakeTabSlotController::GetStrokeThickness() const {
  return 0;
}

bool FakeTabSlotController::CanPaintThrobberToLayer() const {
  return paint_throbber_to_layer_;
}

bool FakeTabSlotController::HasVisibleBackgroundTabShapes() const {
  return false;
}

SkColor FakeTabSlotController::GetTabSeparatorColor() const {
  return SK_ColorBLACK;
}

SkColor FakeTabSlotController::GetTabForegroundColor(TabActive active) const {
  return active == TabActive::kActive ? tab_fg_color_active_
                                      : tab_fg_color_inactive_;
}

std::optional<int> FakeTabSlotController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return std::nullopt;
}

std::u16string FakeTabSlotController::GetAccessibleTabName(
    const Tab* tab) const {
  return std::u16string();
}

float FakeTabSlotController::GetHoverOpacityForTab(
    float range_parameter) const {
  return 1.0f;
}

float FakeTabSlotController::GetHoverOpacityForRadialHighlight() const {
  return 1.0f;
}

std::u16string FakeTabSlotController::GetGroupTitle(
    const tab_groups::TabGroupId& group_id) const {
  return tab_strip_controller_->GetGroupTitle(group_id);
}

std::u16string FakeTabSlotController::GetGroupContentString(
    const tab_groups::TabGroupId& group) const {
  return tab_strip_controller_->GetGroupContentString(group);
}

tab_groups::TabGroupColorId FakeTabSlotController::GetGroupColorId(
    const tab_groups::TabGroupId& group_id) const {
  return tab_strip_controller_->GetGroupColorId(group_id);
}

bool FakeTabSlotController::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  return tab_strip_controller_->IsGroupCollapsed(group);
}

SkColor FakeTabSlotController::GetPaintedGroupColor(
    const tab_groups::TabGroupColorId& color_id) const {
  return SkColor();
}

const Browser* FakeTabSlotController::GetBrowser() const {
  return nullptr;
}

int FakeTabSlotController::GetInactiveTabWidth() const {
  return inactive_tab_width_;
}

bool FakeTabSlotController::IsFrameCondensed() const {
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool FakeTabSlotController::IsLockedForOnTask() {
  return on_task_locked_;
}
#endif
