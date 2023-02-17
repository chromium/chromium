// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"

#include <type_traits>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/geometry/size.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

CoreOobeHandler::CoreOobeHandler(const std::string& display_type) {
  is_oobe_display_ = display_type == OobeUI::kOobeDisplay;

  TabletMode::Get()->AddObserver(this);

  OobeConfiguration::Get()->AddAndFireObserver(this);

  ChromeKeyboardControllerClient::Get()->AddObserver(this);

  OnKeyboardVisibilityChanged(
      ChromeKeyboardControllerClient::Get()->is_keyboard_visible());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif
  UpdateClientAreaSize(
      display::Screen::GetScreen()->GetPrimaryDisplay().size());

  // Don't show version label on the stable and beta channels by default.
  version_info::Channel channel = chrome::GetChannel();
  if (channel != version_info::Channel::STABLE &&
      channel != version_info::Channel::BETA) {
    ToggleSystemInfo();
  }

  if (ash::system::InputDeviceSettings::Get()
          ->ForceKeyboardDrivenUINavigation())
    CallJS("cr.ui.Oobe.enableKeyboardFlow", true);
}

CoreOobeHandler::~CoreOobeHandler() {
  OobeConfiguration::Get()->RemoveObserver(this);

  // Ash may be released before us.
  if (TabletMode::Get())
    TabletMode::Get()->RemoveObserver(this);

  if (ChromeKeyboardControllerClient::Get())
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
}

void CoreOobeHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("title", IDS_SHORT_PRODUCT_NAME);
  builder->Add("productName", IDS_SHORT_PRODUCT_NAME);
  builder->Add("learnMore", IDS_LEARN_MORE);


  // Strings for Asset Identifier shown in version string.
  builder->Add("assetIdLabel", IDS_OOBE_ASSET_ID_LABEL);

  const bool has_api_keys_configured = google_apis::HasAPIKeyConfigured() &&
                                       google_apis::HasOAuthClientConfigured();
  if (!has_api_keys_configured && is_oobe_display_) {
    builder->AddF("missingAPIKeysNotice", IDS_LOGIN_API_KEYS_NOTICE,
                  base::ASCIIToUTF16(google_apis::kAPIKeysDevelopersHowToURL));
  }

  builder->Add("playAnimationAriaLabel", IDS_OOBE_PLAY_ANIMATION_MESSAGE);
  builder->Add("pauseAnimationAriaLabel", IDS_OOBE_PAUSE_ANIMATION_MESSAGE);

  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
}

void CoreOobeHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  dict->Set("isInTabletMode", TabletMode::Get()->InTabletMode());
  dict->Set("isDemoModeEnabled", DemoSetupController::IsDemoModeAllowed());
  if (policy::EnrollmentRequisitionManager::IsMeetDevice()) {
    // The value is used to show a different UI for this type of the devices.
    dict->Set("deviceFlowType", "meet");
  }
}

void CoreOobeHandler::RegisterMessages() {
  AddCallback("screenStateInitialize", &CoreOobeHandler::HandleInitialized);
  AddCallback("updateCurrentScreen",
              &CoreOobeHandler::HandleUpdateCurrentScreen);
  AddCallback("launchHelpApp", &CoreOobeHandler::HandleLaunchHelpApp);
  AddCallback("raiseTabKeyEvent", &CoreOobeHandler::HandleRaiseTabKeyEvent);

  AddCallback("updateOobeUIState", &CoreOobeHandler::HandleUpdateOobeUIState);
  AddCallback("enableShelfButtons", &CoreOobeHandler::HandleEnableShelfButtons);
}

void CoreOobeHandler::ShowScreenWithData(
    const OobeScreenId& screen,
    absl::optional<base::Value::Dict> data) {
  base::Value::Dict screen_params;
  screen_params.Set("id", screen.name);
  if (data.has_value()) {
    screen_params.Set("data", std::move(data.value()));
  }
  CallJS("cr.ui.Oobe.showScreen", std::move(screen_params));
}

