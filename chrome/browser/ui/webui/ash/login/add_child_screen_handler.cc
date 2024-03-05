// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

AddChildScreenHandler::AddChildScreenHandler() : BaseScreenHandler(kScreenId) {}

AddChildScreenHandler::~AddChildScreenHandler() = default;

void AddChildScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
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

void AddChildScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<AddChildScreenView> AddChildScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
