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
  builder->Add("displaySizeTitle", IDS_OOBE_DISPLAY_SIZE_TITLE);
  builder->Add("displaySizeDescription", IDS_OOBE_DISPLAY_SIZE_DESCRIPTION);
  builder->Add("displaySizeSliderTitle", IDS_OOBE_DISPLAY_SIZE_SLIDER_TITLE);
  builder->Add("displaySizeSliderDescription",
               IDS_OOBE_DISPLAY_SIZE_SLIDER_DESCRIPTION);

  // oobe_display_size_selector component resources.
  builder->Add("displaySizePreview", IDS_OOBE_DISPLAY_SIZE_PREVIEW);
  builder->Add("displaySizeA4App", IDS_OOBE_DISPLAY_SIZE_A4_APP_NAME);
  builder->Add("displaySizeCalculatorApp",
               IDS_OOBE_DISPLAY_SIZE_CALCULATOR_APP_NAME);
  builder->Add("displaySizeCameraApp", IDS_OOBE_DISPLAY_SIZE_CAMERA_APP_NAME);
  builder->Add("displaySizeFilesApp", IDS_OOBE_DISPLAY_SIZE_FILES_APP_NAME);
  builder->Add("displaySizePhotosApp", IDS_OOBE_DISPLAY_SIZE_PHOTOS_APP_NAME);
  builder->Add("displaySizeSettingsApp",
               IDS_OOBE_DISPLAY_SIZE_SETTINGS_APP_NAME);
  builder->Add("displaySizeValue", IDS_OOBE_DISPLAY_SIZE_VALUE);
  builder->Add("displaySizePositive",
               IDS_OOBE_DISPLAY_SIZE_POSITIVE_BUTTON_ARIA);
  builder->Add("displaySizeNegative",
               IDS_OOBE_DISPLAY_SIZE_NEGATIVE_BUTTON_ARIA);

  // CHOOBE resources
  builder->Add("choobeDisplaySizeTitle",
               IDS_OOBE_CHOOBE_DISPLAY_SIZE_TILE_TITLE);
  builder->Add("choobeDisplaySizeSubtitle",
               IDS_OOBE_CHOOBE_DISPLAY_SIZE_TILE_SUBTITLE);
}

void DisplaySizeScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

base::WeakPtr<DisplaySizeScreenView> DisplaySizeScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
