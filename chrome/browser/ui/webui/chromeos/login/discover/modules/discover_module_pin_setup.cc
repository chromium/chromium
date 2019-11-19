// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_pin_setup.h"

#include <vector>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {

class DiscoverModulePinSetupHandler : public DiscoverHandler {
 public:
  DiscoverModulePinSetupHandler(base::WeakPtr<DiscoverModulePinSetup> module,
                                JSCallsContainer* js_calls_container);
  ~DiscoverModulePinSetupHandler() override = default;

  // BaseWebUIHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

 private:
  // Message handlers.
  void HandleGetUserPassword(const std::string& callbackId);
  void HandleGetHasLoginSupport(const std::string& callbackId);

  // quick_unlock::PinBackend::HasLoginSupport callback.
  void OnPinLoginAvailable(const std::string& callbackId, bool is_available);

  base::WeakPtr<DiscoverModulePinSetup> module_;

  base::WeakPtrFactory<DiscoverModulePinSetupHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DiscoverModulePinSetupHandler);
};

DiscoverModulePinSetupHandler::DiscoverModulePinSetupHandler(
    base::WeakPtr<DiscoverModulePinSetup> module,
    JSCallsContainer* js_calls_container)
    : DiscoverHandler(js_calls_container), module_(module) {}

void DiscoverModulePinSetupHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
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
  builder->Add("discoverPinSetupPasswordTitle",
               IDS_DISCOVER_PIN_SETUP_PASSWORD_TITLE);
  builder->Add("discoverPinSetupPasswordSubTitle",
               IDS_DISCOVER_PIN_SETUP_PASSWORD_SUBTITLE);

  builder->Add("passwordPromptInvalidPassword",
               IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_INVALID_PASSWORD);
  builder->Add("passwordPromptPasswordLabel",
               IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_PASSWORD_LABEL);

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

void DiscoverModulePinSetupHandler::Initialize() {}

void DiscoverModulePinSetupHandler::RegisterMessages() {
  AddCallback("discover.pinSetup.getUserPassword",
              &DiscoverModulePinSetupHandler::HandleGetUserPassword);
  AddCallback("discover.pinSetup.getHasLoginSupport",
              &DiscoverModulePinSetupHandler::HandleGetHasLoginSupport);
}

void DiscoverModulePinSetupHandler::HandleGetUserPassword(
    const std::string& callbackId) {
  CallJS("window.discoverReturn", callbackId,
         module_->ConsumePrimaryUserPassword());
  return;
}

void DiscoverModulePinSetupHandler::OnPinLoginAvailable(
    const std::string& callbackId,
    bool is_available) {
  CallJS("window.discoverReturn", callbackId, is_available);
}

void DiscoverModulePinSetupHandler::HandleGetHasLoginSupport(
    const std::string& callbackId) {
  chromeos::quick_unlock::PinBackend::GetInstance()->HasLoginSupport(
      base::BindOnce(&DiscoverModulePinSetupHandler::OnPinLoginAvailable,
                     weak_factory_.GetWeakPtr(), callbackId));
}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover PinSetup module implementation below.               */

const char DiscoverModulePinSetup::kModuleName[] = "pinSetup";

DiscoverModulePinSetup::DiscoverModulePinSetup() {}

DiscoverModulePinSetup::~DiscoverModulePinSetup() = default;

bool DiscoverModulePinSetup::IsCompleted() const {
  return false;
}

std::unique_ptr<DiscoverHandler> DiscoverModulePinSetup::CreateWebUIHandler(
    JSCallsContainer* js_calls_container) {
  return std::make_unique<DiscoverModulePinSetupHandler>(
      weak_ptr_factory_.GetWeakPtr(), js_calls_container);
}

std::string DiscoverModulePinSetup::ConsumePrimaryUserPassword() {
  std::string result;
  std::swap(primary_user_password_, result);
  return result;
}

void DiscoverModulePinSetup::SetPrimaryUserPassword(
    const std::string& password) {
  primary_user_password_ = password;
}

}  // namespace chromeos
