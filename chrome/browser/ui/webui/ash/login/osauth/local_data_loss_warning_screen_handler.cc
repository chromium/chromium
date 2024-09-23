// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

LocalDataLossWarningScreenHandler::LocalDataLossWarningScreenHandler()
    : BaseScreenHandler(kScreenId) {}

LocalDataLossWarningScreenHandler::~LocalDataLossWarningScreenHandler() =
    default;

void LocalDataLossWarningScreenHandler::Show(bool is_owner,
                                             const std::string& email,
                                             bool can_go_back) {
  base::Value::Dict dict;
  dict.Set("isOwner", is_owner);
  dict.Set("email", email);
  dict.Set("canGoBack", can_go_back);
  ShowInWebUI(std::move(dict));
}

base::WeakPtr<LocalDataLossWarningScreenView>
LocalDataLossWarningScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void LocalDataLossWarningScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(b/305201812): Rename string constant.
  builder->Add("continueAnywayButtonLabel",
               IDS_LOGIN_PASSWORD_CHANGED_CONTINUE_AND_DELETE_BUTTON);

  builder->Add("powerwashButtonLabel",
               IDS_LOCAL_DATA_LOSS_WARNING_POWERWASH_BUTTON);
  builder->Add("dataLossWarningTitle",
               IDS_LOGIN_PASSWORD_CHANGED_DATA_LOSS_WARNING_TITLE);
  builder->Add("dataLossWarningSubtitle",
               IDS_LOGIN_PASSWORD_CHANGED_DATA_LOSS_WARNING_SUBTITLE);
  builder->Add("cancelButtonLabel", IDS_LOCK_SCREEN_CANCEL_BUTTON);
}

}  // namespace ash
