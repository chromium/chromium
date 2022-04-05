// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId DemoPreferencesScreenView::kScreenId;

DemoPreferencesScreenView::~DemoPreferencesScreenView() = default;

DemoPreferencesScreenHandler::DemoPreferencesScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.DemoPreferencesScreen.userActed");
}

DemoPreferencesScreenHandler::~DemoPreferencesScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void DemoPreferencesScreenHandler::Show() {
  ShowInWebUI();
}

void DemoPreferencesScreenHandler::Hide() {}

void DemoPreferencesScreenHandler::Bind(DemoPreferencesScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
}

void DemoPreferencesScreenHandler::SetInputMethodId(
    const std::string& input_method) {
  CallJS("login.DemoPreferencesScreen.setSelectedKeyboard", input_method);
}

void DemoPreferencesScreenHandler::InitializeDeprecated() {}

void DemoPreferencesScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("demoPreferencesScreenTitle",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_TITLE);
  builder->Add("demoPreferencesNextButtonLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_NEXT_BUTTON_LABEL);
  builder->Add("countryDropdownTitle", IDS_COUNTRY_DROPDOWN_TITLE);
  builder->Add("countryDropdownLabel", IDS_COUNTRY_DROPDOWN_LABEL);
}

void DemoPreferencesScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback("DemoPreferencesScreen.setDemoModeCountry",
              &DemoPreferencesScreenHandler::HandleSetDemoModeCountry);
}

void DemoPreferencesScreenHandler::HandleSetDemoModeCountry(
    const std::string& country_id) {
  if (screen_)
    screen_->SetDemoModeCountry(country_id);
}

}  // namespace chromeos
