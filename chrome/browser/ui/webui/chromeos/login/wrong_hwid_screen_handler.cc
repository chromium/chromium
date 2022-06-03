// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId WrongHWIDScreenView::kScreenId;

WrongHWIDScreenHandler::WrongHWIDScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.WrongHWIDMessageScreen.userActed");
}

WrongHWIDScreenHandler::~WrongHWIDScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void WrongHWIDScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void WrongHWIDScreenHandler::Hide() {
}

void WrongHWIDScreenHandler::Bind(WrongHWIDScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void WrongHWIDScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void WrongHWIDScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("wrongHWIDScreenHeader", IDS_WRONG_HWID_SCREEN_HEADER);
  builder->Add("wrongHWIDMessageFirstPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_FIRST_PART);
  builder->Add("wrongHWIDMessageSecondPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_SECOND_PART);
  builder->Add("wrongHWIDScreenSkipLink",
                IDS_WRONG_HWID_SCREEN_SKIP_LINK);
}

void WrongHWIDScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

}  // namespace chromeos
