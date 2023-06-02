// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"

#include <type_traits>
#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "google_apis/google_api_keys.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_sink.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

CoreOobeHandler::CoreOobeHandler() = default;

CoreOobeHandler::~CoreOobeHandler() = default;

void CoreOobeHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("title", IDS_SHORT_PRODUCT_NAME);
  builder->Add("productName", IDS_SHORT_PRODUCT_NAME);
  builder->Add("learnMore", IDS_LEARN_MORE);


  // Strings for Asset Identifier shown in version string.
  builder->Add("assetIdLabel", IDS_OOBE_ASSET_ID_LABEL);

  const bool has_api_keys_configured = google_apis::HasAPIKeyConfigured() &&
                                       google_apis::HasOAuthClientConfigured();
  if (!has_api_keys_configured) {
    builder->AddF("missingAPIKeysNotice", IDS_LOGIN_API_KEYS_NOTICE,
                  base::ASCIIToUTF16(google_apis::kAPIKeysDevelopersHowToURL));
  }

  builder->Add("playAnimationAriaLabel", IDS_OOBE_PLAY_ANIMATION_MESSAGE);
  builder->Add("pauseAnimationAriaLabel", IDS_OOBE_PAUSE_ANIMATION_MESSAGE);

  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
}

void CoreOobeHandler::DeclareJSCallbacks() {
  AddCallback("initializeCoreHandler",
              &CoreOobeHandler::HandleInitializeCoreHandler);
  AddCallback("screenStateInitialize",
              &CoreOobeHandler::HandleScreenStateInitialize);
  AddCallback("priorityScreensLoaded",
              &CoreOobeHandler::HandlePrriorityScreensLoaded);

  AddCallback("updateCurrentScreen",
              &CoreOobeHandler::HandleUpdateCurrentScreen);
  AddCallback("backdropLoaded", &CoreOobeHandler::HandleBackdropLoaded);
  AddCallback("launchHelpApp", &CoreOobeHandler::HandleLaunchHelpApp);
  AddCallback("raiseTabKeyEvent", &CoreOobeHandler::HandleRaiseTabKeyEvent);

  AddCallback("updateOobeUIState", &CoreOobeHandler::HandleUpdateOobeUIState);
  AddCallback("enableShelfButtons", &CoreOobeHandler::HandleEnableShelfButtons);
}

void CoreOobeHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  dict->Set("isInTabletMode", TabletMode::Get()->InTabletMode());
  dict->Set("isDemoModeEnabled", DemoSetupController::IsDemoModeAllowed());
  if (policy::EnrollmentRequisitionManager::IsMeetDevice()) {
    // The value is used to show a different UI for this type of the devices.
    dict->Set("deviceFlowType", "meet");
  }
}

ui::EventSink* CoreOobeHandler::GetEventSink() {
  return Shell::GetPrimaryRootWindow()->GetHost()->GetEventSink();
}

void CoreOobeHandler::ShowScreenWithData(
    const OobeScreenId& screen,
    absl::optional<base::Value::Dict> data) {
  const bool is_safe_priority_call =
      IsPriorityScreen(screen.name) &&
      ui_init_state_ == UiState::kPriorityScreensLoaded;
  CHECK(ui_init_state_ == UiState::kFullyInitialized || is_safe_priority_call);

  base::Value::Dict screen_params;
  screen_params.Set("id", screen.name);
  if (data.has_value()) {
    screen_params.Set("data", std::move(data.value()));
  }

  CallJS("cr.ui.Oobe.showScreen", std::move(screen_params));
}

void CoreOobeHandler::UpdateOobeConfiguration() {
  CHECK(ui_init_state_ == UiState::kFullyInitialized);
  base::Value::Dict configuration = configuration::FilterConfiguration(
      OobeConfiguration::Get()->configuration(),
      configuration::ConfigurationHandlerSide::HANDLER_JS);
  CallJS("cr.ui.Oobe.updateOobeConfiguration", std::move(configuration));
}

void CoreOobeHandler::ReloadContent() {
  CHECK(ui_init_state_ == UiState::kFullyInitialized);
  base::Value::Dict localized_strings = GetOobeUI()->GetLocalizedStrings();
  CallJS("cr.ui.Oobe.reloadContent", std::move(localized_strings));
}

void CoreOobeHandler::ForwardCancel() {
  CHECK(ui_init_state_ == UiState::kFullyInitialized);
  CallJS("cr.ui.Oobe.handleCancel");
}

