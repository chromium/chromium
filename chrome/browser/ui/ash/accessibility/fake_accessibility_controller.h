// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_

#include "ash/public/cpp/accessibility_controller.h"

// Fake implementation of ash's mojo AccessibilityController interface.
class FakeAccessibilityController : ash::AccessibilityController {
 public:
  FakeAccessibilityController();

  FakeAccessibilityController(const FakeAccessibilityController&) = delete;
  FakeAccessibilityController& operator=(const FakeAccessibilityController&) =
      delete;

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
  void ShowSelectToSpeakPanel(const gfx::Rect& anchor,
                              bool is_paused,
                              double speed) override;
  void HideSelectToSpeakPanel() override;
  void OnSelectToSpeakPanelAction(ash::SelectToSpeakPanelAction action,
                                  double value) override;
  void HideSwitchAccessBackButton() override;
  void HideSwitchAccessMenu() override;
  void ShowSwitchAccessBackButton(const gfx::Rect& anchor) override;
  void ShowSwitchAccessMenu(const gfx::Rect& anchor,
                            std::vector<std::string> actions) override;
  void StartPointScan() override;
  void StopPointScan() override;
  void SetDictationActive(bool is_active) override;
  void SetPointScanSpeedDipsPerSecond(
      int point_scan_speed_dips_per_second) override;
  void ToggleDictationFromSource(ash::DictationToggleSource source) override;
  void ShowDictationLanguageUpgradedNudge(
      const std::string& dictation_locale,
      const std::string& application_locale) override;
  void HandleAutoclickScrollableBoundsFound(
      gfx::Rect& bounds_in_screen) override;
  std::u16string GetBatteryDescription() const override;
  void SetVirtualKeyboardVisible(bool is_visible) override;
  void PerformAcceleratorAction(
      ash::AcceleratorAction accelerator_action) override;
  void NotifyAccessibilityStatusChanged() override;
  bool IsAccessibilityFeatureVisibleInTrayMenu(
      const std::string& path) override;
  void DisableSwitchAccessDisableConfirmationDialogTesting() override;
  void UpdateDictationButtonOnSpeechRecognitionDownloadChanged(
      int download_progress) override;
  void ShowNotificationForDictation(
      ash::DictationNotificationType type,
      const std::u16string& display_language) override;
  void UpdateDictationBubble(
      bool visible,
      ash::DictationBubbleIconType icon,
      const absl::optional<std::u16string>& text,
      const absl::optional<std::vector<ash::DictationBubbleHintType>>& hints)
      override;
  void SilenceSpokenFeedback() override;

 private:
  bool was_client_set_ = false;
};

#endif  // CHROME_BROWSER_UI_ASH_ACCESSIBILITY_FAKE_ACCESSIBILITY_CONTROLLER_H_
