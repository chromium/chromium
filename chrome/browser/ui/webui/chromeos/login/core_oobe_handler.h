// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/chromeos/login/oobe_configuration.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_view.h"
#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "ui/events/event_source.h"

namespace base {
class ListValue;
class Value;
}

namespace ui {
class EventSink;
}

namespace chromeos {

class HelpAppLauncher;
class OobeUI;

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseWebUIHandler,
                        public VersionInfoUpdater::Delegate,
                        public CoreOobeView,
                        public ui::EventSource,
                        public TabletModeClientObserver,
                        public OobeConfiguration::Observer {
 public:
  explicit CoreOobeHandler(OobeUI* oobe_ui,
                           JSCallsContainer* js_calls_container);
  ~CoreOobeHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // BaseScreenHandler implementation:
  void GetAdditionalParameters(base::DictionaryValue* dict) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // VersionInfoUpdater::Delegate implementation:
  void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  void OnEnterpriseInfoUpdated(const std::string& message_text,
                               const std::string& asset_id) override;
  void OnDeviceInfoUpdated(const std::string& bluetooth_name) override;

  // ui::EventSource implementation:
  ui::EventSink* GetEventSink() override;

  // Show or hide OOBE UI.
  void ShowOobeUI(bool show);

  bool show_oobe_ui() const {
    return show_oobe_ui_;
  }

  // If |reboot_on_shutdown| is true, the reboot button becomes visible
  // and the shutdown button is hidden. Vice versa if |reboot_on_shutdown| is
  // false.
  void UpdateShutdownAndRebootVisibility(bool reboot_on_shutdown);

  // Notify WebUI of the user count on the views login screen.
  void SetLoginUserCount(int user_count);

  // Forwards an accelerator value to cr.ui.Oobe.handleAccelerator.
  void ForwardAccelerator(std::string accelerator_name);

 private:
  // CoreOobeView implementation:
  void ShowSignInError(int login_attempts,
                       const std::string& error_text,
                       const std::string& help_link_text,
                       HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowTpmError() override;
  void ShowSignInUI(const std::string& email) override;
  void ResetSignInUI(bool force_online) override;
  void ClearUserPodPassword() override;
  void RefocusCurrentPod() override;
  void ShowPasswordChangedScreen(bool show_password_error,
                                 const std::string& email) override;
  void SetUsageStats(bool checked) override;
  void SetTpmPassword(const std::string& tmp_password) override;
  void ClearErrors() override;
  void ReloadContent(const base::DictionaryValue& dictionary) override;
  void ReloadEulaContent(const base::DictionaryValue& dictionary) override;
  void ShowControlBar(bool show) override;
  void SetVirtualKeyboardShown(bool displayed) override;
  void SetClientAreaSize(int width, int height) override;
  void ShowDeviceResetScreen() override;
  void ShowEnableDebuggingScreen() override;
  void ShowActiveDirectoryPasswordChangeScreen(
      const std::string& username) override;

  void InitDemoModeDetection() override;
  void StopDemoModeDetection() override;
  void UpdateKeyboardState() override;

  // TabletModeClientObserver:
  void OnTabletModeToggled(bool enabled) override;

  // OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

  // Handlers for JS WebUI messages.
  void HandleEnableLargeCursor(bool enabled);
  void HandleEnableHighContrast(bool enabled);
  void HandleEnableVirtualKeyboard(bool enabled);
  void HandleEnableScreenMagnifier(bool enabled);
  void HandleEnableSpokenFeedback(bool /* enabled */);
  void HandleEnableSelectToSpeak(bool /* enabled */);
  void HandleEnableDockedMagnifier(bool /* enabled */);
  void HandleInitialized();
  void HandleSkipUpdateEnrollAfterEula();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleSetDeviceRequisition(const std::string& requisition);
  void HandleScreenAssetsLoaded(const std::string& screen_async_load_id);
  void HandleSkipToLoginForTesting(const base::ListValue* args);
  void HandleSkipToUpdateForTesting();
  void HandleLaunchHelpApp(double help_topic_id);
  void HandleToggleResetScreen();
  void HandleEnableDebuggingScreen();
  void HandleHeaderBarVisible();
  void HandleSetOobeBootstrappingSlave();
  void HandleGetPrimaryDisplayNameForTesting(const base::ListValue* args);
  void GetPrimaryDisplayNameCallback(
      const base::Value& callback_id,
      std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);
  void HandleSetupDemoMode();
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // |demo_config|.
  void HandleStartDemoModeSetupForTesting(const std::string& demo_config);

  // When keyboard_utils.js arrow key down event is reached, raise it
  // to tab/shift-tab event.
  void HandleRaiseTabKeyEvent(bool reverse);

  // Updates a11y menu state based on the current a11y features state(on/off).
  void UpdateA11yState();

  // Calls javascript to sync OOBE UI visibility with show_oobe_ui_.
  void UpdateOobeUIVisibility();

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // Updates the device requisition string on the UI side.
  void UpdateDeviceRequisition();

  // Updates client area size based on the primary screen size.
  void UpdateClientAreaSize();

  // Notification of a change in the accessibility settings.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Owner of this handler.
  OobeUI* oobe_ui_ = nullptr;

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_ = false;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DemoModeDetector demo_mode_detector_;

  ash::mojom::CrosDisplayConfigControllerPtr cros_display_config_ptr_;

  base::WeakPtrFactory<CoreOobeHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
