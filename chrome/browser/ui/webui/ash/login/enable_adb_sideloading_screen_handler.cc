// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"

#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

EnableAdbSideloadingScreenHandler::EnableAdbSideloadingScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EnableAdbSideloadingScreenHandler::~EnableAdbSideloadingScreenHandler() =
    default;

void EnableAdbSideloadingScreenHandler::Show() {
  ShowInWebUI();
}

void EnableAdbSideloadingScreenHandler::SetScreenState(UIState value) {
  CallExternalAPI("setScreenState", static_cast<int>(value));
}

base::WeakPtr<EnableAdbSideloadingScreenView>
EnableAdbSideloadingScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void EnableAdbSideloadingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("enableAdbSideloadingSetupTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_TITLE);
  builder->Add("enableAdbSideloadingSetupMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_MESSAGE);
  builder->Add("enableAdbSideloadingErrorTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_TITLE);
  builder->Add("enableAdbSideloadingErrorMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_MESSAGE);
  builder->Add("enableAdbSideloadingLearnMore",
               IDS_ENABLE_ARC_ADB_SIDELOADING_LEARN_MORE);
  builder->Add("enableAdbSideloadingConfirmButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CONFIRM_BUTTON);
  builder->Add("enableAdbSideloadingCancelButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CANCEL_BUTTON);
  builder->Add("enableAdbSideloadingOkButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_OK_BUTTON);
}

}  // namespace ash
