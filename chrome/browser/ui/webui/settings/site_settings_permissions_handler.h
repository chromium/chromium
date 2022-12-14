// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

/**
 * This handler deals with the permission-related operations on the site
 * settings page.
 */

class SiteSettingsPermissionsHandler : public settings::SettingsPageUIHandler {
 public:
  explicit SiteSettingsPermissionsHandler(Profile* profile);

  ~SiteSettingsPermissionsHandler() override;

 private:
  friend class SiteSettingsPermissionsHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsPermissionsHandlerTest,
                           PopulateUnusedSitePermissionsData);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Returns the list of revoked permissions to be used in
  // "Unused site permissions" module.
  void HandleGetRevokedUnusedSitePermissionsList(const base::Value::List& args);

  // Returns the list of revoked permissions that belongs to origins which
  // haven't been visited recently.
  base::Value::List PopulateUnusedSitePermissionsData();

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
