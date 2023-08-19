// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extension_control_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_system.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/strcat.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ExtensionControlHandler::ExtensionControlHandler() = default;
ExtensionControlHandler::~ExtensionControlHandler() = default;

void ExtensionControlHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "disableExtension",
      base::BindRepeating(&ExtensionControlHandler::HandleDisableExtension,
                          base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "openExtensionPageInLacros",
      base::BindRepeating(
          &ExtensionControlHandler::HandleOpenExtensionPageInLacros,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ExtensionControlHandler::HandleDisableExtension(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1u);
  const std::string& extension_id = args[0].GetString();
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
          ->extension_service();
  DCHECK(extension_service);
  extension_service->DisableExtension(
      extension_id, extensions::disable_reason::DISABLE_USER_ACTION);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ExtensionControlHandler::HandleOpenExtensionPageInLacros(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1u);
  const std::string& extension_id = args[0].GetString();
  GURL url(
      base::StrCat({chrome::kChromeUIExtensionsURL, "?id=", extension_id}));
  CHECK(url.is_valid());

  crosapi::BrowserManager::Get()->SwitchToTab(
      url, NavigateParams::PathBehavior::RESPECT);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
