// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/main/os_settings_hats_handler.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager_factory.h"
#include "content/public/browser/web_ui.h"

namespace ash::settings {

OsSettingsHatsHandler::OsSettingsHatsHandler(Profile* profile)
    : profile_(profile) {}

void OsSettingsHatsHandler::OnJavascriptAllowed() {}

void OsSettingsHatsHandler::OnJavascriptDisallowed() {}

void OsSettingsHatsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "sendSettingsHats",
      base::BindRepeating(&OsSettingsHatsHandler::HandleSendSettingsHats,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "settingsUsedSearch",
      base::BindRepeating(&OsSettingsHatsHandler::HandleSettingsUsedSearch,
                          base::Unretained(this)));
}

void OsSettingsHatsHandler::HandleSettingsUsedSearch(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();

  OsSettingsHatsManagerFactory::GetInstance()
      ->GetForProfile(profile_)
      ->SetSettingsUsedSearch(true);
}

void OsSettingsHatsHandler::HandleSendSettingsHats(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AllowJavascript();

  OsSettingsHatsManagerFactory::GetInstance()
      ->GetForProfile(profile_)
      ->MaybeSendSettingsHats();
}
}  // namespace ash::settings
