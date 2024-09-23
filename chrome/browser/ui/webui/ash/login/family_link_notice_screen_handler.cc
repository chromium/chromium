// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

FamilyLinkNoticeScreenHandler::FamilyLinkNoticeScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FamilyLinkNoticeScreenHandler::~FamilyLinkNoticeScreenHandler() = default;

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

void FamilyLinkNoticeScreenHandler::Show() {
  ShowInWebUI();
}

void FamilyLinkNoticeScreenHandler::SetIsNewGaiaAccount(bool value) {
  CallExternalAPI("setIsNewGaiaAccount", value);
}

void FamilyLinkNoticeScreenHandler::SetDisplayEmail(const std::string& value) {
  CallExternalAPI("setDisplayEmail", value);
}

void FamilyLinkNoticeScreenHandler::SetDomain(const std::string& value) {
  CallExternalAPI("setDomain", value);
}

base::WeakPtr<FamilyLinkNoticeView> FamilyLinkNoticeScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
