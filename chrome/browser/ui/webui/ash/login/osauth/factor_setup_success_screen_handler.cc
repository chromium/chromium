// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

FactorSetupSuccessScreenHandler::FactorSetupSuccessScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FactorSetupSuccessScreenHandler::~FactorSetupSuccessScreenHandler() = default;

void FactorSetupSuccessScreenHandler::Show(base::Value::Dict params) {
  ShowInWebUI(std::move(params));
}

base::WeakPtr<FactorSetupSuccessScreenView>
FactorSetupSuccessScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FactorSetupSuccessScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  const std::u16string device_name = ui::GetChromeOSDeviceName();

  builder->Add("factorSuccessTitleLocalPasswordUpdated",
               IDS_LOGIN_FACTOR_SETUP_SUCCESS_LOCAL_PASSWORD_UPDATED_TITLE);
  builder->Add("factorSuccessTitleLocalPasswordSet",
               IDS_LOGIN_FACTOR_SETUP_SUCCESS_LOCAL_PASSWORD_SET_TITLE);
  builder->AddF("factorSuccessSubtitleLocalPassword",
                IDS_LOGIN_FACTOR_SETUP_SUCCESS_LOCAL_PASSWORD_SUBTITLE,
                device_name);
  builder->Add("factorSuccessDoneButton", IDS_LOGIN_GENERIC_DONE_BUTTON);
}

}  // namespace ash
