// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"

#include <stddef.h>

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/input_events_blocker.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

// WelcomeScreenHandler, public: -----------------------------------------------

WelcomeScreenHandler::WelcomeScreenHandler() : BaseScreenHandler(kScreenId) {}

WelcomeScreenHandler::~WelcomeScreenHandler() = default;

// WelcomeScreenHandler, WelcomeScreenView implementation: ---------------------

void WelcomeScreenHandler::Show() {
  // TODO(crbug.com/1105387): Part of initial screen logic.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    DCHECK(LoginDisplayHost::default_host());
    LoginDisplayHost::default_host()->StartWizard(ResetView::kScreenId);
    return;
  }

  base::Value::Dict welcome_screen_params;
  welcome_screen_params.Set("isDeveloperMode",
                            base::CommandLine::ForCurrentProcess()->HasSwitch(
                                chromeos::switches::kSystemDevMode));
  ShowInWebUI(std::move(welcome_screen_params));
}

void WelcomeScreenHandler::SetLanguageList(base::Value::List language_list) {
  language_list_ = std::move(language_list);
  GetOobeUI()->GetCoreOobe()->ReloadContent();
}

void WelcomeScreenHandler::SetInputMethodId(
    const std::string& input_method_id) {
  CallExternalAPI("onInputMethodIdSetFromBackend", input_method_id);
}

void WelcomeScreenHandler::ShowDemoModeConfirmationDialog() {
  CallExternalAPI("showDemoModeConfirmationDialog");
}

void WelcomeScreenHandler::ShowEditRequisitionDialog(
    const std::string& requisition) {
  CallExternalAPI("showEditRequisitionDialog", requisition);
}

void WelcomeScreenHandler::ShowRemoraRequisitionDialog() {
  CallExternalAPI("showRemoraRequisitionDialog");
}

// WelcomeScreenHandler, BaseScreenHandler implementation: --------------------

void WelcomeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    builder->Add("welcomeScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
    builder->Add("welcomeScreenGreetingSubtitle", IDS_EMPTY_STRING);
  } else if (switches::IsRevenBranding()) {
    builder->AddF("welcomeScreenGreeting",
                  IDS_WELCOME_SCREEN_GREETING_CLOUD_READY,
                  IDS_INSTALLED_PRODUCT_OS_NAME);
    builder->Add("welcomeScreenGreetingSubtitle",
                 IDS_WELCOME_SCREEN_GREETING_SUBTITLE);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  } else if (features::IsBootAnimationEnabled()) {
    auto product_name =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_CROS_OOBE_PRODUCT_NAME);
    builder->AddF("welcomeScreenGreeting",
                  IDS_WELCOME_SCREEN_GREETING_CLOUD_READY,
                  base::UTF8ToUTF16(product_name));
    builder->Add("welcomeScreenGreetingSubtitle",
                 IDS_WELCOME_SCREEN_GREETING_SUBTITLE);
