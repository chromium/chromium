// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"

#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

DemoPreferencesScreenView::~DemoPreferencesScreenView() = default;

DemoPreferencesScreenHandler::DemoPreferencesScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DemoPreferencesScreenHandler::~DemoPreferencesScreenHandler() = default;

void DemoPreferencesScreenHandler::Show() {
  ShowInWebUI();
}

void DemoPreferencesScreenHandler::SetInputMethodId(
    const std::string& input_method) {
  CallJS("login.DemoPreferencesScreen.setSelectedKeyboard", input_method);
}

void DemoPreferencesScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("demoPreferencesScreenTitle",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_TITLE);
  builder->Add("demoPreferencesNextButtonLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_NEXT_BUTTON_LABEL);
  builder->Add("countryDropdownTitle", IDS_COUNTRY_DROPDOWN_TITLE);
  builder->Add("countryDropdownLabel", IDS_COUNTRY_DROPDOWN_LABEL);
  builder->Add("retailerIdInputLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_ID_INPUT_LABEL);
  builder->Add("retailerIdInputPlaceholder",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_ID_INPUT_PLACEHOLDER);
  builder->Add("retailerIdInputHelpText",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_ID_INPUT_HELP_TEXT);
  builder->Add("retailerIdInputErrorText",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_ID_INPUT_ERROR_TEXT);
}

}  // namespace chromeos
