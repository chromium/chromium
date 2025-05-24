// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extension_control_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"

ExtensionControlHandler::ExtensionControlHandler() = default;
ExtensionControlHandler::~ExtensionControlHandler() = default;

void ExtensionControlHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "disableExtension",
      base::BindRepeating(&ExtensionControlHandler::HandleDisableExtension,
                          base::Unretained(this)));
}

void ExtensionControlHandler::HandleDisableExtension(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1u);
  const std::string& extension_id = args[0].GetString();
  auto* extension_registrar =
      extensions::ExtensionRegistrar::Get(Profile::FromWebUI(web_ui()));
  extension_registrar->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
}
