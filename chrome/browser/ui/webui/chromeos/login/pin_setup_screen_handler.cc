// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/pin_setup_screen.h"

namespace chromeos {

constexpr StaticOobeScreenId PinSetupScreenView::kScreenId;

PinSetupScreenHandler::PinSetupScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.PinSetupScreen.userActed");
}

PinSetupScreenHandler::~PinSetupScreenHandler() {}

void PinSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void PinSetupScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  discover_ui_.RegisterMessages(web_ui());
}

void PinSetupScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
  discover_ui_.GetAdditionalParameters(dict);
}

void PinSetupScreenHandler::Bind(PinSetupScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void PinSetupScreenHandler::Hide() {}

void PinSetupScreenHandler::Initialize() {
  discover_ui_.Initialize();
}

void PinSetupScreenHandler::Show() {
  ShowScreen(kScreenId);
  discover_ui_.Show();
}

}  // namespace chromeos
