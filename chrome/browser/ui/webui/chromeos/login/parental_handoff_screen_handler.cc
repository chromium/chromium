// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kUsername[] = "username";

}  // namespace

// static
constexpr StaticOobeScreenId ParentalHandoffScreenView::kScreenId;

ParentalHandoffScreenHandler::ParentalHandoffScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.ParentalHandoffScreen.userActed");
}

ParentalHandoffScreenHandler::~ParentalHandoffScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ParentalHandoffScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("parentalHandoffDialogNextButton",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_NEXT_BUTTON);
  builder->Add("parentalHandoffDialogTitle",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_TITLE);
  builder->Add("parentalHandoffDialogSubtitle",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_SUBTITLE);
}

void ParentalHandoffScreenHandler::Initialize() {}

void ParentalHandoffScreenHandler::Show(const std::u16string& username) {
  base::DictionaryValue data;
  data.SetString(kUsername, username);

  ShowScreenWithData(kScreenId, &data);
}

void ParentalHandoffScreenHandler::Bind(ParentalHandoffScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ParentalHandoffScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

}  // namespace chromeos
