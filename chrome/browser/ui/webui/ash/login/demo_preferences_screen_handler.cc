// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"

#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DemoPreferencesScreenView::~DemoPreferencesScreenView() = default;

DemoPreferencesScreenHandler::DemoPreferencesScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DemoPreferencesScreenHandler::~DemoPreferencesScreenHandler() = default;

void DemoPreferencesScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<DemoPreferencesScreenView>
DemoPreferencesScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DemoPreferencesScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("demoPreferencesScreenTitle",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_TITLE);
  builder->Add("demoPreferencesNextButtonLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_NEXT_BUTTON_LABEL);
  builder->Add("countryDropdownTitle", IDS_COUNTRY_DROPDOWN_TITLE);
  builder->Add("countryDropdownLabel", IDS_COUNTRY_DROPDOWN_LABEL);
  builder->Add("retailerNameInputLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_NAME_INPUT_LABEL);
  builder->Add("retailerNameInputPlaceholder",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_NAME_INPUT_PLACEHOLDER);
  builder->Add("retailerPrivacyDisclaimer",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_RETAILER_PRIVACY_DISCLAIMER);
  builder->Add("storeNumberInputLabel",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_STORE_NUMBER_INPUT_LABEL);
  builder->Add("storeNumberInputPlaceholder",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_STORE_NUMBER_INPUT_PLACEHOLDER);
  builder->Add("storeNumberInputHelpText",
               IDS_OOBE_DEMO_SETUP_PREFERENCES_STORE_NUMBER_INPUT_HELP_TEXT);
}

}  // namespace ash
