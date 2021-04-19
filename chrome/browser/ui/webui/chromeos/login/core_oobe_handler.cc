// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include <type_traits>

#include "ash/constants/ash_features.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
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

namespace chromeos {

namespace {

void LaunchResetScreen() {
  DCHECK(LoginDisplayHost::default_host());
  LoginDisplayHost::default_host()->StartWizard(ResetView::kScreenId);
}

}  // namespace

// Note that show_oobe_ui_ defaults to false because WizardController assumes
// OOBE UI is not visible by default.
CoreOobeHandler::CoreOobeHandler(JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), version_info_updater_(this) {
  DCHECK(js_calls_container);

  ash::TabletMode::Get()->AddObserver(this);

  ash::BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
  OobeConfiguration::Get()->AddAndFireObserver(this);
}

CoreOobeHandler::~CoreOobeHandler() {
  OobeConfiguration::Get()->RemoveObserver(this);

  // Ash may be released before us.
  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->RemoveObserver(this);
}

void CoreOobeHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("title", IDS_SHORT_PRODUCT_NAME);
  builder->Add("productName", IDS_SHORT_PRODUCT_NAME);
  builder->Add("learnMore", IDS_LEARN_MORE);


  // Strings for Asset Identifier shown in version string.
  builder->Add("assetIdLabel", IDS_OOBE_ASSET_ID_LABEL);

  builder->AddF("missingAPIKeysNotice", IDS_LOGIN_API_KEYS_NOTICE,
                base::ASCIIToUTF16(google_apis::kAPIKeysDevelopersHowToURL));
}

void CoreOobeHandler::Initialize() {
  UpdateOobeUIVisibility();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif
  UpdateKeyboardState();
  UpdateClientAreaSize(
      display::Screen::GetScreen()->GetPrimaryDisplay().size());
}

void CoreOobeHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetKey("isInTabletMode",
               base::Value(ash::TabletMode::Get()->InTabletMode()));
  dict->SetKey("isDemoModeEnabled",
               base::Value(DemoSetupController::IsDemoModeAllowed()));
  dict->SetKey("newLayoutEnabled",
               base::Value(features::IsNewOobeLayoutEnabled()));
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    dict->SetKey("flowType", base::Value("meet"));
  }
}

void CoreOobeHandler::RegisterMessages() {
  AddCallback("screenStateInitialize", &CoreOobeHandler::HandleInitialized);
  AddCallback("updateCurrentScreen",
              &CoreOobeHandler::HandleUpdateCurrentScreen);
  AddCallback("skipToLoginForTesting",
              &CoreOobeHandler::HandleSkipToLoginForTesting);
  AddCallback("skipToUpdateForTesting",
              &CoreOobeHandler::HandleSkipToUpdateForTesting);
  AddCallback("launchHelpApp", &CoreOobeHandler::HandleLaunchHelpApp);
  AddCallback("toggleResetScreen", &CoreOobeHandler::HandleToggleResetScreen);
  AddCallback("raiseTabKeyEvent", &CoreOobeHandler::HandleRaiseTabKeyEvent);
  // Note: Used by enterprise_RemoraRequisitionDisplayUsage.py:
  // TODO(felixe): Use chrome.system.display or cros_display_config.mojom,
  // https://crbug.com/858958.
  AddRawCallback("getPrimaryDisplayNameForTesting",
                 &CoreOobeHandler::HandleGetPrimaryDisplayNameForTesting);
  AddCallback("startDemoModeSetupForTesting",
              &CoreOobeHandler::HandleStartDemoModeSetupForTesting);

  AddCallback("hideOobeDialog", &CoreOobeHandler::HandleHideOobeDialog);
  AddCallback("updateOobeUIState", &CoreOobeHandler::HandleUpdateOobeUIState);
  AddCallback("enableShelfButtons", &CoreOobeHandler::HandleEnableShelfButtons);
}

void CoreOobeHandler::ShowSignInError(
    int login_attempts,
    const std::string& error_text,
    const std::string& help_link_text,
    HelpAppLauncher::HelpTopic help_topic_id) {
  LOG(ERROR) << "CoreOobeHandler::ShowSignInError: error_text=" << error_text;
  CallJS("cr.ui.Oobe.showSignInError", login_attempts, error_text,
         help_link_text, static_cast<int>(help_topic_id));
}

void CoreOobeHandler::ShowDeviceResetScreen() {
  LaunchResetScreen();
}

void CoreOobeHandler::FocusReturned(bool reverse) {
  CallJS("cr.ui.Oobe.focusReturned", reverse);
}

void CoreOobeHandler::ResetSignInUI(bool force_online) {
  CallJS("cr.ui.Oobe.resetSigninUI", force_online);
}

