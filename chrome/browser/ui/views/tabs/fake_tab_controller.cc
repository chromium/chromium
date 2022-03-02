// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_tab_controller.h"

const ui::ListSelectionModel& FakeTabController::GetSelectionModel() const {
  return selection_model_;
}

bool FakeTabController::IsActiveTab(const Tab* tab) const {
  return active_tab_;
}

bool FakeTabController::IsTabSelected(const Tab* tab) const {
  return false;
}

bool FakeTabController::IsTabPinned(const Tab* tab) const {
  return false;
}

bool FakeTabController::IsTabFirst(const Tab* tab) const {
  return false;
}

bool FakeTabController::IsFocusInTabs() const {
  return false;
}

bool FakeTabController::EndDrag(EndDragReason reason) {
  return false;
}

Tab* FakeTabController::GetTabAt(const gfx::Point& point) {
  return nullptr;
}

const Tab* FakeTabController::GetAdjacentTab(const Tab* tab, int offset) {
  return nullptr;
}

bool FakeTabController::ShowDomainInHoverCards() const {
  return true;
}

bool FakeTabController::HoverCardIsShowingForTab(Tab* tab) {
  return false;
}

int FakeTabController::GetBackgroundOffset() const {
  return 0;
}

bool FakeTabController::ShouldPaintAsActiveFrame() const {
  return true;
}

int FakeTabController::GetStrokeThickness() const {
  return 0;
}

bool FakeTabController::CanPaintThrobberToLayer() const {
  return paint_throbber_to_layer_;
}

bool FakeTabController::HasVisibleBackgroundTabShapes() const {
  return false;
}

SkColor FakeTabController::GetTabSeparatorColor() const {
  return SK_ColorBLACK;
}

SkColor FakeTabController::GetTabBackgroundColor(
    TabActive active,
    BrowserFrameActiveState active_state) const {
  return active == TabActive::kActive ? tab_bg_color_active_
                                      : tab_bg_color_inactive_;
}

SkColor FakeTabController::GetTabForegroundColor(
    TabActive active,
    SkColor background_color) const {
  return active == TabActive::kActive ? tab_fg_color_active_
                                      : tab_fg_color_inactive_;
}

absl::optional<int> FakeTabController::GetCustomBackgroundId(
    BrowserFrameActiveState active_state) const {
  return absl::nullopt;
}

gfx::Rect FakeTabController::GetTabAnimationTargetBounds(const Tab* tab) {
  return tab->bounds();
}

std::u16string FakeTabController::GetAccessibleTabName(const Tab* tab) const {
  return std::u16string();
}

float FakeTabController::GetHoverOpacityForTab(float range_parameter) const {
  return 1.0f;
}

float FakeTabController::GetHoverOpacityForRadialHighlight() const {
  return 1.0f;
}

std::u16string FakeTabController::GetGroupTitle(
    const tab_groups::TabGroupId& group_id) const {
  return std::u16string();
}

tab_groups::TabGroupColorId FakeTabController::GetGroupColorId(
    const tab_groups::TabGroupId& group_id) const {
  return tab_groups::TabGroupColorId();
}

SkColor FakeTabController::GetPaintedGroupColor(
    const tab_groups::TabGroupColorId& color_id) const {
  return SkColor();
}
