// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include <memory>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

OmniboxPopupLocationBarHost::OmniboxPopupLocationBarHost(
    LocationBarView& location_bar,
    OmniboxController& controller)
    : controller_(controller) {}

OmniboxPopupLocationBarHost::~OmniboxPopupLocationBarHost() = default;

void OmniboxPopupLocationBarHost::Show() {}

void OmniboxPopupLocationBarHost::Hide() {}

void OmniboxPopupLocationBarHost::RequestFocus() {}

bool OmniboxPopupLocationBarHost::IsShown() const {
  return false;
}

OmniboxPopupWebUIBaseContent* OmniboxPopupLocationBarHost::GetWebUIContent() {
  return nullptr;
}

void OmniboxPopupLocationBarHost::OnOmniboxFocused() {}

void OmniboxPopupLocationBarHost::SetPermissionPromptShowing(bool showing) {}

bool OmniboxPopupLocationBarHost::IsClosePrevented() const {
  return false;
}

bool OmniboxPopupLocationBarHost::CompletedFirstVisuallyNonEmptyPaint() {
  return false;
}

void OmniboxPopupLocationBarHost::DeactivateAndKillFocus() {}

std::optional<OmniboxPopupHost::FocusStateToRestore>
OmniboxPopupLocationBarHost::TakeFocusStateToRestoreAfterModalDialog() {
  return std::nullopt;
}

bool OmniboxPopupLocationBarHost::ShouldReceiveFocus() const {
  return true;
}

std::unique_ptr<OmniboxPopupDeactivationBlocker>
OmniboxPopupLocationBarHost::CreateDeactivationBlocker() {
  return nullptr;
}

OmniboxController* OmniboxPopupLocationBarHost::GetOmniboxController() {
  return &controller_.get();
}
