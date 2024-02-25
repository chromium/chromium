// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/osauth/osauth_error_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

OSAuthErrorScreenHandler::OSAuthErrorScreenHandler()
    : BaseScreenHandler(kScreenId) {}

OSAuthErrorScreenHandler::~OSAuthErrorScreenHandler() = default;

void OSAuthErrorScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<OSAuthErrorScreenView> OSAuthErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OSAuthErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("osauthRetryButton", IDS_LOGIN_GENERIC_RETRY_BUTTON);
  builder->Add("osauthErrorTitle", IDS_OOBE_GENERIC_FATAL_ERROR_TITLE);
}

}  // namespace ash
