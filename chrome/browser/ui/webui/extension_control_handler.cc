// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extension_control_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
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
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1u);
  // Calling base::Value::GetString() on a non-string value can cause a DCHECK.
  if (!args[0].is_string()) {
    return;
  }

  const std::string& extension_id = args[0].GetString();
  // `extension_id` is from the WebUI frontend, so it could be
  // corrupted/compromised. If so, ignore it because downstream code can assume
  // valid extension IDs.
  // TODO(crbug.com/518751548): Investigate whether we should kill the renderer
  // when this happens here and in other similar places.
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    return;
  }

  auto* extension_registrar =
      extensions::ExtensionRegistrar::Get(Profile::FromWebUI(web_ui()));
  extension_registrar->DisableExtension(
      extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
}
