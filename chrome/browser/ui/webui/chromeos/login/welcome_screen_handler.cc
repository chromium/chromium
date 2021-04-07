// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/ui/input_events_blocker.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

using ::ash::AccessibilityManager;
using ::ash::MagnificationManager;

constexpr StaticOobeScreenId WelcomeView::kScreenId;

// WelcomeScreenHandler, public: -----------------------------------------------

WelcomeScreenHandler::WelcomeScreenHandler(JSCallsContainer* js_calls_container,
                                           CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
  set_user_acted_method_path("login.WelcomeScreen.userActed");
  DCHECK(core_oobe_view_);

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::BindRepeating(&WelcomeScreenHandler::OnAccessibilityStatusChanged,
                          base::Unretained(this)));
}

WelcomeScreenHandler::~WelcomeScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

// WelcomeScreenHandler, WelcomeScreenView implementation: ---------------------

void WelcomeScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_view_)
      core_oobe_view_->ShowDeviceResetScreen();

    return;
  }

  base::DictionaryValue welcome_screen_params;
  welcome_screen_params.SetBoolean(
      "isDeveloperMode", base::CommandLine::ForCurrentProcess()->HasSwitch(
                             chromeos::switches::kSystemDevMode));
  ShowScreenWithData(kScreenId, &welcome_screen_params);
}

void WelcomeScreenHandler::Hide() {}

void WelcomeScreenHandler::Bind(WelcomeScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void WelcomeScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void WelcomeScreenHandler::ReloadLocalizedContent() {
  base::DictionaryValue localized_strings;
  GetOobeUI()->GetLocalizedStrings(&localized_strings);
  core_oobe_view_->ReloadContent(localized_strings);
}

void WelcomeScreenHandler::SetInputMethodId(
    const std::string& input_method_id) {
  CallJS("login.WelcomeScreen.onInputMethodIdSetFromBackend", input_method_id);
}

void WelcomeScreenHandler::ShowDemoModeConfirmationDialog() {
  CallJS("login.WelcomeScreen.showDemoModeConfirmationDialog");
}

void WelcomeScreenHandler::ShowEditRequisitionDialog(
    const std::string& requisition) {
  CallJS("login.WelcomeScreen.showEditRequisitionDialog", requisition);
}

void WelcomeScreenHandler::ShowRemoraRequisitionDialog() {
  CallJS("login.WelcomeScreen.showRemoraRequisitionDialog");
}

// WelcomeScreenHandler, BaseScreenHandler implementation: --------------------

void WelcomeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    builder->Add("newWelcomeScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
    builder->Add("newWelcomeScreenGreetingSubtitle", IDS_EMPTY_STRING);
    builder->Add("welcomeScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
  } else {
    builder->AddF("newWelcomeScreenGreeting", IDS_NEW_WELCOME_SCREEN_GREETING,
                  ui::GetChromeOSDeviceTypeResourceId());
    builder->Add("newWelcomeScreenGreetingSubtitle",
                 IDS_WELCOME_SCREEN_GREETING_SUBTITLE);
    builder->Add("welcomeScreenGreeting", IDS_WELCOME_SCREEN_GREETING);
  }

  builder->Add("welcomeScreenGetStarted", IDS_LOGIN_GET_STARTED);

  // MD-OOBE (oobe-welcome-element)
  builder->Add("debuggingFeaturesLink", IDS_WELCOME_ENABLE_DEV_FEATURES_LINK);
  builder->Add("timezoneDropdownLabel", IDS_TIMEZONE_DROPDOWN_LABEL);
  builder->Add("oobeOKButtonText", IDS_OOBE_OK_BUTTON_TEXT);
  builder->Add("welcomeNextButtonText", IDS_OOBE_WELCOME_NEXT_BUTTON_TEXT);
  builder->Add("languageButtonLabel", IDS_LANGUAGE_BUTTON_LABEL);
  builder->Add("languageSectionTitle", IDS_LANGUAGE_SECTION_TITLE);
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
  builder->Add("chromeVoxHintText", IDS_OOBE_CHROMEVOX_HINT_TEXT);
  builder->Add("chromeVoxHintAnnouncementTextLaptop",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_LAPTOP);
  builder->Add("chromeVoxHintAnnouncementTextTablet",
               IDS_OOBE_CHROMEVOX_HINT_ANNOUNCEMENT_TEXT_TABLET);

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
}

void WelcomeScreenHandler::DeclareJSCallbacks() {
  AddCallback("WelcomeScreen.setLocaleId",
              &WelcomeScreenHandler::HandleSetLocaleId);
  AddCallback("WelcomeScreen.setInputMethodId",
              &WelcomeScreenHandler::HandleSetInputMethodId);
  AddCallback("WelcomeScreen.setTimezoneId",
              &WelcomeScreenHandler::HandleSetTimezoneId);
  AddCallback("WelcomeScreen.setDeviceRequisition",
              &WelcomeScreenHandler::HandleSetDeviceRequisition);
  AddCallback("WelcomeScreen.recordChromeVoxHintSpokenSuccess",
              &WelcomeScreenHandler::HandleRecordChromeVoxHintSpokenSuccess);
}

void WelcomeScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
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

  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  const std::string selected_input_method =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetCurrentInputMethod()
          .id();

  std::unique_ptr<base::ListValue> language_list;
  if (screen_) {
    if (screen_->language_list() &&
        screen_->language_list_locale() == application_locale) {
      language_list.reset(screen_->language_list()->DeepCopy());
    } else {
      screen_->UpdateLanguageList();
    }
  }

  if (!language_list)
    language_list = GetMinimalUILanguageList();

  const bool enable_layouts = true;

  dict->Set("languageList", std::move(language_list));
  dict->Set("inputMethodsList",
            GetAndActivateLoginKeyboardLayouts(
                application_locale, selected_input_method, enable_layouts));
  dict->Set("timezoneList", GetTimezoneList());
  dict->Set("demoModeCountryList",
            base::Value::ToUniquePtrValue(DemoSession::GetCountryList()));
}

void WelcomeScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }

  // Reload localized strings if they are already resolved.
  if (screen_ && screen_->language_list())
    ReloadLocalizedContent();
  UpdateA11yState();
}

// WelcomeScreenHandler, private: ----------------------------------------------

void WelcomeScreenHandler::HandleSetLocaleId(const std::string& locale_id) {
  if (screen_)
    screen_->SetApplicationLocale(locale_id);
}

void WelcomeScreenHandler::HandleSetInputMethodId(
    const std::string& input_method_id) {
  if (screen_)
    screen_->SetInputMethod(input_method_id);
}

void WelcomeScreenHandler::HandleSetTimezoneId(const std::string& timezone_id) {
  if (screen_)
    screen_->SetTimezone(timezone_id);
}

void WelcomeScreenHandler::HandleSetDeviceRequisition(
    const std::string& requisition) {
  if (screen_)
    screen_->SetDeviceRequisition(requisition);
}

void WelcomeScreenHandler::GiveChromeVoxHint() {
  // Show the ChromeVox hint dialog and give a spoken announcement with
  // instructions for activating ChromeVox.
  CallJS("login.WelcomeScreen.maybeGiveChromeVoxHint");
}

void WelcomeScreenHandler::CancelChromeVoxHintIdleDetection() {
  screen_->CancelChromeVoxHintIdleDetection();
}

void WelcomeScreenHandler::HandleRecordChromeVoxHintSpokenSuccess() {
  base::UmaHistogramBoolean("OOBE.WelcomeScreen.ChromeVoxHintSpokenSuccess",
                            true);
}

void WelcomeScreenHandler::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      ash::AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
  } else {
    UpdateA11yState();
  }
}

void WelcomeScreenHandler::UpdateA11yState() {
  base::DictionaryValue a11y_info;
  a11y_info.SetBoolean("highContrastEnabled",
                       AccessibilityManager::Get()->IsHighContrastEnabled());
  a11y_info.SetBoolean("largeCursorEnabled",
                       AccessibilityManager::Get()->IsLargeCursorEnabled());
  a11y_info.SetBoolean("spokenFeedbackEnabled",
                       AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  a11y_info.SetBoolean("selectToSpeakEnabled",
                       AccessibilityManager::Get()->IsSelectToSpeakEnabled());
  DCHECK(MagnificationManager::Get());
  a11y_info.SetBoolean("screenMagnifierEnabled",
                       MagnificationManager::Get()->IsMagnifierEnabled());
  a11y_info.SetBoolean("dockedMagnifierEnabled",
                       MagnificationManager::Get()->IsDockedMagnifierEnabled());
  a11y_info.SetBoolean("virtualKeyboardEnabled",
                       AccessibilityManager::Get()->IsVirtualKeyboardEnabled());
  if (screen_ && AccessibilityManager::Get()->IsSpokenFeedbackEnabled())
    CancelChromeVoxHintIdleDetection();
  CallJS("login.WelcomeScreen.refreshA11yInfo", a11y_info);
}

// static
std::unique_ptr<base::ListValue> WelcomeScreenHandler::GetTimezoneList() {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);

  std::unique_ptr<base::ListValue> timezone_list(new base::ListValue);
  std::unique_ptr<base::ListValue> timezones = system::GetTimezoneList();
  for (size_t i = 0; i < timezones->GetSize(); ++i) {
    const base::ListValue* timezone = NULL;
    CHECK(timezones->GetList(i, &timezone));

    std::string timezone_id;
    CHECK(timezone->GetString(0, &timezone_id));

    std::string timezone_name;
    CHECK(timezone->GetString(1, &timezone_name));

    std::unique_ptr<base::DictionaryValue> timezone_option(
        new base::DictionaryValue);
    timezone_option->SetString("value", timezone_id);
    timezone_option->SetString("title", timezone_name);
    timezone_option->SetBoolean("selected", timezone_id == current_timezone_id);
    timezone_list->Append(std::move(timezone_option));
  }

  return timezone_list;
}

}  // namespace chromeos