void CoreOobeHandler::SetTabletModeState(bool tablet_mode_enabled) {
  CHECK(ui_init_state_ == UiState::kFullyInitialized);
  CallJS("cr.ui.Oobe.setTabletModeState", tablet_mode_enabled);
}

void CoreOobeHandler::ToggleSystemInfo() {
  CallJS("cr.ui.Oobe.toggleSystemInfo");
}

void CoreOobeHandler::TriggerDown() {
  CallJS("cr.ui.Oobe.triggerDown");
}

void CoreOobeHandler::EnableKeyboardFlow() {
  CallJS("cr.ui.Oobe.enableKeyboardFlow", true);
}

void CoreOobeHandler::SetShelfHeight(int height) {
  CallJS("cr.ui.Oobe.setShelfHeight", height);
}

void CoreOobeHandler::SetOrientation(bool is_horizontal) {
  CallJS("cr.ui.Oobe.setOrientation", is_horizontal);
}

void CoreOobeHandler::SetDialogSize(int width, int height) {
  CallJS("cr.ui.Oobe.setDialogSize", width, height);
}

void CoreOobeHandler::SetVirtualKeyboardShown(bool shown) {
  CallJS("cr.ui.Oobe.setVirtualKeyboardShown", shown);
}

void CoreOobeHandler::SetOsVersionLabelText(const std::string& label_text) {
  CallJS("cr.ui.Oobe.setLabelText", "version", label_text);
}

void CoreOobeHandler::SetBluetoothDeviceInfo(
    const std::string& bluetooth_name) {
  CallJS("cr.ui.Oobe.setBluetoothDeviceInfo", bluetooth_name);
}

bool CoreOobeHandler::IsPriorityScreen(const std::string& screen_name) {
  // List of screens that are supported for prioritization. Currently, only the
  // Welcome Screen ('connect') is supported.
  const std::vector<std::string> supported_screens{"connect"};

  const auto iter = std::find(supported_screens.begin(),
                              supported_screens.end(), screen_name);
  return iter != supported_screens.end();
}

void CoreOobeHandler::HandleInitializeCoreHandler() {
  VLOG(3) << "CoreOobeHandler::HandleInitializeCoreHandler";
  CHECK(ui_init_state_ == UiState::kUninitialized);
  ui_init_state_ = UiState::kCoreHandlerInitialized;
  AllowJavascript();
  GetOobeUI()->GetCoreOobe()->UpdateUiInitState(
      UiState::kCoreHandlerInitialized);
}

void CoreOobeHandler::HandlePrriorityScreensLoaded() {
  VLOG(3) << "CoreOobeHandler::HandlePrriorityScreensLoaded";
  CHECK(ui_init_state_ == UiState::kCoreHandlerInitialized);
  ui_init_state_ = UiState::kPriorityScreensLoaded;
  GetOobeUI()->GetCoreOobe()->UpdateUiInitState(
      UiState::kPriorityScreensLoaded);
}

void CoreOobeHandler::HandleScreenStateInitialize() {
  VLOG(3) << "CoreOobeHandler::HandleScreenStateInitialize";
  CHECK(ui_init_state_ == UiState::kPriorityScreensLoaded);
  ui_init_state_ = UiState::kFullyInitialized;
  GetOobeUI()->GetCoreOobe()->UpdateUiInitState(UiState::kFullyInitialized);
}

void CoreOobeHandler::HandleEnableShelfButtons(bool enable) {
  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->SetShelfButtonsEnabled(enable);
  }
}

void CoreOobeHandler::HandleUpdateCurrentScreen(
    const std::string& screen_name) {
  const OobeScreenId screen(screen_name);
  GetOobeUI()->CurrentScreenChanged(screen);
}

void CoreOobeHandler::HandleBackdropLoaded() {
  GetOobeUI()->OnBackdropLoaded();
}

void CoreOobeHandler::HandleLaunchHelpApp(int help_topic_id) {
  LoginDisplayHost::default_host()->GetOobeUI()->GetCoreOobe()->LaunchHelpApp(
      help_topic_id);
}

void CoreOobeHandler::HandleUpdateOobeUIState(int state) {
  if (LoginDisplayHost::default_host()) {
    auto dialog_state = static_cast<OobeDialogState>(state);
    LoginDisplayHost::default_host()->UpdateOobeDialogState(dialog_state);
  }
}

void CoreOobeHandler::HandleRaiseTabKeyEvent(bool reverse) {
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::EF_NONE);
  if (reverse) {
    event.set_flags(ui::EF_SHIFT_DOWN);
  }
  SendEventToSink(&event);
}

}  // namespace ash
