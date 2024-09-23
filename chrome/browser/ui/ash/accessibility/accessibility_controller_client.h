// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/accessibility_controller_client.h"

// Handles method calls from ash to do accessibility-related work in chrome.
class AccessibilityControllerClient
    : public ash::AccessibilityControllerClient {
 public:
  AccessibilityControllerClient();

  AccessibilityControllerClient(const AccessibilityControllerClient&) = delete;
  AccessibilityControllerClient& operator=(
      const AccessibilityControllerClient&) = delete;

  virtual ~AccessibilityControllerClient();

  // ash::AccessibilityControllerClient:
  void TriggerAccessibilityAlert(ash::AccessibilityAlert alert) override;
  void TriggerAccessibilityAlertWithMessage(
      const std::string& message) override;
  void PlayEarcon(ash::Sound sound_key) override;
  base::TimeDelta PlayShutdownSound() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location) override;
  bool ToggleDictation() override;
  void SilenceSpokenFeedback() override;
  bool ShouldToggleSpokenFeedbackViaTouch() const override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void RequestSelectToSpeakStateChange() override;
  void RequestAutoclickScrollableBoundsForPoint(
      const gfx::Point& point_in_screen) override;
  void MagnifierBoundsChanged(const gfx::Rect& bounds_in_screen) override;
  void OnSwitchAccessDisabled() override;
  void OnSelectToSpeakPanelAction(ash::SelectToSpeakPanelAction action,
                                  double value) override;
  void SetA11yOverrideWindow(aura::Window* a11y_override_window) override;
  std::string GetDictationDefaultLocale(bool new_user) override;
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_
