// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

LocalStateErrorScreenHandler::LocalStateErrorScreenHandler()
    : BaseScreenHandler(kScreenId) {}

LocalStateErrorScreenHandler::~LocalStateErrorScreenHandler() = default;

void LocalStateErrorScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<LocalStateErrorScreenView>
LocalStateErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void LocalStateErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("localStateErrorTitle", IDS_RESET_SCREEN_REVERT_ERROR);
  builder->AddF("localStateErrorText0", IDS_LOCAL_STATE_ERROR_TEXT_0,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("localStateErrorText1", IDS_LOCAL_STATE_ERROR_TEXT_1);
  builder->Add("localStateErrorPowerwashButton",
               IDS_LOCAL_STATE_ERROR_POWERWASH_BUTTON);
}

}  // namespace ash
