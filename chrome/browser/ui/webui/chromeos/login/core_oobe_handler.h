// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/version_info_updater.h"
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event_source.h"

namespace ui {
class EventSink;
}

namespace chromeos {

class CoreOobeView {
 public:
  virtual ~CoreOobeView() = default;

  virtual void ShowScreenWithData(const ash::OobeScreenId& screen,
                                  absl::optional<base::Value::Dict> data) = 0;
  virtual void ReloadContent(base::Value::Dict dictionary) = 0;
  virtual void SetVirtualKeyboardShown(bool shown) = 0;
  virtual void SetShelfHeight(int height) = 0;
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
  CoreOobeHandler();

  CoreOobeHandler(const CoreOobeHandler&) = delete;
  CoreOobeHandler& operator=(const CoreOobeHandler&) = delete;

  ~CoreOobeHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

  // BaseScreenHandler implementation:
  void GetAdditionalParameters(base::Value::Dict* dict) override;

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
  void ShowScreenWithData(const ash::OobeScreenId& screen,
                          absl::optional<base::Value::Dict> data) override;
  void ReloadContent(base::Value::Dict dictionary) override;
  void SetVirtualKeyboardShown(bool displayed) override;
  void SetShelfHeight(int height) override;
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
  void HandleEnableShelfButtons(bool enable);
  void HandleInitialized();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleSkipToLoginForTesting();
  void HandleLaunchHelpApp(int help_topic_id);
  void HandleToggleResetScreen();
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // `demo_config`.
  void HandleStartDemoModeSetupForTesting(const std::string& demo_config);
  void HandleUpdateOobeUIState(int state);

  // Shows the reset screen if `is_reset_allowed` and updates the
  // tpm_firmware_update in settings.
  void HandleToggleResetScreenCallback(
      bool is_reset_allowed,
      absl::optional<tpm_firmware_update::Mode> tpm_firmware_update_mode);

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
  VersionInfoUpdater version_info_updater_{this};

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  base::WeakPtrFactory<CoreOobeHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::CoreOobeView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
