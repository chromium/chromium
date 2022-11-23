// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

/**
 * This handler deals with the permission-related operations on the site
 * settings page.
 */

class SiteSettingsPermissionsHandler : public settings::SettingsPageUIHandler {
 public:
  SiteSettingsPermissionsHandler();

  ~SiteSettingsPermissionsHandler() override;

 private:
  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Returns the list of origins that haven't been visited recently with
  // associated permissions.
  void HandleGetRevokedUnusedSitePermissionsList(const base::Value::List& args);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
