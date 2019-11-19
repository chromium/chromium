// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/discover_screen.h"

namespace chromeos {

constexpr StaticOobeScreenId DiscoverScreenView::kScreenId;

DiscoverScreenHandler::DiscoverScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.DiscoverScreen.userActed");
}

DiscoverScreenHandler::~DiscoverScreenHandler() {}

void DiscoverScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void DiscoverScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  discover_ui_.RegisterMessages(web_ui());
}

void DiscoverScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
  discover_ui_.GetAdditionalParameters(dict);
}

void DiscoverScreenHandler::Bind(DiscoverScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void DiscoverScreenHandler::Hide() {}

void DiscoverScreenHandler::Initialize() {
  discover_ui_.Initialize();
}

void DiscoverScreenHandler::Show() {
  ShowScreen(kScreenId);
  discover_ui_.Show();
}

}  // namespace chromeos
