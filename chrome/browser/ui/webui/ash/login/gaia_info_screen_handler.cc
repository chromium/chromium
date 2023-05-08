// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

GaiaInfoScreenHandler::GaiaInfoScreenHandler() : BaseScreenHandler(kScreenId) {}

GaiaInfoScreenHandler::~GaiaInfoScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void GaiaInfoScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("gaiaInfoScreenTitle", IDS_GAIA_INFO_TITLE,
                ui::GetChromeOSDeviceTypeResourceId());
  builder->Add("gaiaInfoScreenDescription", IDS_GAIA_INFO_DESCRIPTION);
}

void GaiaInfoScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace ash