void CoreOobeHandler::ReloadContent(base::Value::Dict dictionary) {
  CallJS("cr.ui.Oobe.reloadContent", std::move(dictionary));
}

void CoreOobeHandler::HandleInitialized() {
  VLOG(3) << "CoreOobeHandler::HandleInitialized";
  GetOobeUI()->InitializeHandlers();
}

void CoreOobeHandler::HandleUpdateCurrentScreen(
    const std::string& screen_name) {
  const OobeScreenId screen(screen_name);
  GetOobeUI()->CurrentScreenChanged(screen);
}

void CoreOobeHandler::HandleEnableShelfButtons(bool enable) {
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->SetShelfButtonsEnabled(enable);
}

void CoreOobeHandler::ForwardCancel() {
  CallJS("cr.ui.Oobe.handleCancel");
}

void CoreOobeHandler::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  UpdateLabel("version", os_version_label_text);
}

void CoreOobeHandler::OnEnterpriseInfoUpdated(const std::string& message_text,
                                              const std::string& asset_id) {
  // Not relevant in OOBE mode.
}

void CoreOobeHandler::OnDeviceInfoUpdated(const std::string& bluetooth_name) {
  CallJS("cr.ui.Oobe.setBluetoothDeviceInfo", bluetooth_name);
}

ui::EventSink* CoreOobeHandler::GetEventSink() {
  return Shell::GetPrimaryRootWindow()->GetHost()->GetEventSink();
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  CallJS("cr.ui.Oobe.setLabelText", id, text);
}

void CoreOobeHandler::OnTabletModeStarted() {
  CallJS("cr.ui.Oobe.setTabletModeState", true);
}

void CoreOobeHandler::OnTabletModeEnded() {
  CallJS("cr.ui.Oobe.setTabletModeState", false);
}

void CoreOobeHandler::UpdateClientAreaSize(const gfx::Size& size) {
  CallJS("cr.ui.Oobe.setShelfHeight", ShelfConfig::Get()->shelf_size());

  const gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  const bool is_horizontal = display_size.width() > display_size.height();
  CallJS("cr.ui.Oobe.setOrientation", is_horizontal);

  const gfx::Size dialog_size = CalculateOobeDialogSize(
      size, ShelfConfig::Get()->shelf_size(), is_horizontal);
  CallJS("cr.ui.Oobe.setDialogSize", dialog_size.width(), dialog_size.height());
}

void CoreOobeHandler::ToggleSystemInfo() {
  CallJS("cr.ui.Oobe.toggleSystemInfo");
}

void CoreOobeHandler::LaunchHelpApp(int help_topic_id) {
  HandleLaunchHelpApp(help_topic_id);
}

void CoreOobeHandler::OnOobeConfigurationChanged() {
  base::Value::Dict configuration = configuration::FilterConfiguration(
      OobeConfiguration::Get()->configuration(),
      configuration::ConfigurationHandlerSide::HANDLER_JS);
  CallJS("cr.ui.Oobe.updateOobeConfiguration", std::move(configuration));
}

void CoreOobeHandler::OnKeyboardVisibilityChanged(bool shown) {
  CallJS("cr.ui.Oobe.setVirtualKeyboardShown", shown);
}

void CoreOobeHandler::HandleLaunchHelpApp(int help_topic_id) {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  help_app_->ShowHelpTopic(
      static_cast<HelpAppLauncher::HelpTopic>(help_topic_id));
}

void CoreOobeHandler::HandleRaiseTabKeyEvent(bool reverse) {
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
  if (reverse)
    event.set_flags(ui::EF_SHIFT_DOWN);
  SendEventToSink(&event);
}

void CoreOobeHandler::HandleUpdateOobeUIState(int state) {
  if (LoginDisplayHost::default_host()) {
    auto dialog_state = static_cast<OobeDialogState>(state);
    LoginDisplayHost::default_host()->UpdateOobeDialogState(dialog_state);
  }
}

}  // namespace ash
