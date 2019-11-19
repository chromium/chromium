// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_

#include "ash/public/cpp/accessibility_controller_client.h"
#include "base/macros.h"

// Handles method calls from ash to do accessibility-related work in chrome.
class AccessibilityControllerClient
    : public ash::AccessibilityControllerClient {
 public:
  AccessibilityControllerClient();
  virtual ~AccessibilityControllerClient();

  // ash::AccessibilityControllerClient:
  void TriggerAccessibilityAlert(ash::AccessibilityAlert alert) override;
  void TriggerAccessibilityAlertWithMessage(
      const std::string& message) override;
  void PlayEarcon(int sound_key) override;
  base::TimeDelta PlayShutdownSound() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override;
  bool ToggleDictation() override;
  void SilenceSpokenFeedback() override;
  void OnTwoFingerTouchStart() override;
  void OnTwoFingerTouchStop() override;
  bool ShouldToggleSpokenFeedbackViaTouch() const override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void RequestSelectToSpeakStateChange() override;
  void RequestAutoclickScrollableBoundsForPoint(
      gfx::Point& point_in_screen) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_ACCESSIBILITY_CONTROLLER_CLIENT_H_
