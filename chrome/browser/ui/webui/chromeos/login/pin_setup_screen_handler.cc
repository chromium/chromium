// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId PinSetupScreenView::kScreenId;

PinSetupScreenHandler::PinSetupScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.PinSetupScreen.userActed");
}

PinSetupScreenHandler::~PinSetupScreenHandler() {}

void PinSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(crbug.com/1104120): clean up constant names
  builder->Add("discoverPinSetup", IDS_DISCOVER_PIN_SETUP);

  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
  builder->Add("discoverPinSetupDone", IDS_DISCOVER_PIN_SETUP_DONE);

  builder->Add("discoverPinSetupTitle1", IDS_DISCOVER_PIN_SETUP_TITLE1);
  builder->Add("discoverPinSetupSubtitle1", IDS_DISCOVER_PIN_SETUP_SUBTITLE1);
  builder->Add("discoverPinSetupSkip", IDS_DISCOVER_PIN_SETUP_SKIP);
  builder->Add("discoverPinSetupTitle2", IDS_DISCOVER_PIN_SETUP_TITLE2);
  builder->Add("discoverPinSetupTitle3", IDS_DISCOVER_PIN_SETUP_TITLE3);
  builder->Add("discoverPinSetupSubtitle3NoLogin",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_NO_LOGIN);
  builder->Add("discoverPinSetupSubtitle3WithLogin",
               IDS_DISCOVER_PIN_SETUP_SUBTITLE3_WITH_LOGIN);

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
}

void PinSetupScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
}

void PinSetupScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {}

void PinSetupScreenHandler::Bind(PinSetupScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void PinSetupScreenHandler::Hide() {}

void PinSetupScreenHandler::Initialize() {
}

void PinSetupScreenHandler::Show(const std::string& token) {
  base::DictionaryValue data;
  data.SetKey("auth_token", base::Value(token));
  ShowScreenWithData(kScreenId, &data);
}

void PinSetupScreenHandler::SetLoginSupportAvailable(bool available) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (!IsSafeToCallJavascript()) {
    LOG(ERROR)
        << "Silently dropping login.PinSetupScreen.setHasLoginSupport request.";
    return;
  }

  CallJS("login.PinSetupScreen.setHasLoginSupport", available);
}

}  // namespace chromeos