#endif
  } else {
    builder->AddF("welcomeScreenGreeting", IDS_NEW_WELCOME_SCREEN_GREETING,
                  ui::GetChromeOSDeviceTypeResourceId());
    builder->Add("welcomeScreenGreetingSubtitle",
                 IDS_WELCOME_SCREEN_GREETING_SUBTITLE);
  }

  builder->Add("welcomeScreenGetStarted", IDS_LOGIN_GET_STARTED);

  // MD-OOBE (oobe-welcome-element)
  builder->Add("debuggingFeaturesLink", IDS_WELCOME_ENABLE_DEV_FEATURES_LINK);
  builder->Add("timezoneDropdownLabel", IDS_TIMEZONE_DROPDOWN_LABEL);
  builder->Add("oobeOKButtonText", IDS_OOBE_OK_BUTTON_TEXT);
  builder->Add("languageButtonLabel", IDS_LANGUAGE_BUTTON_LABEL);
  builder->Add("languageSectionTitle", IDS_LANGUAGE_SECTION_TITLE);
  builder->Add("languageSectionHint", IDS_LANGUAGE_SECTION_HINT);
  builder->Add("accessibilitySectionTitle", IDS_ACCESSIBILITY_SECTION_TITLE);
  builder->Add("accessibilitySectionHint", IDS_ACCESSIBILITY_SECTION_HINT);
  builder->Add("timezoneSectionTitle", IDS_TIMEZONE_SECTION_TITLE);
  builder->Add("advancedOptionsSectionTitle",
               IDS_OOBE_ADVANCED_OPTIONS_SCREEN_TITLE);
  builder->Add("advancedOptionsCFMSetupTitle",
               IDS_OOBE_ADVANCED_OPTIONS_CFM_SETUP_TITLE);
  builder->Add("advancedOptionsCFMSetupSubtitle",
               IDS_OOBE_ADVANCED_OPTIONS_CFM_SETUP_SUBTITLE);
  builder->Add("advancedOptionsDeviceRequisitionTitle",
               IDS_OOBE_ADVANCED_OPTIONS_DEVICE_REQUISITION_TITLE);
  builder->Add("advancedOptionsDeviceRequisitionSubtitle",
               IDS_OOBE_ADVANCED_OPTIONS_DEVICE_REQUISITION_SUBTITLE);

  builder->Add("languageDropdownTitle", IDS_LANGUAGE_DROPDOWN_TITLE);
  builder->Add("languageDropdownLabel", IDS_LANGUAGE_DROPDOWN_LABEL);
  builder->Add("keyboardDropdownTitle", IDS_KEYBOARD_DROPDOWN_TITLE);
  builder->Add("keyboardDropdownLabel", IDS_KEYBOARD_DROPDOWN_LABEL);

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

  builder->Add("a11ySettingToggleOptionOff",
               IDS_A11Y_SETTING_TOGGLE_OPTION_OFF);
  builder->Add("a11ySettingToggleOptionOn", IDS_A11Y_SETTING_TOGGLE_OPTION_ON);
  builder->Add("largeCursorOptionOff", IDS_LARGE_CURSOR_OPTION_OFF);
  builder->Add("largeCursorOptionOn", IDS_LARGE_CURSOR_OPTION_ON);

  builder->Add("timezoneDropdownTitle", IDS_TIMEZONE_DROPDOWN_TITLE);
  builder->Add("timezoneButtonText", IDS_TIMEZONE_BUTTON_TEXT);

  // Strings for enable demo mode dialog.
  builder->Add("enableDemoModeDialogTitle", IDS_ENABLE_DEMO_MODE_DIALOG_TITLE);
  builder->Add("enableDemoModeDialogText", IDS_ENABLE_DEMO_MODE_DIALOG_TEXT);
  builder->Add("enableDemoModeDialogConfirm",
               IDS_ENABLE_DEMO_MODE_DIALOG_CONFIRM);
  builder->Add("enableDemoModeDialogCancel",
               IDS_ENABLE_DEMO_MODE_DIALOG_CANCEL);

  // Strings for ChromeVox hint.
  builder->Add("activateChromeVox", IDS_OOBE_ACTIVATE_CHROMEVOX);
  builder->Add("continueWithoutChromeVox", IDS_OOBE_CONTINUE_WITHOUT_CHROMEVOX);
  builder->Add("chromevoxHintClose", IDS_OOBE_CHROMEVOX_HINT_CLOSE);
  builder->Add("chromevoxHintTitle", IDS_OOBE_CHROMEVOX_HINT_TITLE);
  builder->Add("chromeVoxHintText", IDS_OOBE_CHROMEVOX_HINT_TEXT);
  builder->Add("chromeVoxHintTextExpanded",
               IDS_OOBE_CHROMEVOX_HINT_TEXT_EXPANDED);
  builder->Add("chromeVoxHintAnnouncementTextLaptop",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_LAPTOP);
  builder->Add("chromeVoxHintAnnouncementTextTablet",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_TABLET);
  builder->Add("chromeVoxHintAnnouncementTextLaptopExpanded",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_LAPTOP_EXPANDED);
  builder->Add("chromeVoxHintAnnouncementTextTabletExpanded",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_TABLET_EXPANDED);

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

  builder->Add("welcomeScreenQuickStart", IDS_LOGIN_QUICK_START_SETUP);
}

