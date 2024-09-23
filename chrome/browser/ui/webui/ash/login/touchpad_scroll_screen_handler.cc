// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

TouchpadScrollScreenHandler::TouchpadScrollScreenHandler()
    : BaseScreenHandler(kScreenId) {}

TouchpadScrollScreenHandler::~TouchpadScrollScreenHandler() = default;

void TouchpadScrollScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("TouchpadScrollScreenTitle", IDS_OOBE_TOUCHPAD_SCROLL_TITLE);
  builder->Add("TouchpadScrollScreenDescription",
               IDS_OOBE_TOUCHPAD_SCROLL_DESCRIPTION);
  builder->Add("TouchpadScrollToggleTitle",
               IDS_OOBE_TOUCHPAD_SCROLL_TOGGLE_TITLE);
  builder->Add("TouchpadScrollToggleDescription",
               IDS_OOBE_TOUCHPAD_SCROLL_TOGGLE_DESC);
  builder->Add("TouchpadScrollAreaDescription",
               IDS_OOBE_TOUCHPAD_SCROLL_AREA_DESC);
  builder->Add("choobeTouchpadScrollTitle",
               IDS_OOBE_CHOOBE_TOUCHPAD_SCROLL_TILE_TITLE);
  builder->Add("choobeTouchpadScrollSubtitleEnabled",
               IDS_OOBE_CHOOBE_TOUCHPAD_SCROLL_SUBTITLE_ENABLED);
  builder->Add("choobeTouchpadScrollSubtitleDisabled",
               IDS_OOBE_CHOOBE_TOUCHPAD_SCROLL_SUBTITLE_DISABLED);
}

void TouchpadScrollScreenHandler::SetReverseScrolling(bool value) {
  CallExternalAPI("setReverseScrolling", value);
}

void TouchpadScrollScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

base::WeakPtr<TouchpadScrollScreenView>
TouchpadScrollScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
