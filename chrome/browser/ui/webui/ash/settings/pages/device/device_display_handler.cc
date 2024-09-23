// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/device_display_handler.h"

#include "ash/public/ash_interfaces.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "ui/display/types/display_constants.h"

namespace ash::settings {

DisplayHandler::DisplayHandler() {
  BindCrosDisplayConfigController(
      cros_display_config_.BindNewPipeAndPassReceiver());
}

DisplayHandler::~DisplayHandler() {
  cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
}

void DisplayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "highlightDisplay",
      base::BindRepeating(&DisplayHandler::HandleHighlightDisplay,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "dragDisplayDelta",
      base::BindRepeating(&DisplayHandler::HandleDragDisplayDelta,
                          base::Unretained(this)));
}

void DisplayHandler::HandleHighlightDisplay(const base::Value::List& args) {
  AllowJavascript();

  int64_t display_id;

  if (args.empty() || !args[0].is_string() ||
      !base::StringToInt64(args[0].GetString(), &display_id)) {
    cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
    return;
  }

  cros_display_config_->HighlightDisplay(display_id);
}

void DisplayHandler::HandleDragDisplayDelta(const base::Value::List& args) {
  DCHECK_EQ(3U, args.size());
  AllowJavascript();

  const std::string& display_id_str = args[0].GetString();
  int32_t delta_x = static_cast<int32_t>(args[1].GetInt());
  int32_t delta_y = static_cast<int32_t>(args[2].GetInt());

  int64_t display_id;
  if (!base::StringToInt64(display_id_str, &display_id)) {
    NOTREACHED_IN_MIGRATION()
        << "Unable to parse |display_id| for HandleDragDisplayDelta";
    return;
  }

  cros_display_config_->DragDisplayDelta(display_id, delta_x, delta_y);
}

}  // namespace ash::settings
