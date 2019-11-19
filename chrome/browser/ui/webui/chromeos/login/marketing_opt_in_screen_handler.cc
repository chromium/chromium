// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId MarketingOptInScreenView::kScreenId;

MarketingOptInScreenHandler::MarketingOptInScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

MarketingOptInScreenHandler::~MarketingOptInScreenHandler() {}

void MarketingOptInScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("marketingOptInScreenTitle",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_TITLE);
  builder->Add("marketingOptInScreenSubtitle",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SUBTITLE);
  builder->Add("marketingOptInGetPlayUpdates",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_GET_PLAY_UPDATES);
  builder->Add("marketingOptInGetChromebookUpdates",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_GET_CHROMEBOOK_UPDATES);
  builder->Add("marketingOptInScreenAllSet",
               IDS_LOGIN_MARKETING_OPT_IN_SCREEN_ALL_SET);
}

void MarketingOptInScreenHandler::Bind(MarketingOptInScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void MarketingOptInScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void MarketingOptInScreenHandler::Hide() {}

void MarketingOptInScreenHandler::Initialize() {}

void MarketingOptInScreenHandler::RegisterMessages() {
  AddCallback("login.MarketingOptInScreen.allSet",
              &MarketingOptInScreenHandler::HandleAllSet);
}

void MarketingOptInScreenHandler::HandleAllSet(
    bool play_communications_opt_in,
    bool tips_communications_opt_in) {
  screen_->OnAllSet(play_communications_opt_in, tips_communications_opt_in);
}

}  // namespace chromeos
