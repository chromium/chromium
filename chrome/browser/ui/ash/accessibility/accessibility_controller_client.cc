// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
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

AccessibilityControllerClient::AccessibilityControllerClient()
    : binding_(this) {}

AccessibilityControllerClient::~AccessibilityControllerClient() = default;

void AccessibilityControllerClient::Init() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller_);
  BindAndSetClient();
}

void AccessibilityControllerClient::InitForTesting(
    ash::mojom::AccessibilityControllerPtr controller) {
  accessibility_controller_ = std::move(controller);
  BindAndSetClient();
}

void AccessibilityControllerClient::TriggerAccessibilityAlert(
    ash::mojom::AccessibilityAlert alert) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  int msg = 0;
  switch (alert) {
    case ash::mojom::AccessibilityAlert::CAPS_ON:
      msg = IDS_A11Y_ALERT_CAPS_ON;
      break;
    case ash::mojom::AccessibilityAlert::CAPS_OFF:
      msg = IDS_A11Y_ALERT_CAPS_OFF;
      break;
    case ash::mojom::AccessibilityAlert::SCREEN_ON:
      // Enable automation manager when alert is screen-on, as it is
      // previously disabled by alert screen-off.
      SetAutomationManagerEnabled(profile, true);
      msg = IDS_A11Y_ALERT_SCREEN_ON;
      break;
    case ash::mojom::AccessibilityAlert::SCREEN_OFF:
      msg = IDS_A11Y_ALERT_SCREEN_OFF;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_MOVED_TO_ANOTHER_DISPLAY:
      msg = IDS_A11Y_ALERT_WINDOW_MOVED_TO_ANOTHER_DISPLAY;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_NEEDED:
      msg = IDS_A11Y_ALERT_WINDOW_NEEDED;
      break;
    case ash::mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED:
      msg = IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED;
      break;
    case ash::mojom::AccessibilityAlert::NONE:
      msg = 0;
      break;
  }

  if (msg) {
    AutomationManagerAura::GetInstance()->HandleAlert(
        l10n_util::GetStringUTF8(msg));
    // After handling the alert, if the alert is screen-off, we should
    // disable automation manager to handle any following a11y events.
    if (alert == ash::mojom::AccessibilityAlert::SCREEN_OFF)
      SetAutomationManagerEnabled(profile, false);
  }
}

void AccessibilityControllerClient::PlayEarcon(int32_t sound_key) {
  chromeos::AccessibilityManager::Get()->PlayEarcon(
      sound_key, chromeos::PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);
}

void AccessibilityControllerClient::PlayShutdownSound(
    PlayShutdownSoundCallback callback) {
  base::TimeDelta sound_duration =
      chromeos::AccessibilityManager::Get()->PlayShutdownSound();
  std::move(callback).Run(sound_duration);
}

void AccessibilityControllerClient::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  chromeos::AccessibilityManager::Get()->HandleAccessibilityGesture(gesture);
}

void AccessibilityControllerClient::ToggleDictation(
    ToggleDictationCallback callback) {
  bool dictation_active =
      chromeos::AccessibilityManager::Get()->ToggleDictation();
  std::move(callback).Run(dictation_active);
}

void AccessibilityControllerClient::SilenceSpokenFeedback() {
  TtsController::GetInstance()->Stop();
}

void AccessibilityControllerClient::OnTwoFingerTouchStart() {
  chromeos::AccessibilityManager::Get()->OnTwoFingerTouchStart();
}

void AccessibilityControllerClient::OnTwoFingerTouchStop() {
  chromeos::AccessibilityManager::Get()->OnTwoFingerTouchStop();
}

void AccessibilityControllerClient::ShouldToggleSpokenFeedbackViaTouch(
    ShouldToggleSpokenFeedbackViaTouchCallback callback) {
  std::move(callback).Run(chromeos::AccessibilityManager::Get()
                              ->ShouldToggleSpokenFeedbackViaTouch());
}

void AccessibilityControllerClient::PlaySpokenFeedbackToggleCountdown(
    int tick_count) {
  chromeos::AccessibilityManager::Get()->PlaySpokenFeedbackToggleCountdown(
      tick_count);
}

void AccessibilityControllerClient::RequestSelectToSpeakStateChange() {
  chromeos::AccessibilityManager::Get()->RequestSelectToSpeakStateChange();
}

void AccessibilityControllerClient::FlushForTesting() {
  accessibility_controller_.FlushForTesting();
}

void AccessibilityControllerClient::BindAndSetClient() {
  ash::mojom::AccessibilityControllerClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  accessibility_controller_->SetClient(std::move(client));
}
