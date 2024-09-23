// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/mac_system_settings_handler.h"

#include "base/mac/mac_util.h"

using content::WebContents;

namespace settings {

MacSystemSettingsHandler::MacSystemSettingsHandler() = default;
MacSystemSettingsHandler::~MacSystemSettingsHandler() = default;

void MacSystemSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openTrackpadGesturesSettings",
      base::BindRepeating(
          &MacSystemSettingsHandler::HandleOpenTrackpadGesturesSettings,
          base::Unretained(this)));
}

void MacSystemSettingsHandler::HandleOpenTrackpadGesturesSettings(
    const base::Value::List& args) {
  AllowJavascript();
  // TODO(crbug.com/40279003): Figure out how to directly open the more gestures
  // subpane. Currently this only opens the first subpane of trackpad settings.
  base::mac::OpenSystemSettingsPane(base::mac::SystemSettingsPane::kTrackpad);
}

}  // namespace settings
