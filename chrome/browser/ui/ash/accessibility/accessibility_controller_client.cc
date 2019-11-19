// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/tts_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void SetAutomationManagerEnabled(content::BrowserContext* context,
                                 bool enabled) {
  DCHECK(context);
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  if (enabled)
    manager->Enable();
  else
    manager->Disable();
}

}  // namespace

AccessibilityControllerClient::AccessibilityControllerClient() {
  ash::AccessibilityController::Get()->SetClient(this);
}

AccessibilityControllerClient::~AccessibilityControllerClient() {
  ash::AccessibilityController::Get()->SetClient(nullptr);
}

void AccessibilityControllerClient::TriggerAccessibilityAlert(
    ash::AccessibilityAlert alert) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  int msg = 0;
  switch (alert) {
    case ash::AccessibilityAlert::CAPS_ON:
      msg = IDS_A11Y_ALERT_CAPS_ON;
      break;
    case ash::AccessibilityAlert::CAPS_OFF:
      msg = IDS_A11Y_ALERT_CAPS_OFF;
      break;
    case ash::AccessibilityAlert::SCREEN_ON:
      // Enable automation manager when alert is screen-on, as it is
      // previously disabled by alert screen-off.
      SetAutomationManagerEnabled(profile, true);
      msg = IDS_A11Y_ALERT_SCREEN_ON;
      break;
    case ash::AccessibilityAlert::SCREEN_OFF:
      msg = IDS_A11Y_ALERT_SCREEN_OFF;
      break;
    case ash::AccessibilityAlert::WINDOW_MOVED_TO_ANOTHER_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_ANOTHER_DISPLAY;
      break;
    case ash::AccessibilityAlert::WINDOW_NEEDED:
      msg = IDS_A11Y_ALERT_WINDOW_NEEDED;
      break;
    case ash::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED:
      msg = IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED;
      break;
    case ash::AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED:
      msg = IDS_A11Y_ALERT_WORKSPACE_FULLSCREEN_STATE_ENTERED;
      break;
    case ash::AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED:
      msg = IDS_A11Y_ALERT_WORKSPACE_FULLSCREEN_STATE_EXITED;
      break;
    case ash::AccessibilityAlert::NONE:
      msg = 0;
      break;
  }

  if (msg) {
    AutomationManagerAura::GetInstance()->HandleAlert(
        l10n_util::GetStringUTF8(msg));
    // After handling the alert, if the alert is screen-off, we should
    // disable automation manager to handle any following a11y events.
    if (alert == ash::AccessibilityAlert::SCREEN_OFF)
      SetAutomationManagerEnabled(profile, false);
  }
}

void AccessibilityControllerClient::TriggerAccessibilityAlertWithMessage(
    const std::string& message) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  AutomationManagerAura::GetInstance()->HandleAlert(message);
}

void AccessibilityControllerClient::PlayEarcon(int32_t sound_key) {
  chromeos::AccessibilityManager::Get()->PlayEarcon(
      sound_key, chromeos::PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);
}

base::TimeDelta AccessibilityControllerClient::PlayShutdownSound() {
  return chromeos::AccessibilityManager::Get()->PlayShutdownSound();
}

void AccessibilityControllerClient::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  chromeos::AccessibilityManager::Get()->HandleAccessibilityGesture(gesture);
}

bool AccessibilityControllerClient::ToggleDictation() {
  return chromeos::AccessibilityManager::Get()->ToggleDictation();
}

void AccessibilityControllerClient::SilenceSpokenFeedback() {
  content::TtsController::GetInstance()->Stop();
}

void AccessibilityControllerClient::OnTwoFingerTouchStart() {
  chromeos::AccessibilityManager::Get()->OnTwoFingerTouchStart();
}

void AccessibilityControllerClient::OnTwoFingerTouchStop() {
  chromeos::AccessibilityManager::Get()->OnTwoFingerTouchStop();
}

bool AccessibilityControllerClient::ShouldToggleSpokenFeedbackViaTouch() const {
  return chromeos::AccessibilityManager::Get()
      ->ShouldToggleSpokenFeedbackViaTouch();
}

void AccessibilityControllerClient::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  chromeos::AccessibilityManager::Get()->PlaySpokenFeedbackToggleCountdown(
      tick_count);
}

void AccessibilityControllerClient::RequestSelectToSpeakStateChange() {
  chromeos::AccessibilityManager::Get()->RequestSelectToSpeakStateChange();
}

void AccessibilityControllerClient::RequestAutoclickScrollableBoundsForPoint(
    gfx::Point& point_in_screen) {
  chromeos::AccessibilityManager::Get()
      ->RequestAutoclickScrollableBoundsForPoint(point_in_screen);
}
