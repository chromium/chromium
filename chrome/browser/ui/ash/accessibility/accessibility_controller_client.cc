// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/dictation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "content/public/browser/tts_controller.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::ash::AccessibilityManager;

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
    case ash::AccessibilityAlert::SAVED_DESKS_MODE_ENTERED:
      msg = ash::saved_desk_util::AreDesksTemplatesEnabled()
                ? IDS_A11Y_ALERT_SAVED_DESKS_LIBRARY_MODE_ENTERED
                : IDS_A11Y_ALERT_SAVED_DESKS_SAVED_FOR_LATER_MODE_ENTERED;
      break;
    case ash::AccessibilityAlert::FASTER_SPLIT_SCREEN_SETUP:
      msg = IDS_A11Y_ALERT_FASTER_SPLITSCREEN_TOAST;
      break;
    case ash::AccessibilityAlert::SNAP_GROUP_RESIZE_LEFT:
      msg = IDS_A11Y_ALERT_SNAP_GROUP_RESIZE_LEFT;
      break;
    case ash::AccessibilityAlert::SNAP_GROUP_RESIZE_RIGHT:
      msg = IDS_A11Y_ALERT_SNAP_GROUP_RESIZE_RIGHT;
      break;
    case ash::AccessibilityAlert::SNAP_GROUP_RESIZE_UP:
      msg = IDS_A11Y_ALERT_SNAP_GROUP_RESIZE_UP;
      break;
    case ash::AccessibilityAlert::SNAP_GROUP_RESIZE_DOWN:
      msg = IDS_A11Y_ALERT_SNAP_GROUP_RESIZE_DOWN;
      break;
    case ash::AccessibilityAlert::SNAP_GROUP_CREATION:
      msg = IDS_A11Y_ALERT_SNAP_GROUP_CREATION;
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

void AccessibilityControllerClient::PlayEarcon(ash::Sound sound_key) {
  AccessibilityManager::Get()->PlayEarcon(
      sound_key, ash::PlaySoundOption::kOnlyIfSpokenFeedbackEnabled);
}

base::TimeDelta AccessibilityControllerClient::PlayShutdownSound() {
  return AccessibilityManager::Get()->PlayShutdownSound();
}

void AccessibilityControllerClient::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  AccessibilityManager::Get()->HandleAccessibilityGesture(gesture, location);
}

bool AccessibilityControllerClient::ToggleDictation() {
  return AccessibilityManager::Get()->ToggleDictation();
}

void AccessibilityControllerClient::SilenceSpokenFeedback() {
  content::TtsController::GetInstance()->Stop();
}

bool AccessibilityControllerClient::ShouldToggleSpokenFeedbackViaTouch() const {
  return AccessibilityManager::Get()->ShouldToggleSpokenFeedbackViaTouch();
}

void AccessibilityControllerClient::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  AccessibilityManager::Get()->PlaySpokenFeedbackToggleCountdown(tick_count);
}

void AccessibilityControllerClient::RequestSelectToSpeakStateChange() {
  AccessibilityManager::Get()->RequestSelectToSpeakStateChange();
}

void AccessibilityControllerClient::RequestAutoclickScrollableBoundsForPoint(
    const gfx::Point& point_in_screen) {
  AccessibilityManager::Get()->RequestAutoclickScrollableBoundsForPoint(
      point_in_screen);
}

void AccessibilityControllerClient::MagnifierBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  AccessibilityManager::Get()->MagnifierBoundsChanged(bounds_in_screen);
}

void AccessibilityControllerClient::OnSwitchAccessDisabled() {
  AccessibilityManager::Get()->OnSwitchAccessDisabled();
}

void AccessibilityControllerClient::OnSelectToSpeakPanelAction(
    ash::SelectToSpeakPanelAction action,
    double value) {
  AccessibilityManager::Get()->OnSelectToSpeakPanelAction(action, value);
}

void AccessibilityControllerClient::SetA11yOverrideWindow(
    aura::Window* a11y_override_window) {
  AutomationManagerAura::GetInstance()->SetA11yOverrideWindow(
      a11y_override_window);
}

std::string AccessibilityControllerClient::GetDictationDefaultLocale(
    bool new_user) {
  return ash::Dictation::DetermineDefaultSupportedLocale(
      ProfileManager::GetActiveUserProfile(), new_user);
}
