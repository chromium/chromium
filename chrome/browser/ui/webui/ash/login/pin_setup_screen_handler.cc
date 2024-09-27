// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"

#include <string>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

PinSetupScreenHandler::PinSetupScreenHandler() : BaseScreenHandler(kScreenId) {}

PinSetupScreenHandler::~PinSetupScreenHandler() = default;

void PinSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(crbug.com/40139544): clean up constant names
  builder->Add("discoverPinSetup", IDS_DISCOVER_PIN_SETUP);

  builder->Add("discoverPinSetupDone", IDS_DISCOVER_PIN_SETUP_DONE);

  builder->Add("discoverPinSetupTitle1", IDS_DISCOVER_PIN_SETUP_TITLE1);
  builder->Add("discoverPinSetupTitle1ForChild",
               IDS_DISCOVER_PIN_SETUP_TITLE1_CHILD);
  builder->Add("discoverPinSetupSubtitle1", IDS_DISCOVER_PIN_SETUP_SUBTITLE1);
  builder->Add("discoverPinSetupSubtitle1ForChild",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE1_CHILD);
  builder->Add("discoverPinSetupSkip", IDS_DISCOVER_PIN_SETUP_SKIP);
  builder->Add("discoverPinSetupTitle2", IDS_DISCOVER_PIN_SETUP_TITLE2);
  builder->Add("discoverPinSetupTitle2ForChild",
               IDS_DISCOVER_PIN_SETUP_TITLE2_CHILD);
  builder->Add("discoverPinSetupTitle3", IDS_DISCOVER_PIN_SETUP_TITLE3);
  builder->Add("discoverPinSetupTitle3ForChild",
               IDS_DISCOVER_PIN_SETUP_TITLE3_CHILD);
  builder->Add("discoverPinSetupSubtitle3NoLogin",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_NO_LOGIN);
  builder->Add("discoverPinSetupSubtitle3NoLoginForChild",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_NO_LOGIN_CHILD);
  builder->Add("discoverPinSetupSubtitle3WithLogin",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_WITH_LOGIN);
  builder->Add("discoverPinSetupSubtitle3WithLoginForChild",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_WITH_LOGIN_CHILD);
  builder->Add("discoverPinSetupPinAsMainFactorTitle",
               IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_TITLE);
  builder->AddF("discoverPinSetupPinAsMainFactorSubtitle",
                IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_SUBTITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("discoverPinSetupPinAsMainFactorSkip",
               IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_SKIP);

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    builder->Add("pinKeyboard" + base::NumberToString(j),
                 base::FormatNumber(int64_t{j}));
  }
  builder->Add("pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN);
  builder->Add("pinKeyboardPlaceholderPinPassword",
               IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD);
  builder->Add("pinKeyboardDeleteAccessibleName",
               IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME);
  builder->Add("configurePinMismatched",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_MISMATCHED);
  builder->Add("configurePinTooShort",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_SHORT);
  builder->Add("configurePinTooLong",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_LONG);
  builder->Add("configurePinWeakPin",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_WEAK_PIN);
  builder->Add("configurePinNondigit",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_NONDIGIT);
  builder->Add("internalError",
               IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_INTERNAL_ERROR);
}

void PinSetupScreenHandler::Show(const std::string& token,
                                 bool is_child_account,
                                 bool has_login_support,
                                 bool using_pin_as_main_factor) {
  ShowInWebUI(base::Value::Dict()
                  .Set("authToken", base::Value(token))
                  .Set("isChildAccount", is_child_account)
                  .Set("hasLoginSupport", has_login_support)
                  .Set("usingPinAsMainSignInFactor", using_pin_as_main_factor));
}

base::WeakPtr<PinSetupScreenView> PinSetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
