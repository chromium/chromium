// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DisplaySizeScreenHandler::DisplaySizeScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DisplaySizeScreenHandler::~DisplaySizeScreenHandler() = default;

void DisplaySizeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("DisplaySizeTitle", IDS_OOBE_DISPLAY_SIZE_TITLE);
  builder->Add("DisplaySizeDescription", IDS_OOBE_DISPLAY_SIZE_DESCRIPTION);
  builder->Add("DisplaySizeAdditionalDescription",
               IDS_OOBE_DISPLAY_SIZE_DESCRIPTION_ADDITIONAL);
  builder->Add("DisplaySizeSliderTitle", IDS_OOBE_DISPLAY_SIZE_SLIDER_TITLE);
  builder->Add("DisplaySizeSliderDescription",
               IDS_OOBE_DISPLAY_SIZE_SLIDER_DESCRIPTION);
  builder->Add("choobeDisplaySizeTitle",
               IDS_OOBE_CHOOBE_DISPLAY_SIZE_TILE_TITLE);
}

void DisplaySizeScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace ash
