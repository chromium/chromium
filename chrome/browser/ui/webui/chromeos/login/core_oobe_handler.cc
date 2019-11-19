// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"

#include <type_traits>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
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
#include "chromeos/constants/chromeos_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/system_connector.h"
#include "google_apis/google_api_keys.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

bool IsRemoraRequisition() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  return policy_manager && policy_manager->IsRemoraRequisition();
}

void LaunchResetScreen() {
  // Don't recreate WizardController if it already exists.
  WizardController* const wizard_controller =
      WizardController::default_controller();
  if (wizard_controller && !wizard_controller->login_screen_started()) {
    wizard_controller->AdvanceToScreen(ResetView::kScreenId);
  } else {
    DCHECK(LoginDisplayHost::default_host());
    LoginDisplayHost::default_host()->StartWizard(ResetView::kScreenId);
  }
}

}  // namespace

// Note that show_oobe_ui_ defaults to false because WizardController assumes
// OOBE UI is not visible by default.
CoreOobeHandler::CoreOobeHandler(JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), version_info_updater_(this) {
  DCHECK(js_calls_container);
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&CoreOobeHandler::OnAccessibilityStatusChanged,
                 base::Unretained(this)));

  ash::TabletMode::Get()->AddObserver(this);

  // |connector| may be null in tests.
  auto* connector = content::GetSystemConnector();
  if (connector) {
    connector->Connect(ash::mojom::kServiceName,
                       cros_display_config_.BindNewPipeAndPassReceiver());
  }
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

  // OOBE accessibility options menu strings shown on each screen.
  builder->Add("accessibilityLink", IDS_OOBE_ACCESSIBILITY_LINK);
  builder->Add("spokenFeedbackOption", IDS_OOBE_SPOKEN_FEEDBACK_OPTION);
  builder->Add("selectToSpeakOption", IDS_OOBE_SELECT_TO_SPEAK_OPTION);
  builder->Add("largeCursorOption", IDS_OOBE_LARGE_CURSOR_OPTION);
  builder->Add("highContrastOption", IDS_OOBE_HIGH_CONTRAST_MODE_OPTION);
  builder->Add("screenMagnifierOption", IDS_OOBE_SCREEN_MAGNIFIER_OPTION);
  builder->Add("dockedMagnifierOption", IDS_OOBE_DOCKED_MAGNIFIER_OPTION);
  builder->Add("virtualKeyboardOption", IDS_OOBE_VIRTUAL_KEYBOARD_OPTION);
  builder->Add("closeAccessibilityMenu", IDS_OOBE_CLOSE_ACCESSIBILITY_MENU);

  // Strings for the device requisition prompt.
  builder->Add("deviceRequisitionPromptCancel",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_CANCEL);
  builder->Add("deviceRequisitionPromptOk",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_OK);
  builder->Add("deviceRequisitionPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_PROMPT_TEXT);
  builder->Add("deviceRequisitionRemoraPromptCancel",
               IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
  builder->Add("deviceRequisitionRemoraPromptOk",
               IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
  builder->Add("deviceRequisitionRemoraPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_REMORA_PROMPT_TEXT);
  builder->Add("deviceRequisitionSharkPromptText",
               IDS_ENTERPRISE_DEVICE_REQUISITION_SHARK_PROMPT_TEXT);

  // Strings for enable demo mode dialog.
  builder->Add("enableDemoModeDialogTitle", IDS_ENABLE_DEMO_MODE_DIALOG_TITLE);
  builder->Add("enableDemoModeDialogText", IDS_ENABLE_DEMO_MODE_DIALOG_TEXT);
  builder->Add("enableDemoModeDialogConfirm",
               IDS_ENABLE_DEMO_MODE_DIALOG_CONFIRM);
  builder->Add("enableDemoModeDialogCancel",
               IDS_ENABLE_DEMO_MODE_DIALOG_CANCEL);

  // Strings for Asset Identifier shown in version string.
  builder->Add("assetIdLabel", IDS_OOBE_ASSET_ID_LABEL);

  builder->AddF("missingAPIKeysNotice", IDS_LOGIN_API_KEYS_NOTICE,
                base::ASCIIToUTF16(google_apis::kAPIKeysDevelopersHowToURL));
}

void CoreOobeHandler::Initialize() {
  UpdateA11yState();
  UpdateOobeUIVisibility();
#if defined(OFFICIAL_BUILD)
  version_info_updater_.StartUpdate(true);
#else
  version_info_updater_.StartUpdate(false);
#endif
  UpdateDeviceRequisition();
  UpdateKeyboardState();
  UpdateClientAreaSize();
}

void CoreOobeHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetKey("isInTabletMode",
               base::Value(ash::TabletMode::Get()->InTabletMode()));
  dict->SetKey("isDemoModeEnabled",
               base::Value(DemoSetupController::IsDemoModeAllowed()));
  dict->SetKey("showTechnologyBadge",
               base::Value(!ash::features::IsSeparateNetworkIconsEnabled()));
}

void CoreOobeHandler::RegisterMessages() {
  AddCallback("screenStateInitialize", &CoreOobeHandler::HandleInitialized);
  AddCallback("skipUpdateEnrollAfterEula",
              &CoreOobeHandler::HandleSkipUpdateEnrollAfterEula);
  AddCallback("updateCurrentScreen",
              &CoreOobeHandler::HandleUpdateCurrentScreen);
  AddCallback("enableHighContrast", &CoreOobeHandler::HandleEnableHighContrast);
  AddCallback("enableLargeCursor", &CoreOobeHandler::HandleEnableLargeCursor);
  AddCallback("enableVirtualKeyboard",
              &CoreOobeHandler::HandleEnableVirtualKeyboard);
  AddCallback("enableScreenMagnifier",
              &CoreOobeHandler::HandleEnableScreenMagnifier);
  AddCallback("enableSpokenFeedback",
              &CoreOobeHandler::HandleEnableSpokenFeedback);
  AddCallback("enableSelectToSpeak",
              &CoreOobeHandler::HandleEnableSelectToSpeak);
  AddCallback("enableDockedMagnifier",
              &CoreOobeHandler::HandleEnableDockedMagnifier);
  AddCallback("setDeviceRequisition",
              &CoreOobeHandler::HandleSetDeviceRequisition);
  AddRawCallback("skipToLoginForTesting",
                 &CoreOobeHandler::HandleSkipToLoginForTesting);
  AddCallback("skipToUpdateForTesting",
              &CoreOobeHandler::HandleSkipToUpdateForTesting);
  AddCallback("launchHelpApp", &CoreOobeHandler::HandleLaunchHelpApp);
  AddCallback("toggleResetScreen", &CoreOobeHandler::HandleToggleResetScreen);
  AddCallback("toggleEnableDebuggingScreen",
              &CoreOobeHandler::HandleEnableDebuggingScreen);
  AddCallback("raiseTabKeyEvent", &CoreOobeHandler::HandleRaiseTabKeyEvent);
  // Note: Used by enterprise_RemoraRequisitionDisplayUsage.py:
  // TODO(felixe): Use chrome.system.display or cros_display_config.mojom,
  // https://crbug.com/858958.
  AddRawCallback("getPrimaryDisplayNameForTesting",
                 &CoreOobeHandler::HandleGetPrimaryDisplayNameForTesting);
  AddCallback("setupDemoMode", &CoreOobeHandler::HandleSetupDemoMode);
  AddCallback("startDemoModeSetupForTesting",
              &CoreOobeHandler::HandleStartDemoModeSetupForTesting);
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

void CoreOobeHandler::ShowTpmError() {
  CallJS("cr.ui.Oobe.showTpmError");
}

void CoreOobeHandler::ShowDeviceResetScreen() {
  LaunchResetScreen();
}

void CoreOobeHandler::ShowEnableDebuggingScreen() {
  // Don't recreate WizardController if it already exists.
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && !wizard_controller->login_screen_started()) {
    wizard_controller->AdvanceToScreen(EnableDebuggingScreenView::kScreenId);
  }
}

void CoreOobeHandler::ShowEnableAdbSideloadingScreen() {
  // Don't recreate WizardController if it already exists.
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && !wizard_controller->login_screen_started()) {
    wizard_controller->AdvanceToScreen(
        EnableAdbSideloadingScreenView::kScreenId);
  } else {
    DCHECK(LoginDisplayHost::default_host());
    LoginDisplayHost::default_host()->StartWizard(
        EnableAdbSideloadingScreenView::kScreenId);
  }
}

void CoreOobeHandler::ShowActiveDirectoryPasswordChangeScreen(
    const std::string& username) {
  CallJS("cr.ui.Oobe.showActiveDirectoryPasswordChangeScreen", username);
}

void CoreOobeHandler::ShowSignInUI(const std::string& email) {
  CallJS("cr.ui.Oobe.showSigninUI", email);
}

void CoreOobeHandler::ResetSignInUI(bool force_online) {
  CallJS("cr.ui.Oobe.resetSigninUI", force_online);
}

void CoreOobeHandler::ClearUserPodPassword() {
  CallJS("cr.ui.Oobe.clearUserPodPassword");
}

void CoreOobeHandler::RefocusCurrentPod() {
  CallJS("cr.ui.Oobe.refocusCurrentPod");
}

void CoreOobeHandler::ShowPasswordChangedScreen(bool show_password_error,
                                                const std::string& email) {
  CallJS("cr.ui.Oobe.showPasswordChangedScreen", show_password_error, email);
}

void CoreOobeHandler::SetUsageStats(bool checked) {
  CallJS("cr.ui.Oobe.setUsageStats", checked);
}

void CoreOobeHandler::SetTpmPassword(const std::string& tpm_password) {
  CallJS("cr.ui.Oobe.setTpmPassword", tpm_password);
}

void CoreOobeHandler::ClearErrors() {
  CallJS("cr.ui.Oobe.clearErrors");
}

void CoreOobeHandler::ReloadContent(const base::DictionaryValue& dictionary) {
  CallJS("cr.ui.Oobe.reloadContent", dictionary);
}

void CoreOobeHandler::ReloadEulaContent(
    const base::DictionaryValue& dictionary) {
  CallJS("cr.ui.Oobe.reloadEulaContent", dictionary);
}

void CoreOobeHandler::SetVirtualKeyboardShown(bool shown) {
  CallJS("cr.ui.Oobe.setVirtualKeyboardShown", shown);
}

void CoreOobeHandler::SetClientAreaSize(int width, int height) {
  CallJS("cr.ui.Oobe.setClientAreaSize", width, height);
}

void CoreOobeHandler::SetShelfHeight(int height) {
  CallJS("cr.ui.Oobe.setShelfHeight", height);
}

void CoreOobeHandler::HandleInitialized() {
  GetOobeUI()->InitializeHandlers();
  AllowJavascript();
}

void CoreOobeHandler::HandleSkipUpdateEnrollAfterEula() {
  WizardController* controller = WizardController::default_controller();
  DCHECK(controller);
  if (controller)
    controller->SkipUpdateEnrollAfterEula();
}

void CoreOobeHandler::HandleUpdateCurrentScreen(
    const std::string& screen_name) {
  const OobeScreenId screen(screen_name);
  GetOobeUI()->CurrentScreenChanged(screen);
  ash::EventRewriterController::Get()->SetArrowToTabRewritingEnabled(
      screen == EulaView::kScreenId);
}

void CoreOobeHandler::HandleEnableHighContrast(bool enabled) {
  AccessibilityManager::Get()->EnableHighContrast(enabled);
}

void CoreOobeHandler::HandleEnableLargeCursor(bool enabled) {
  AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

void CoreOobeHandler::HandleEnableVirtualKeyboard(bool enabled) {
  AccessibilityManager::Get()->EnableVirtualKeyboard(enabled);
}

void CoreOobeHandler::HandleEnableScreenMagnifier(bool enabled) {
  DCHECK(MagnificationManager::Get());
  MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

void CoreOobeHandler::HandleEnableSpokenFeedback(bool /* enabled */) {
  // Checkbox is initialized on page init and updates when spoken feedback
  // setting is changed so just toggle spoken feedback here.
  AccessibilityManager::Get()->EnableSpokenFeedback(
      !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
}

void CoreOobeHandler::HandleEnableSelectToSpeak(bool /* enabled */) {
  // Checkbox is initialized on page init and updates when Select to Speak
  // setting is changed so just toggle Select to Speak here.
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(
      !AccessibilityManager::Get()->IsSelectToSpeakEnabled());
}

void CoreOobeHandler::HandleEnableDockedMagnifier(bool enabled) {
  // Checkbox is initialized on page init and updates when the docked magnifier
  // setting is changed so just toggle Select to Speak here.
  DCHECK(MagnificationManager::Get());
  MagnificationManager::Get()->SetDockedMagnifierEnabled(enabled);
}

void CoreOobeHandler::HandleSetDeviceRequisition(
    const std::string& requisition) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string initial_requisition =
      connector->GetDeviceCloudPolicyManager()->GetDeviceRequisition();
  connector->GetDeviceCloudPolicyManager()->SetDeviceRequisition(requisition);

  if (IsRemoraRequisition()) {
    // CfM devices default to static timezone.
    g_browser_process->local_state()->SetInteger(
        prefs::kResolveDeviceTimezoneByGeolocationMethod,
        static_cast<int>(chromeos::system::TimeZoneResolverManager::
                             TimeZoneResolveMethod::DISABLED));
  }

  // Exit Chrome to force the restart as soon as a new requisition is set.
  if (initial_requisition !=
      connector->GetDeviceCloudPolicyManager()->GetDeviceRequisition()) {
    chrome::AttemptRestart();
  }
}

void CoreOobeHandler::HandleSkipToLoginForTesting(const base::ListValue* args) {
  LoginScreenContext context;

  // Parse |args|.
  if (args->GetSize() > 0) {
    std::string email;
    if (args->GetString(0, &email))
      context.set_email(email);
  }

  if (WizardController::default_controller())
    WizardController::default_controller()->SkipToLoginForTesting(context);
}

void CoreOobeHandler::HandleSkipToUpdateForTesting() {
  if (WizardController::default_controller())
    WizardController::default_controller()->SkipToUpdateForTesting();
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

void CoreOobeHandler::HandleEnableDebuggingScreen() {
  ShowEnableDebuggingScreen();
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

void CoreOobeHandler::UpdateA11yState() {
  base::DictionaryValue a11y_info;
  a11y_info.SetBoolean("highContrastEnabled",
                       AccessibilityManager::Get()->IsHighContrastEnabled());
  a11y_info.SetBoolean("largeCursorEnabled",
                       AccessibilityManager::Get()->IsLargeCursorEnabled());
  a11y_info.SetBoolean("spokenFeedbackEnabled",
                       AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  a11y_info.SetBoolean("selectToSpeakEnabled",
                       AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  a11y_info.SetBoolean(
      "enableExperimentalA11yFeatures",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableExperimentalAccessibilityFeatures));
  DCHECK(MagnificationManager::Get());
  a11y_info.SetBoolean("screenMagnifierEnabled",
                       MagnificationManager::Get()->IsMagnifierEnabled());
  a11y_info.SetBoolean("dockedMagnifierEnabled",
                       MagnificationManager::Get()->IsDockedMagnifierEnabled());
  a11y_info.SetBoolean("virtualKeyboardEnabled",
                       AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  CallJS("cr.ui.Oobe.refreshA11yInfo", a11y_info);
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
  CallJS("cr.ui.Oobe.setEnterpriseInfo", message_text, asset_id);
}

void CoreOobeHandler::OnDeviceInfoUpdated(const std::string& bluetooth_name) {
  CallJS("cr.ui.Oobe.setBluetoothDeviceInfo", bluetooth_name);
}

ui::EventSink* CoreOobeHandler::GetEventSink() {
  return ash::Shell::GetPrimaryRootWindow()->GetHost()->event_sink();
}

void CoreOobeHandler::UpdateLabel(const std::string& id,
                                  const std::string& text) {
  CallJS("cr.ui.Oobe.setLabelText", id, text);
}

void CoreOobeHandler::UpdateDeviceRequisition() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetDeviceCloudPolicyManager();
  if (policy_manager) {
    CallJS("cr.ui.Oobe.updateDeviceRequisition",
           policy_manager->GetDeviceRequisition());
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

void CoreOobeHandler::UpdateClientAreaSize() {
  const gfx::Size size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  SetClientAreaSize(size.width(), size.height());
  SetShelfHeight(ash::ShelfConfig::Get()->shelf_size());
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
  CallJS("cr.ui.Oobe.setDialogPaddingMode", padding);
}

void CoreOobeHandler::OnOobeConfigurationChanged() {
  base::Value configuration(base::Value::Type::DICTIONARY);
  chromeos::configuration::FilterConfiguration(
      OobeConfiguration::Get()->GetConfiguration(),
      chromeos::configuration::ConfigurationHandlerSide::HANDLER_JS,
      configuration);
  CallJS("cr.ui.Oobe.updateOobeConfiguration", configuration);
}

void CoreOobeHandler::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN)
    accessibility_subscription_.reset();
  else
    UpdateA11yState();
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

void CoreOobeHandler::HandleSetupDemoMode() {
  WizardController* wizard_controller = WizardController::default_controller();
  if (wizard_controller && !wizard_controller->login_screen_started()) {
    wizard_controller->StartDemoModeSetup();
  }
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

void CoreOobeHandler::InitDemoModeDetection() {
  demo_mode_detector_.InitDetection();
}

void CoreOobeHandler::StopDemoModeDetection() {
  demo_mode_detector_.StopDetection();
}

}  // namespace chromeos
