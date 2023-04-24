// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DrivePinningScreenHandler::DrivePinningScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DrivePinningScreenHandler::~DrivePinningScreenHandler() = default;

void DrivePinningScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("DevicePinningScreenTitle", IDS_OOBE_DRIVE_PINNING_TITLE);
  builder->Add("DevicePinningScreenDescription",
               IDS_OOBE_DRIVE_PINNING_SUBTITLE);
  builder->Add("DevicePinningScreenSpaceDescription",
               IDS_OOBE_DRIVE_PINNING_ADDITIONAL_SUBTITLE);

  builder->Add("DevicePinningScreenAcceptButton",
               IDS_OOBE_DRIVE_PINNING_ACCEPT_BUTTON);
  builder->Add("DevicePinningScreenDeclineButton",
               IDS_OOBE_DRIVE_PINNING_DECLINE_BUTTON);
}

void DrivePinningScreenHandler::SetRequiredSpaceInfo(
    std::u16string required_space,
    std::u16string free_space) {
  CallExternalAPI("setRequiredSpaceInfo", required_space, free_space);
}

void DrivePinningScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace ash
