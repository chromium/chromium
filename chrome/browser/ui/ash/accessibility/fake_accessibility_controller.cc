// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"

FakeAccessibilityController::FakeAccessibilityController() = default;

FakeAccessibilityController::~FakeAccessibilityController() = default;

void FakeAccessibilityController::SetClient(
    ash::AccessibilityControllerClient* client) {
  was_client_set_ = true;
}

void FakeAccessibilityController::SetDarkenScreen(bool darken) {}

void FakeAccessibilityController::BrailleDisplayStateChanged(bool connected) {}

void FakeAccessibilityController::SetFocusHighlightRect(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetCaretBounds(
    const gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::SetAccessibilityPanelAlwaysVisible(
    bool always_visible) {}

void FakeAccessibilityController::SetAccessibilityPanelBounds(
    const gfx::Rect& bounds,
    ash::AccessibilityPanelState state) {}

void FakeAccessibilityController::SetSelectToSpeakState(
    ash::SelectToSpeakState state) {}

void FakeAccessibilityController::SetSelectToSpeakEventHandlerDelegate(
    ash::SelectToSpeakEventHandlerDelegate* delegate) {}

void FakeAccessibilityController::SetSwitchAccessEventHandlerDelegate(
    ash::SwitchAccessEventHandlerDelegate* delegate) {}

void FakeAccessibilityController::SetDictationActive(bool is_active) {}

void FakeAccessibilityController::ToggleDictationFromSource(
    ash::DictationToggleSource source) {}

void FakeAccessibilityController::OnAutoclickScrollableBoundsFound(
    gfx::Rect& bounds_in_screen) {}

void FakeAccessibilityController::ForwardKeyEventsToSwitchAccess(
    bool should_forward) {}

base::string16 FakeAccessibilityController::GetBatteryDescription() const {
  return base::string16();
}

void FakeAccessibilityController::SetVirtualKeyboardVisible(bool is_visible) {}

void FakeAccessibilityController::NotifyAccessibilityStatusChanged() {}

bool FakeAccessibilityController::IsAccessibilityFeatureVisibleInTrayMenu(
    const std::string& path) {
  return true;
}

void FakeAccessibilityController::
    SetSwitchAccessIgnoreVirtualKeyEventForTesting(bool should_ignore) {}
