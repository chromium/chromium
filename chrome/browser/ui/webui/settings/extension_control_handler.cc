// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/extension_control_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"

namespace settings {

ExtensionControlHandler::ExtensionControlHandler() {}
ExtensionControlHandler::~ExtensionControlHandler() {}

void ExtensionControlHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "disableExtension",
      base::BindRepeating(&ExtensionControlHandler::HandleDisableExtension,
                          base::Unretained(this)));
}

void ExtensionControlHandler::HandleDisableExtension(
    const base::Value::List& args) {
  const std::string& extension_id = args[0].GetString();
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
          ->extension_service();
  DCHECK(extension_service);
  extension_service->DisableExtension(
      extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
}

}  // namespace settings
