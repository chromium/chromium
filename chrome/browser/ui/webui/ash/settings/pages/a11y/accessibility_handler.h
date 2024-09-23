// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/soda/soda_installer.h"

class Profile;

namespace ash::settings {

class AccessibilityHandler : public ::settings::SettingsPageUIHandler,
                             public speech::SodaInstaller::Observer {
 public:
  explicit AccessibilityHandler(Profile* profile);

  AccessibilityHandler(const AccessibilityHandler&) = delete;
  AccessibilityHandler& operator=(const AccessibilityHandler&) = delete;

  ~AccessibilityHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback which updates if startup sound is enabled. Visible for testing.
  void HandleManageA11yPageReady(const base::Value::List& args);

 private:
  friend class AccessibilityHandlerTest;

  void HandleRecordSelectedShowShelfNavigationButtonsValue(
      const base::Value::List& args);
  void HandleShowBrowserAppearanceSettings(const base::Value::List& args);
  void HandleShowChromeVoxTutorial(const base::Value::List& args);
  void HandleSetStartupSoundEnabled(const base::Value::List& args);
  void HandleUpdateBluetoothBrailleDisplayAddress(
      const base::Value::List& args);
  void HandleGetStartupSoundEnabled(const base::Value::List& args);
  void HandlePreviewFlashNotification(const base::Value::List& args);

  void OpenExtensionOptionsPage(const char extension_id[]);

  void MaybeAddSodaInstallerObserver();

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;

  void MaybeAddDictationLocales();
  speech::LanguageCode GetDictationLocale();
  std::u16string GetDictationLocaleDisplayName();

  raw_ptr<Profile> profile_;  // Weak pointer.

  // Timer to record user changed value for the accessibility setting to turn
  // shelf navigation buttons on in tablet mode. The metric is recorded with 10
  // second delay to avoid overreporting when the user keeps toggling the
  // setting value in the screen UI.
  base::OneShotTimer a11y_nav_buttons_toggle_metrics_reporter_timer_;

  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_observation_{this};

  base::WeakPtrFactory<AccessibilityHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_HANDLER_H_
