// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId FamilyLinkNoticeView::kScreenId;

FamilyLinkNoticeScreenHandler::FamilyLinkNoticeScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.FamilyLinkNoticeScreen.userActed");
}

FamilyLinkNoticeScreenHandler::~FamilyLinkNoticeScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void FamilyLinkNoticeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("familyLinkDialogTitle",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_TITLE);
  builder->Add("familyLinkDialogNewGaiaAccountSubtitle",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_NEW_ACCOUNT_SUBTITLE);
  builder->Add("familyLinkDialogExistingGaiaAccountSubtitle",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_EXISTING_ACCOUNT_SUBTITLE);
  builder->Add("familyLinkDialogNotEligibleTitle",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_NOT_ELIGIBLE_TITLE);
  builder->Add("familyLinkDialogNotEligibleSubtitle",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_NOT_ELIGIBLE_SUBTITLE);
  builder->Add("familyLinkContinueButton",
               IDS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_CONTINUE_BUTTON);
}

void FamilyLinkNoticeScreenHandler::Initialize() {}

void FamilyLinkNoticeScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void FamilyLinkNoticeScreenHandler::Bind(FamilyLinkNoticeScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void FamilyLinkNoticeScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void FamilyLinkNoticeScreenHandler::SetIsNewGaiaAccount(bool value) {
  CallJS("login.FamilyLinkNoticeScreen.setIsNewGaiaAccount", value);
}

void FamilyLinkNoticeScreenHandler::SetDisplayEmail(const std::string& value) {
  CallJS("login.FamilyLinkNoticeScreen.setDisplayEmail", value);
}

void FamilyLinkNoticeScreenHandler::SetDomain(const std::string& value) {
  CallJS("login.FamilyLinkNoticeScreen.setDomain", value);
}

}  // namespace chromeos
