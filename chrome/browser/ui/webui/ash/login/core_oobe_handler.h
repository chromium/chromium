// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/version_info_updater.h"
#include "chrome/browser/ash/tpm_firmware_update.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event_source.h"

namespace ui {
class EventSink;
}

namespace ash {

class CoreOobeView {
 public:
  virtual ~CoreOobeView() = default;

  virtual void ShowScreenWithData(const OobeScreenId& screen,
                                  absl::optional<base::Value::Dict> data) = 0;
  virtual void ReloadContent(base::Value::Dict dictionary) = 0;
  virtual void UpdateClientAreaSize(const gfx::Size& size) = 0;
  virtual void ToggleSystemInfo() = 0;
  virtual void ForwardCancel() = 0;
  virtual void LaunchHelpApp(int help_topic_id) = 0;
};

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseWebUIHandler,
                        public VersionInfoUpdater::Delegate,
                        public CoreOobeView,
                        public ui::EventSource,
                        public TabletModeObserver,
                        public OobeConfiguration::Observer,
                        public ChromeKeyboardControllerClient::Observer {
 public:
  explicit CoreOobeHandler(const std::string& display_type);

  CoreOobeHandler(const CoreOobeHandler&) = delete;
  CoreOobeHandler& operator=(const CoreOobeHandler&) = delete;

  ~CoreOobeHandler() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

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

 private:
  // CoreOobeView implementation:
  void ShowScreenWithData(const OobeScreenId& screen,
                          absl::optional<base::Value::Dict> data) override;
  void ReloadContent(base::Value::Dict dictionary) override;
  // Updates client area size based on the primary screen size.
  void UpdateClientAreaSize(const gfx::Size& size) override;
  void ToggleSystemInfo() override;
  // Forwards the cancel accelerator value to the shown screen.
  void ForwardCancel() override;
  void LaunchHelpApp(int help_topic_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // OobeConfiguration::Observer:
  void OnOobeConfigurationChanged() override;

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // Handlers for JS WebUI messages.
  void HandleEnableShelfButtons(bool enable);
  void HandleInitialized();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleLaunchHelpApp(int help_topic_id);
  // Handles demo mode setup for tests. Accepts 'online' and 'offline' as
  // `demo_config`.
  void HandleUpdateOobeUIState(int state);

  // When keyboard_utils.js arrow key down event is reached, raise it
  // to tab/shift-tab event.
  void HandleRaiseTabKeyEvent(bool reverse);

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_{this};

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  bool is_oobe_display_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CORE_OOBE_HANDLER_H_
