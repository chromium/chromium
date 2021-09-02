// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_display_handler.h"

#include "ash/public/ash_interfaces.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {
namespace settings {

DisplayHandler::DisplayHandler() {
  ash::BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
}

DisplayHandler::~DisplayHandler() {
  cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
}

void DisplayHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "highlightDisplay",
      base::BindRepeating(&DisplayHandler::HandleHighlightDisplay,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "dragDisplayDelta",
      base::BindRepeating(&DisplayHandler::HandleDragDisplayDelta,
                          base::Unretained(this)));
}

void DisplayHandler::HandleHighlightDisplay(const base::ListValue* args) {
  AllowJavascript();

  std::string display_id_str;
  int64_t display_id;

  if (!args->GetString(0, &display_id_str) ||
      !base::StringToInt64(display_id_str, &display_id)) {
    cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
    return;
  }

  cros_display_config_->HighlightDisplay(display_id);
}

void DisplayHandler::HandleDragDisplayDelta(const base::ListValue* args) {
  DCHECK_EQ(3U, args->GetList().size());
  AllowJavascript();

  const auto& args_list = args->GetList();
  const std::string& display_id_str = args_list[0].GetString();
  int32_t delta_x = static_cast<int32_t>(args_list[1].GetInt());
  int32_t delta_y = static_cast<int32_t>(args_list[2].GetInt());

  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id)) {
    NOTREACHED() << "Unable to parse |display_id| for HandleDragDisplayDelta";
    return;
  }

  cros_display_config_->DragDisplayDelta(display_id, delta_x, delta_y);
}

}  // namespace settings
}  // namespace chromeos
