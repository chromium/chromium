// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/test_location_bar.h"

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

TestLocationBar::TestLocationBar(CommandUpdater* command_updater,
                                 LocationBarModel* location_bar_model)
    : LocationBar(command_updater), location_bar_model_(location_bar_model) {}

TestLocationBar::~TestLocationBar() = default;

void TestLocationBar::FocusLocation(bool select_all,
                                    bool clear_focus_if_failed) {}

void TestLocationBar::FocusSearch() {}

void TestLocationBar::UpdateFocusBehavior(bool toolbar_visible) {}

void TestLocationBar::UpdateContentSettingsIcons() {}

void TestLocationBar::SaveStateToContents(content::WebContents* contents) {}

void TestLocationBar::Revert() {}

OmniboxView* TestLocationBar::GetOmniboxView() {
  return omnibox_view_;
}

OmniboxPopupView* TestLocationBar::GetOmniboxPopupView() {
  return nullptr;
}

OmniboxController* TestLocationBar::GetOmniboxController() {
  return nullptr;
}

bool TestLocationBar::ShouldCloseOmniboxPopup(ui::MouseEvent* event) {
  return false;
}

ChipController* TestLocationBar::GetChipController() {
  return nullptr;
}

LocationBarTesting* TestLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

LocationBarModel* TestLocationBar::GetLocationBarModel() {
  return location_bar_model_;
}

content::WebContents* TestLocationBar::GetWebContents() {
  return nullptr;
}

std::optional<bubble_anchor_util::AnchorConfiguration>
TestLocationBar::GetChipAnchor() {
  return std::nullopt;
}

void TestLocationBar::AnnounceAlert(const std::u16string& announcement) {}

void TestLocationBar::OnChanged() {}

void TestLocationBar::UpdateWithoutTabRestore() {
  if (omnibox_view_) {
    omnibox_view_->Update();
  }
}

ui::TrackedElement* TestLocationBar::GetAnchorOrNull() {
  return nullptr;
}

BrowserWindowInterface* TestLocationBar::GetBrowser() {
  return nullptr;
}

Profile* TestLocationBar::GetProfile() {
  return profile_;
}

bool TestLocationBar::IsInitialized() const {
  return true;
}

bool TestLocationBar::IsVisible() const {
  return true;
}

bool TestLocationBar::IsDrawn() const {
  return true;
}

bool TestLocationBar::IsFullscreen() const {
  return false;
}

bool TestLocationBar::IsEditingOrEmpty() const {
  return false;
}

bool TestLocationBar::IsMouseHovered() const {
  return false;
}

bool TestLocationBar::IsFocusWithin() const {
  return false;
}

void TestLocationBar::InvalidateLayout() {}

gfx::Rect TestLocationBar::Bounds() const {
  return bounds_;
}

gfx::Rect TestLocationBar::BoundsInScreen() const {
  return bounds_in_screen_;
}

gfx::Size TestLocationBar::MinimumSize() const {
  return bounds_.size();
}

gfx::Size TestLocationBar::PreferredSize() const {
  return bounds_.size();
}

void TestLocationBar::Update(content::WebContents* contents) {}

void TestLocationBar::ResetTabState(content::WebContents* contents) {}

bool TestLocationBar::HasSecurityStateChanged() {
  return false;
}
