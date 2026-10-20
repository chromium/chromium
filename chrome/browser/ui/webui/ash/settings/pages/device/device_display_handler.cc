// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/device_display_handler.h"

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "ui/display/types/display_constants.h"

namespace ash::settings {

DisplayHandler::DisplayHandler()
    : cros_display_config_(CHECK_DEREF(Shell::Get()->cros_display_config())) {}

DisplayHandler::~DisplayHandler() {
  cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
}

void DisplayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "highlightDisplay",
      base::BindRepeating(&DisplayHandler::HandleHighlightDisplay,
                          base::Unretained(this)));
}

void DisplayHandler::HandleHighlightDisplay(const base::ListValue& args) {
  AllowJavascript();

  int64_t display_id;

  if (args.empty() || !args[0].is_string() ||
      !base::StringToInt64(args[0].GetString(), &display_id)) {
    cros_display_config_->HighlightDisplay(display::kInvalidDisplayId);
    return;
  }

  cros_display_config_->HighlightDisplay(display_id);
}

}  // namespace ash::settings