void CoreOobeHandler::ClearErrors() {
  CallJS("cr.ui.Oobe.clearErrors");
}

void CoreOobeHandler::ReloadContent(const base::DictionaryValue& dictionary) {
  CallJS("cr.ui.Oobe.reloadContent", dictionary);
}

void CoreOobeHandler::ReloadEulaContent(
    const base::DictionaryValue& dictionary) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.reloadEulaContent", dictionary);
  } else {
    LOG(ERROR) << "Silently dropping ReloadEulaContent request.";
  }
}

void CoreOobeHandler::SetVirtualKeyboardShown(bool shown) {
  CallJS("cr.ui.Oobe.setVirtualKeyboardShown", shown);
}

void CoreOobeHandler::SetClientAreaSize(int width, int height) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setClientAreaSize", width, height);
  } else {
    LOG(ERROR) << "Silently dropping SetClientAreaSize request.";
  }
}

void CoreOobeHandler::SetShelfHeight(int height) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setShelfHeight", height);
  } else {
    LOG(ERROR) << "Silently dropping SetShelfHeight request.";
  }
}

void CoreOobeHandler::SetOrientation(bool is_horizontal) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setOrientation", is_horizontal);
  } else {
    LOG(ERROR) << "Silently dropping SetOrientation request.";
  }
}

void CoreOobeHandler::SetDialogSize(int width, int height) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setDialogSize", width, height);
  } else {
    LOG(ERROR) << "Silently dropping SetDialogSize request.";
  }
}

void CoreOobeHandler::HandleInitialized() {
  VLOG(3) << "CoreOobeHandler::HandleInitialized";
  AllowJavascript();
  GetOobeUI()->InitializeHandlers();
}

void CoreOobeHandler::HandleUpdateCurrentScreen(
    const std::string& screen_name) {
  const OobeScreenId screen(screen_name);
  GetOobeUI()->CurrentScreenChanged(screen);
  ash::EventRewriterController::Get()->SetArrowToTabRewritingEnabled(
      screen == EulaView::kScreenId);
}

void CoreOobeHandler::HandleHideOobeDialog() {
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->HideOobeDialog();
}

void CoreOobeHandler::HandleEnableShelfButtons(bool enable) {
  if (LoginDisplayHost::default_host())
    LoginDisplayHost::default_host()->SetShelfButtonsEnabled(enable);
}

void CoreOobeHandler::HandleSkipToLoginForTesting() {
  WizardController* controller = WizardController::default_controller();
  if (controller && controller->is_initialized())
    WizardController::default_controller()->SkipToLoginForTesting();
}

void CoreOobeHandler::HandleSkipToUpdateForTesting() {
  WizardController* controller = WizardController::default_controller();
  if (controller && controller->is_initialized())
    controller->SkipToUpdateForTesting();
}

void CoreOobeHandler::HandleToggleResetScreen() {
  base::OnceCallback<void(bool, base::Optional<tpm_firmware_update::Mode>)>
      callback =
          base::BindOnce(&CoreOobeHandler::HandleToggleResetScreenCallback,
                         weak_ptr_factory_.GetWeakPtr());
  ResetScreen::CheckIfPowerwashAllowed(std::move(callback));
}

void CoreOobeHandler::HandleToggleResetScreenCallback(
    bool is_reset_allowed,
    base::Optional<tpm_firmware_update::Mode> tpm_firmware_update_mode) {
  if (!is_reset_allowed)
    return;
  if (tpm_firmware_update_mode.has_value()) {
    // Force the TPM firmware update option to be enabled.
    g_browser_process->local_state()->SetInteger(
        prefs::kFactoryResetTPMFirmwareUpdateMode,
        static_cast<int>(tpm_firmware_update_mode.value()));
  }
  LaunchResetScreen();
}

void CoreOobeHandler::ShowOobeUI(bool show) {
  if (show == show_oobe_ui_)
    return;

  show_oobe_ui_ = show;

  if (page_is_ready())
    UpdateOobeUIVisibility();
}

void CoreOobeHandler::SetLoginUserCount(int user_count) {
  CallJS("cr.ui.Oobe.setLoginUserCount", user_count);
}

void CoreOobeHandler::ForwardAccelerator(std::string accelerator_name) {
  CallJS("cr.ui.Oobe.handleAccelerator", accelerator_name);
}

