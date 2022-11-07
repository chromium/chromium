// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

UserCreationScreenHandler::UserCreationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

UserCreationScreenHandler::~UserCreationScreenHandler() = default;

void UserCreationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("userCreationTitle", IDS_OOBE_USER_CREATION_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("userCreationSubtitle", IDS_OOBE_USER_CREATION_SUBTITLE);
  builder->AddF("userCreationAddPersonTitle",
                IDS_OOBE_USER_CREATION_ADD_PERSON_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("userCreationAddPersonSubtitle",
               IDS_OOBE_USER_CREATION_ADD_PERSON_SUBTITLE);
  builder->Add("createForSelfLabel", IDS_OOBE_USER_CREATION_SELF_BUTTON_LABEL);
  builder->Add("createForSelfDescription",
               IDS_OOBE_USER_CREATION_SELF_BUTTON_DESCRIPTION);
  builder->Add("createForChildLabel",
               IDS_OOBE_USER_CREATION_CHILD_BUTTON_LABEL);
  builder->Add("createForChildDescription",
               IDS_OOBE_USER_CREATION_CHILD_BUTTON_DESCRIPTION);
  builder->AddF("childSignInTitle", IDS_OOBE_USER_CREATION_CHILD_SIGNIN_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("childSignInSubtitle",
               IDS_OOBE_USER_CREATION_CHILD_SIGNIN_SUBTITLE);
  builder->Add("createAccountForChildLabel",
               IDS_OOBE_USER_CREATION_CHILD_ACCOUNT_CREATION_BUTTON_LABEL);
  builder->Add("signInForChildLabel",
               IDS_OOBE_USER_CREATION_CHILD_SIGN_IN_BUTTON_LABEL);
  builder->AddF("childSignInParentNotificationText",
                IDS_OOBE_USER_CREATION_CHILD_SIGN_IN_PARENT_NOTIFICATION_TEXT,
                ui::GetChromeOSDeviceName());
  builder->Add("childSignInLearnMore",
               IDS_OOBE_USER_CREATION_CHILD_SIGNIN_LEARN_MORE);
  builder->Add("childSignInLearnMoreDialogTitle",
               IDS_OOBE_USER_CREATION_CHILD_SIGN_IN_LEARN_MORE_DIALOG_TITLE);
  builder->Add("childSignInLearnMoreDialogText",
               IDS_OOBE_USER_CREATION_CHILD_SIGN_IN_LEARN_MORE_DIALOG_TEXT);
}

void UserCreationScreenHandler::Show() {
  ShowInWebUI();
}

void UserCreationScreenHandler::SetIsBackButtonVisible(bool value) {
  CallExternalAPI("setIsBackButtonVisible", value);
}

}  // namespace ash
