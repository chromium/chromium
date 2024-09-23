// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/user_allowlist_check_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

UserAllowlistCheckScreenHandler::UserAllowlistCheckScreenHandler()
    : BaseScreenHandler(kScreenId) {}

UserAllowlistCheckScreenHandler::~UserAllowlistCheckScreenHandler() = default;

void UserAllowlistCheckScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("allowlistErrorEnterpriseAndFamilyLink",
               IDS_ENTERPRISE_AND_FAMILY_LINK_LOGIN_ERROR_ALLOWLIST);
  builder->Add("allowlistErrorEnterprise",
               IDS_ENTERPRISE_LOGIN_ERROR_ALLOWLIST);
  builder->Add("allowlistErrorConsumer", IDS_LOGIN_ERROR_ALLOWLIST);
  builder->Add("tryAgainButton", IDS_ALLOWLIST_ERROR_TRY_AGAIN_BUTTON);
}

void UserAllowlistCheckScreenHandler::DeclareJSCallbacks() {}

void UserAllowlistCheckScreenHandler::Show(bool enterprise_managed,
                                           bool family_link_allowed) {
  base::Value::Dict params;
  params.Set("enterpriseManaged", enterprise_managed);
  params.Set("familyLinkAllowed", family_link_allowed);
  ShowInWebUI(std::move(params));
}

void UserAllowlistCheckScreenHandler::Hide() {}

base::WeakPtr<UserAllowlistCheckScreenView>
UserAllowlistCheckScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