void CoreOobeHandler::UpdateOobeUIVisibility() {
  const std::string& display = GetOobeUI()->display_type();
  bool has_api_keys_configured = google_apis::HasAPIKeyConfigured() &&
                                 google_apis::HasOAuthClientConfigured();
  CallJS("cr.ui.Oobe.showAPIKeysNotice",
         !has_api_keys_configured && (display == OobeUI::kOobeDisplay ||
                                      display == OobeUI::kLoginDisplay));

  // Don't show version label on the stable channel by default.
  bool should_show_version = true;
  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::BETA) {
    should_show_version = false;
  }
  CallJS("cr.ui.Oobe.showVersion", should_show_version);
  CallJS("cr.ui.Oobe.showOobeUI", show_oobe_ui_);
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    CallJS("cr.ui.Oobe.enableKeyboardFlow", true);
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
  return ash::Shell::GetPrimaryRootWindow()->GetHost()->GetEventSink();
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setLabelText", id, text);
  } else {
    LOG(ERROR) << "Silently dropping UpdateLabel request.";
  }
}

void CoreOobeHandler::UpdateKeyboardState() {
  const bool is_keyboard_shown =
      ChromeKeyboardControllerClient::Get()->is_keyboard_visible();
  SetVirtualKeyboardShown(is_keyboard_shown);
}

void CoreOobeHandler::OnTabletModeStarted() {
  CallJS("cr.ui.Oobe.setTabletModeState", true);
}

void CoreOobeHandler::OnTabletModeEnded() {
  CallJS("cr.ui.Oobe.setTabletModeState", false);
}

void CoreOobeHandler::UpdateClientAreaSize(const gfx::Size& size) {
  SetClientAreaSize(size.width(), size.height());
  SetShelfHeight(ash::ShelfConfig::Get()->shelf_size());
  if (features::IsNewOobeLayoutEnabled()) {
    const gfx::Size display_size =
        display::Screen::GetScreen()->GetPrimaryDisplay().size();
    const bool is_horizontal = display_size.width() > display_size.height();
    SetOrientation(is_horizontal);
    const gfx::Size dialog_size = CalculateOobeDialogSize(
        size, ash::ShelfConfig::Get()->shelf_size(), is_horizontal);
    SetDialogSize(dialog_size.width(), dialog_size.height());
  }
}

void CoreOobeHandler::SetDialogPaddingMode(
    CoreOobeView::DialogPaddingMode mode) {
  std::string padding;
  switch (mode) {
    case CoreOobeView::DialogPaddingMode::MODE_AUTO:
      padding = "auto";
      break;
    case CoreOobeView::DialogPaddingMode::MODE_NARROW:
      padding = "narrow";
      break;
    case CoreOobeView::DialogPaddingMode::MODE_WIDE:
      padding = "wide";
      break;
    default:
      NOTREACHED();
  }
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.setDialogPaddingMode", padding);
  } else {
    LOG(ERROR) << "Silently dropping SetDialogPaddingMode request.";
  }
}

void CoreOobeHandler::OnOobeConfigurationChanged() {
  base::Value configuration(base::Value::Type::DICTIONARY);
  chromeos::configuration::FilterConfiguration(
      OobeConfiguration::Get()->GetConfiguration(),
      chromeos::configuration::ConfigurationHandlerSide::HANDLER_JS,
      configuration);
  CallJS("cr.ui.Oobe.updateOobeConfiguration", configuration);
}

void CoreOobeHandler::HandleLaunchHelpApp(double help_topic_id) {
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

void CoreOobeHandler::HandleGetPrimaryDisplayNameForTesting(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&CoreOobeHandler::GetPrimaryDisplayNameCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback_id->Clone()));
}

void CoreOobeHandler::GetPrimaryDisplayNameCallback(
    const base::Value& callback_id,
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  AllowJavascript();
  std::string display_name;
  for (const ash::mojom::DisplayUnitInfoPtr& info : info_list) {
    if (info->is_primary) {
      display_name = info->name;
      break;
    }
  }
  DCHECK(!display_name.empty());
  ResolveJavascriptCallback(callback_id, base::Value(display_name));
}

void CoreOobeHandler::HandleStartDemoModeSetupForTesting(
    const std::string& demo_config) {
  DemoSession::DemoModeConfig config;
  if (demo_config == "online") {
    config = DemoSession::DemoModeConfig::kOnline;
  } else if (demo_config == "offline") {
    config = DemoSession::DemoModeConfig::kOffline;
  } else {
    NOTREACHED() << "Unknown demo config passed for tests";
  }

  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && !wizard_controller->login_screen_started()) {
    wizard_controller->SimulateDemoModeSetupForTesting(config);
    wizard_controller->AdvanceToScreen(DemoSetupScreenView::kScreenId);
  }
}

void CoreOobeHandler::HandleUpdateOobeUIState(int state) {
  if (LoginDisplayHost::default_host()) {
    auto dialog_state = static_cast<ash::OobeDialogState>(state);
    LoginDisplayHost::default_host()->UpdateOobeDialogState(dialog_state);
  }
}

}  // namespace chromeos
