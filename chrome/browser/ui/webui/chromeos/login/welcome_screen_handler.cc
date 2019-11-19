// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

constexpr StaticOobeScreenId WelcomeView::kScreenId;

// WelcomeScreenHandler, public: -----------------------------------------------

WelcomeScreenHandler::WelcomeScreenHandler(JSCallsContainer* js_calls_container,
                                           CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
  set_user_acted_method_path("login.WelcomeScreen.userActed");
  DCHECK(core_oobe_view_);
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

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_view_)
      core_oobe_view_->ShowDeviceResetScreen();

    return;
  } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
    if (core_oobe_view_)
      core_oobe_view_->ShowEnableDebuggingScreen();

    return;
  }

  base::DictionaryValue welcome_screen_params;
  welcome_screen_params.SetBoolean(
      "isDeveloperMode", base::CommandLine::ForCurrentProcess()->HasSwitch(
                             chromeos::switches::kSystemDevMode));
  ShowScreenWithData(kScreenId, &welcome_screen_params);
  core_oobe_view_->InitDemoModeDetection();
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

void WelcomeScreenHandler::StopDemoModeDetection() {
  core_oobe_view_->StopDemoModeDetection();
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

// WelcomeScreenHandler, BaseScreenHandler implementation: --------------------

void WelcomeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    builder->Add("welcomeScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
  else
    builder->Add("welcomeScreenGreeting", IDS_WELCOME_SCREEN_GREETING);

  // MD-OOBE (oobe-welcome-md)
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

  builder->Add("a11ySettingToggleOptionOff",
               IDS_A11Y_SETTING_TOGGLE_OPTION_OFF);
  builder->Add("a11ySettingToggleOptionOn", IDS_A11Y_SETTING_TOGGLE_OPTION_ON);
  builder->Add("largeCursorOptionOff", IDS_LARGE_CURSOR_OPTION_OFF);
  builder->Add("largeCursorOptionOn", IDS_LARGE_CURSOR_OPTION_ON);

  builder->Add("timezoneDropdownTitle", IDS_TIMEZONE_DROPDOWN_TITLE);
  builder->Add("timezoneButtonText", IDS_TIMEZONE_BUTTON_TEXT);
}

void WelcomeScreenHandler::DeclareJSCallbacks() {
  AddCallback("WelcomeScreen.setLocaleId",
              &WelcomeScreenHandler::HandleSetLocaleId);
  AddCallback("WelcomeScreen.setInputMethodId",
              &WelcomeScreenHandler::HandleSetInputMethodId);
  AddCallback("WelcomeScreen.setTimezoneId",
              &WelcomeScreenHandler::HandleSetTimezoneId);
}

void WelcomeScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
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

  // GetAdditionalParameters() is called when OOBE language is updated.
  // This happens in two different cases:
  //
  // 1) User selects new locale on OOBE screen. We need to sync active input
  // methods with locale, so EnableLoginLayouts() is needed.
  //
  // 2) This is signin to public session. User has selected some locale & input
  // method on "Public Session User POD". After "Login" button is pressed,
  // new user session is created, locale & input method are changed (both
  // asynchronously).
  // But after public user session is started, "Terms of Service" dialog is
  // shown. It is a part of OOBE UI screens, so it initiates reload of UI
  // strings in new locale. It also happens asynchronously, that leads to race
  // between "locale change", "input method change" and
  // "EnableLoginLayouts()".  This way EnableLoginLayouts() happens after user
  // input method has been changed, resetting input method to hardware default.
  //
  // So we need to disable activation of login layouts if we are already in
  // active user session.
  const bool enable_layouts =
      !user_manager::UserManager::Get()->IsUserLoggedIn();

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
}

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

// WelcomeScreenHandler, private: ----------------------------------------------

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
