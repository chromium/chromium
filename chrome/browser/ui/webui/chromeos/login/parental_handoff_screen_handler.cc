// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"
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

ParentalHandoffScreenHandler::ParentalHandoffScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.ParentalHandoffScreen.userActed");
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

void ParentalHandoffScreenHandler::InitializeDeprecated() {}

void ParentalHandoffScreenHandler::Show(const std::u16string& username) {
  base::Value::Dict data;
  data.Set(kUsername, username);

  ShowInWebUI(std::move(data));
}

void ParentalHandoffScreenHandler::Bind(ParentalHandoffScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
}

void ParentalHandoffScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

}  // namespace chromeos
