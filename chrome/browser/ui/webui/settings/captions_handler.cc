// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/captions_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/accessibility/caption_settings_dialog.h"
#include "content/public/browser/web_ui.h"

namespace settings {

CaptionsHandler::CaptionsHandler() {}

CaptionsHandler::~CaptionsHandler() {}

void CaptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openSystemCaptionsDialog",
      base::BindRepeating(&CaptionsHandler::HandleOpenSystemCaptionsDialog,
                          base::Unretained(this)));
}

void CaptionsHandler::OnJavascriptAllowed() {}

void CaptionsHandler::OnJavascriptDisallowed() {}

void CaptionsHandler::HandleOpenSystemCaptionsDialog(
    const base::ListValue* args) {
  captions::CaptionSettingsDialog::ShowCaptionSettingsDialog();
}

}  // namespace settings
