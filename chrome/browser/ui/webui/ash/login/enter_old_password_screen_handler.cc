// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

EnterOldPasswordScreenHandler::EnterOldPasswordScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EnterOldPasswordScreenHandler::~EnterOldPasswordScreenHandler() = default;

void EnterOldPasswordScreenHandler::Show() {
  ShowInWebUI();
}

void EnterOldPasswordScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("nextButtonText", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("oldPasswordHint", IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT);
  builder->Add("oldPasswordIncorrect",
               IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD);
  builder->Add("recoverLocalDataTitle",
               IDS_LOGIN_PASSWORD_CHANGED_RECOVER_DATA_TITLE);
  builder->Add("recoverLocalDataSubtitle",
               IDS_LOGIN_PASSWORD_CHANGED_RECOVER_DATA_SUBTITLE);
  builder->Add("forgotOldPasswordButton",
               IDS_LOGIN_PASSWORD_CHANGED_FORGOT_OLD_PASSWORD_BUTTON);
}

void EnterOldPasswordScreenHandler::ShowWrongPasswordError() {
  CallExternalAPI("showWrongPasswordError");
}

base::WeakPtr<EnterOldPasswordScreenView>
EnterOldPasswordScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
