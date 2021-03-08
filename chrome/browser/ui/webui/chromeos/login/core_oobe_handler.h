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
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/version_info_updater.h"
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
  virtual void ResetSignInUI(bool force_online) = 0;
  virtual void ClearErrors() = 0;
  virtual void ReloadContent(const base::DictionaryValue& dictionary) = 0;
  virtual void ReloadEulaContent(const base::DictionaryValue& dictionary) = 0;
  virtual void SetVirtualKeyboardShown(bool shown) = 0;
  virtual void SetClientAreaSize(int width, int height) = 0;
  virtual void SetShelfHeight(int height) = 0;
  virtual void SetDialogPaddingMode(DialogPaddingMode mode) = 0;
  virtual void ShowDeviceResetScreen() = 0;
  virtual void UpdateKeyboardState() = 0;
  virtual void FocusReturned(bool reverse) = 0;
  virtual void SetOrientation(bool is_horizontal) = 0;
  virtual void SetDialogSize(int width, int height) = 0;
  virtual void UpdateClientAreaSize(const gfx::Size& size) = 0;
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

  // If `reboot_on_shutdown` is true, the reboot button becomes visible
  // and the shutdown button is hidden. Vice versa if `reboot_on_shutdown` is
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
  void ResetSignInUI(bool force_online) override;
  void ClearErrors() override;
  void ReloadContent(const base::DictionaryValue& dictionary) override;
  void ReloadEulaContent(const base::DictionaryValue& dictionary) override;
  void SetVirtualKeyboardShown(bool displayed) override;
  void SetClientAreaSize(int width, int height) override;
  void SetShelfHeight(int height) override;
  void SetDialogPaddingMode(CoreOobeView::DialogPaddingMode mode) override;
  void ShowDeviceResetScreen() override;
  void FocusReturned(bool reverse) override;
  void SetOrientation(bool is_horizontal) override;
  void SetDialogSize(int width, int height) override;
  // Updates client area size based on the primary screen size.
  void UpdateClientAreaSize(const gfx::Size& size) override;

  void UpdateKeyboardState() override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

  // Handlers for JS WebUI messages.
  void HandleHideOobeDialog();
  void HandleEnableShelfButtons(bool enable);
  void HandleInitialized();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleSkipToLoginForTesting();
  void HandleSkipToUpdateForTesting();
  void HandleLaunchHelpApp(double help_topic_id);
  void HandleToggleResetScreen();
  void HandleGetPrimaryDisplayNameForTesting(const base::ListValue* args);
  void GetPrimaryDisplayNameCallback(
      const base::Value& callback_id,
      std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // `demo_config`.
  void HandleStartDemoModeSetupForTesting(const std::string& demo_config);
  void HandleUpdateOobeUIState(int state);

  // Shows the reset screen if `is_reset_allowed` and updates the
  // tpm_firmware_update in settings.
  void HandleToggleResetScreenCallback(
      bool is_reset_allowed,
      base::Optional<tpm_firmware_update::Mode> tpm_firmware_update_mode);

  // When keyboard_utils.js arrow key down event is reached, raise it
  // to tab/shift-tab event.
  void HandleRaiseTabKeyEvent(bool reverse);

  // Calls javascript to sync OOBE UI visibility with show_oobe_ui_.
  void UpdateOobeUIVisibility();

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_ = false;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;

  base::WeakPtrFactory<CoreOobeHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
