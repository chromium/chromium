// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_

#include "ash/public/cpp/accessibility_controller.h"
#include "base/macros.h"

// Fake implementation of ash's mojo AccessibilityController interface.
//
// This fake registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class FakeAccessibilityController : ash::AccessibilityController {
 public:
  FakeAccessibilityController();
  ~FakeAccessibilityController() override;

  bool was_client_set() const { return was_client_set_; }

  // ash::AccessibilityController:
  void SetClient(ash::AccessibilityControllerClient* client) override;
  void SetDarkenScreen(bool darken) override;
  void BrailleDisplayStateChanged(bool connected) override;
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override;
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override;
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override;
  void SetAccessibilityPanelBounds(const gfx::Rect& bounds,
                                   ash::AccessibilityPanelState state) override;
  void SetSelectToSpeakState(ash::SelectToSpeakState state) override;
  void SetSelectToSpeakEventHandlerDelegate(
      ash::SelectToSpeakEventHandlerDelegate* delegate) override;
  void SetSwitchAccessEventHandlerDelegate(
      ash::SwitchAccessEventHandlerDelegate* delegate) override;
  void SetDictationActive(bool is_active) override;
  void ToggleDictationFromSource(ash::DictationToggleSource source) override;
  void OnAutoclickScrollableBoundsFound(gfx::Rect& bounds_in_screen) override;
  void ForwardKeyEventsToSwitchAccess(bool should_forward) override;
  base::string16 GetBatteryDescription() const override;
  void SetVirtualKeyboardVisible(bool is_visible) override;
  void NotifyAccessibilityStatusChanged() override;
  bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) override;
  void SetSwitchAccessIgnoreVirtualKeyEventForTesting(
      bool should_ignore) override;

 private:
  bool was_client_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityController);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