void WelcomeScreenHandler::DeclareJSCallbacks() {
  AddCallback("WelcomeScreen.recordChromeVoxHintSpokenSuccess",
              &WelcomeScreenHandler::HandleRecordChromeVoxHintSpokenSuccess);
}

void WelcomeScreenHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  // GetAdditionalParameters() is called when OOBE language is updated.
  // This happens in two different cases:
  //
  // 1) User selects new locale on OOBE screen. We need to sync active input
  // methods with locale.
  //
  // 2) After user session started and user preferences applied.
  // Either signin to public session: user has selected some locale & input
  // method on "Public Session User pod". After "Login" button is pressed,
  // new user session is created, locale & input method are changed (both
  // asynchronously).
  // Or signin to Gaia account which might trigger language change from the
  // user locale or synced application locale.
  // For the case 2) we might just skip this setup - welcome screen is not
  // needed anymore.

  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }

  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::Get();
  const std::string selected_input_method =
      input_method_manager->GetActiveIMEState()->GetCurrentInputMethod().id();

  base::Value::List language_list = language_list_.Clone();

  if (language_list.empty()) {
    language_list = GetMinimalUILanguageList();
  }

  dict->Set("languageList", std::move(language_list));
  dict->Set("inputMethodsList", GetAndActivateLoginKeyboardLayouts(
                                    application_locale, selected_input_method,
                                    input_method_manager));
  dict->Set("timezoneList", GetTimezoneList());
  dict->Set("demoModeCountryList", DemoSession::GetCountryList());

  // If this switch is set allow to open advanced options and configure device
  // requisition.
  dict->Set("isDeviceRequisitionConfigurable",
            switches::IsDeviceRequisitionConfigurable());
}

// WelcomeScreenHandler, private: ----------------------------------------------

void WelcomeScreenHandler::GiveChromeVoxHint() {
  // Show the ChromeVox hint dialog and give a spoken announcement with
  // instructions for activating ChromeVox.
  CallExternalAPI("maybeGiveChromeVoxHint");
}

void WelcomeScreenHandler::SetQuickStartEnabled() {
  CallExternalAPI("setQuickStartEnabled");
}

base::WeakPtr<WelcomeView> WelcomeScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WelcomeScreenHandler::HandleRecordChromeVoxHintSpokenSuccess() {
  base::UmaHistogramBoolean("OOBE.WelcomeScreen.ChromeVoxHintSpokenSuccess",
                            true);
}

void WelcomeScreenHandler::UpdateA11yState(const A11yState& state) {
  base::Value::Dict a11y_info;
  a11y_info.Set("highContrastEnabled", state.high_contrast);
  a11y_info.Set("largeCursorEnabled", state.large_cursor);
  a11y_info.Set("spokenFeedbackEnabled", state.spoken_feedback);
  a11y_info.Set("selectToSpeakEnabled", state.select_to_speak);
  a11y_info.Set("screenMagnifierEnabled", state.screen_magnifier);
  a11y_info.Set("dockedMagnifierEnabled", state.docked_magnifier);
  a11y_info.Set("virtualKeyboardEnabled", state.virtual_keyboard);
  CallExternalAPI("refreshA11yInfo", std::move(a11y_info));
}

// static
base::Value::List WelcomeScreenHandler::GetTimezoneList() {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);

  base::Value::List timezone_list;
  base::Value::List timezones = ash::system::GetTimezoneList();
  for (const auto& value : timezones) {
    CHECK(value.is_list());
    const base::Value::List& timezone = value.GetList();

    std::string timezone_id = timezone[0].GetString();
    std::string timezone_name = timezone[1].GetString();

    base::Value::Dict timezone_option;
    timezone_option.Set("value", timezone_id);
    timezone_option.Set("title", timezone_name);
    timezone_option.Set("selected", timezone_id == current_timezone_id);
    timezone_list.Append(std::move(timezone_option));
  }

  return timezone_list;
}

}  // namespace ash
