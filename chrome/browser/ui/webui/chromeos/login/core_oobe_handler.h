// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/oobe_configuration.h"
#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/event_source.h"

namespace base {
class ListValue;
class Value;
}

namespace ui {
class EventSink;
}

namespace chromeos {

class CoreOobeView {
 public:
  // Enum that specifies how inner padding of OOBE dialog should be calculated.
  enum class DialogPaddingMode {
    // Oobe dialog is displayed full screen, padding will be calculated
    // via css depending on media size.
    MODE_AUTO,
    // Oobe dialog have enough free space around and should use wide padding.
    MODE_WIDE,
    // Oobe dialog is positioned in limited space and should use narrow padding.
    MODE_NARROW,
  };

  virtual ~CoreOobeView() {}

  virtual void ShowSignInError(int login_attempts,
                               const std::string& error_text,
                               const std::string& help_link_text,
                               HelpAppLauncher::HelpTopic help_topic_id) = 0;
  virtual void ShowTpmError() = 0;
  virtual void ShowSignInUI(const std::string& email) = 0;
  virtual void ResetSignInUI(bool force_online) = 0;
  virtual void ClearUserPodPassword() = 0;
  virtual void RefocusCurrentPod() = 0;
  virtual void ShowPasswordChangedScreen(bool show_password_error,
                                         const std::string& email) = 0;
  virtual void SetUsageStats(bool checked) = 0;
  virtual void SetTpmPassword(const std::string& tmp_password) = 0;
  virtual void ClearErrors() = 0;
  virtual void ReloadContent(const base::DictionaryValue& dictionary) = 0;
  virtual void ReloadEulaContent(const base::DictionaryValue& dictionary) = 0;
  virtual void SetVirtualKeyboardShown(bool shown) = 0;
  virtual void SetClientAreaSize(int width, int height) = 0;
  virtual void SetShelfHeight(int height) = 0;
  virtual void SetDialogPaddingMode(DialogPaddingMode mode) = 0;
  virtual void ShowDeviceResetScreen() = 0;
  virtual void ShowEnableAdbSideloadingScreen() = 0;
  virtual void ShowEnableDebuggingScreen() = 0;
  virtual void InitDemoModeDetection() = 0;
  virtual void StopDemoModeDetection() = 0;
  virtual void UpdateKeyboardState() = 0;
  virtual void ShowActiveDirectoryPasswordChangeScreen(
      const std::string& username) = 0;
};

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseWebUIHandler,
                        public VersionInfoUpdater::Delegate,
                        public CoreOobeView,
                        public ui::EventSource,
                        public ash::TabletModeObserver,
                        public OobeConfiguration::Observer {
 public:
  explicit CoreOobeHandler(JSCallsContainer* js_calls_container);
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
  void OnAdbSideloadStatusUpdated(bool enabled) override {}

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
  void SetVirtualKeyboardShown(bool displayed) override;
  void SetClientAreaSize(int width, int height) override;
  void SetShelfHeight(int height) override;
  void SetDialogPaddingMode(CoreOobeView::DialogPaddingMode mode) override;
  void ShowDeviceResetScreen() override;
  void ShowEnableAdbSideloadingScreen() override;
  void ShowEnableDebuggingScreen() override;
  void ShowActiveDirectoryPasswordChangeScreen(
      const std::string& username) override;

  void InitDemoModeDetection() override;
  void StopDemoModeDetection() override;
  void UpdateKeyboardState() override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

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
  void HandleSkipToLoginForTesting(const base::ListValue* args);
  void HandleSkipToUpdateForTesting();
  void HandleLaunchHelpApp(double help_topic_id);
  void HandleToggleResetScreen();
  void HandleEnableDebuggingScreen();
  void HandleSetOobeBootstrappingSlave();
  void HandleGetPrimaryDisplayNameForTesting(const base::ListValue* args);
  void GetPrimaryDisplayNameCallback(
      const base::Value& callback_id,
      std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);
  void HandleSetupDemoMode();
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // |demo_config|.
  void HandleStartDemoModeSetupForTesting(const std::string& demo_config);

  // Shows the reset screen if |is_reset_allowed| and updates the
  // tpm_firmware_update in settings.
  void HandleToggleResetScreenCallback(
      bool is_reset_allowed,
      base::Optional<tpm_firmware_update::Mode> tpm_firmware_update_mode);

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

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_ = false;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DemoModeDetector demo_mode_detector_;

  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;

  base::WeakPtrFactory<CoreOobeHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
