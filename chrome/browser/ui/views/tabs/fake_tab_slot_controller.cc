// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"

const ui::ListSelectionModel& FakeTabSlotController::GetSelectionModel() const {
  return selection_model_;
}

Tab* FakeTabSlotController::tab_at(int index) const {
  return nullptr;
}

int FakeTabSlotController::GetActiveIndex() const {
  return 0;
}

bool FakeTabSlotController::ToggleTabGroupCollapsedState(
    const tab_groups::TabGroupId group,
    ToggleTabGroupCollapsedStateOrigin origin) {
  return false;
}

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

bool FakeTabSlotController::EndDrag(EndDragReason reason) {
  return false;
}

Tab* FakeTabSlotController::GetTabAt(const gfx::Point& point) {
  return nullptr;
}

const Tab* FakeTabSlotController::GetAdjacentTab(const Tab* tab, int offset) {
  return nullptr;
}

bool FakeTabSlotController::ShowDomainInHoverCards() const {
  return true;
}

bool FakeTabSlotController::HoverCardIsShowingForTab(Tab* tab) {
  return false;
}

int FakeTabSlotController::GetBackgroundOffset() const {
  return 0;
}

bool FakeTabSlotController::ShouldPaintAsActiveFrame() const {
  return true;
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

SkColor FakeTabSlotController::GetTabBackgroundColor(
    TabActive active,
    BrowserFrameActiveState active_state) const {
  return active == TabActive::kActive ? tab_bg_color_active_
                                      : tab_bg_color_inactive_;
}

SkColor FakeTabSlotController::GetTabForegroundColor(TabActive active) const {
  return active == TabActive::kActive ? tab_fg_color_active_
                                      : tab_fg_color_inactive_;
}

absl::optional<int> FakeTabSlotController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return absl::nullopt;
}

gfx::Rect FakeTabSlotController::GetTabAnimationTargetBounds(const Tab* tab) {
  return tab->bounds();
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
  return std::u16string();
}

std::u16string FakeTabSlotController::GetGroupContentString(
    const tab_groups::TabGroupId& group) const {
  return std::u16string();
}

tab_groups::TabGroupColorId FakeTabSlotController::GetGroupColorId(
    const tab_groups::TabGroupId& group_id) const {
  return tab_groups::TabGroupColorId();
}

bool FakeTabSlotController::IsGroupCollapsed(
    const tab_groups::TabGroupId& group) const {
  return false;
}

absl::optional<int> FakeTabSlotController::GetLastTabInGroup(
    const tab_groups::TabGroupId& group) const {
  return absl::nullopt;
}

SkColor FakeTabSlotController::GetPaintedGroupColor(
    const tab_groups::TabGroupColorId& color_id) const {
  return SkColor();
}

const Browser* FakeTabSlotController::GetBrowser() const {
  return nullptr;
}
