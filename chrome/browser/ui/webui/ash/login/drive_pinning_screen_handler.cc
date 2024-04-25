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
  builder->Add("DevicePinningScreenToggleTitle",
               IDS_OOBE_DRIVE_PINNING_TOGGLE_TITLE);
  builder->Add("DevicePinningScreenToggleSubtitle",
               IDS_OOBE_DRIVE_PINNING_TOGGLE_SUBTITLE);

  builder->Add("choobeDrivePinningTitle",
               IDS_OOBE_CHOOBE_DRIVE_PINNING_TILE_TITLE);

  builder->Add("choobeDevicePinningSubtitleEnabled",
               IDS_OOBE_CHOOBE_DRIVE_PINNING_SUBTITLE_ENABLED);
  builder->Add("choobeDevicePinningSubtitleDisabled",
               IDS_OOBE_CHOOBE_DRIVE_PINNING_SUBTITLE_DISABLED);
}

void DrivePinningScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

base::WeakPtr<DrivePinningScreenView> DrivePinningScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
