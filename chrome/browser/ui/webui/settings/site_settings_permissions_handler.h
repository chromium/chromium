// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/origin.h"

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
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsPermissionsHandlerTest,
                           HandleAllowPermissionsAgainForUnusedSite);
  FRIEND_TEST_ALL_PREFIXES(SiteSettingsPermissionsHandlerTest,
                           HandleAcknowledgeRevokedUnusedSitePermissionsList);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Returns the list of revoked permissions to be used in
  // "Unused site permissions" module.
  void HandleGetRevokedUnusedSitePermissionsList(const base::Value::List& args);

  // Re-grant the revoked permissions and remove the given origin from the
  // revoked permissions list.
  void HandleAllowPermissionsAgainForUnusedSite(const base::Value::List& args);

  // Reverse the changes made by |HandleAllowPermissionsAgainForUnusedSite| for
  // the given |UnusedSitePermission| object.
  void HandleUndoAllowPermissionsAgainForUnusedSite(
      const base::Value::List& args);

  // Clear the list of revoked permissions so they are not shown again.
  // Permission settings themselves are not affected by this.
  void HandleAcknowledgeRevokedUnusedSitePermissionsList(
      const base::Value::List& args);

  // Reverse the changes made by
  // |HandleAcknowledgeRevokedUnusedSitePermissionsList| for the given list of
  // |UnusedSitePermission| objects. List of revoked
  // permissions is repopulated. Permission settings are not changed.
  void HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
      const base::Value::List& args);

  // Returns the list of revoked permissions that belongs to origins which
  // haven't been visited recently.
  base::Value::List PopulateUnusedSitePermissionsData();

  // Sends the list of unused site permissions to review to the WebUI.
  void SendUnusedSitePermissionsReviewList();

  // Get values from |UnusedSitePermission| object in
  // site_settings_permissions_browser_proxy.ts.
  std::tuple<url::Origin,
             std::set<ContentSettingsType>,
             content_settings::ContentSettingConstraints>
  GetUnusedSitePermissionsFromDict(
      const base::Value::Dict& unused_site_permissions);

  const raw_ptr<Profile> profile_;

  base::Clock* clock_;

  void SetClockForTesting(base::Clock* clock);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SITE_SETTINGS_PERMISSIONS_HANDLER_H_
