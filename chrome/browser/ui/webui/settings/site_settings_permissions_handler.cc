// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/site_settings_permissions_handler.h"

SiteSettingsPermissionsHandler::SiteSettingsPermissionsHandler() = default;
SiteSettingsPermissionsHandler::~SiteSettingsPermissionsHandler() = default;

void SiteSettingsPermissionsHandler::HandleGetRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  // TODO(crbug.com/1345920): Replace with content from Unused Site Permissions
  // service.
  base::Value::List result;
  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SiteSettingsPermissionsHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getRevokedUnusedSitePermissionsList",
      base::BindRepeating(&SiteSettingsPermissionsHandler::
                              HandleGetRevokedUnusedSitePermissionsList,
                          base::Unretained(this)));
}

void SiteSettingsPermissionsHandler::OnJavascriptAllowed() {}

void SiteSettingsPermissionsHandler::OnJavascriptDisallowed() {}
