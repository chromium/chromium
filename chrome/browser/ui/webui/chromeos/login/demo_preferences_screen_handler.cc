// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId DemoPreferencesScreenView::kScreenId;

DemoPreferencesScreenView::~DemoPreferencesScreenView() = default;

DemoPreferencesScreenHandler::DemoPreferencesScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.DemoPreferencesScreen.userActed");
}

DemoPreferencesScreenHandler::~DemoPreferencesScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void DemoPreferencesScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void DemoPreferencesScreenHandler::Hide() {}

void DemoPreferencesScreenHandler::Bind(DemoPreferencesScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void DemoPreferencesScreenHandler::SetInputMethodId(
    const std::string& input_method) {
  CallJS("login.DemoPreferencesScreen.setInputMethodIdFromBackend",
         input_method);
}

void DemoPreferencesScreenHandler::Initialize() {}

void DemoPreferencesScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("demoPreferencesScreenTitle",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_TITLE);
  builder->Add("demoPreferencesNextButtonLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_NEXT_BUTTON_LABEL);
  builder->Add("languageDropdownTitle", IDS_LANGUAGE_DROPDOWN_TITLE);
  builder->Add("languageDropdownLabel", IDS_LANGUAGE_DROPDOWN_LABEL);
  builder->Add("keyboardDropdownTitle", IDS_KEYBOARD_DROPDOWN_TITLE);
  builder->Add("keyboardDropdownLabel", IDS_KEYBOARD_DROPDOWN_LABEL);
  builder->Add("countryDropdownTitle", IDS_COUNTRY_DROPDOWN_TITLE);
  builder->Add("countryDropdownLabel", IDS_COUNTRY_DROPDOWN_LABEL);
}

void DemoPreferencesScreenHandler::DeclareJSCallbacks() {
  AddCallback("DemoPreferencesScreen.setLocaleId",
              &DemoPreferencesScreenHandler::HandleSetLocaleId);
  AddCallback("DemoPreferencesScreen.setInputMethodId",
              &DemoPreferencesScreenHandler::HandleSetInputMethodId);
  AddCallback("DemoPreferencesScreen.setDemoModeCountry",
              &DemoPreferencesScreenHandler::HandleSetDemoModeCountry);
}

void DemoPreferencesScreenHandler::HandleSetLocaleId(
    const std::string& language_id) {
  if (screen_)
    screen_->SetLocale(language_id);
}

void DemoPreferencesScreenHandler::HandleSetInputMethodId(
    const std::string& input_method_id) {
  if (screen_)
    screen_->SetInputMethod(input_method_id);
}

void DemoPreferencesScreenHandler::HandleSetDemoModeCountry(
    const std::string& country_id) {
  if (screen_)
    screen_->SetDemoModeCountry(country_id);
}

}  // namespace chromeos
