// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_

#include <memory>
#include <string>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"

class PrefChangeRegistrar;
class PrefService;

namespace url {
class Origin;
}

// This class keeps track of unused site permissions by updating
// their last_visit date on navigations and clearing them periodically.
class UnusedSitePermissionsManager {
 public:
  explicit UnusedSitePermissionsManager(
      content::BrowserContext* browser_context,
      PrefService* prefs);

  UnusedSitePermissionsManager(const UnusedSitePermissionsManager&) = delete;
  UnusedSitePermissionsManager& operator=(const UnusedSitePermissionsManager&) =
      delete;

  ~UnusedSitePermissionsManager();

  // Helper to convert content settings type into its string representation.
  static std::string ConvertContentSettingsTypeToKey(ContentSettingsType type);

  // Helper to get content settings type from its string representation.
  static ContentSettingsType ConvertKeyToContentSettingsType(
      const std::string& key);

  // Helper to convert single origin primary pattern to an origin.
  // Converting a primary pattern to an origin is normally an anti-pattern, and
  // this method should only be used for single origin primary patterns.
  // They have fully defined URL+scheme+port which makes converting
  // a primary pattern to an origin successful.
  static url::Origin ConvertPrimaryPatternToOrigin(
      const ContentSettingsPattern& primary_pattern);

 private:
  FRIEND_TEST_ALL_PREFIXES(UnusedSitePermissionsManagerTest,
                           UpdateIntegerValuesToGroupName_AllContentSettings);
  FRIEND_TEST_ALL_PREFIXES(
      UnusedSitePermissionsManagerTest,
      UpdateIntegerValuesToGroupName_SubsetOfContentSettings);
  FRIEND_TEST_ALL_PREFIXES(
      UnusedSitePermissionsManagerTest,
      UpdateIntegerValuesToGroupName_UnknownContentSettings);

  // Convert all integer permission values to string, if there is any
  // permission represented by integer.
  void UpdateIntegerValuesToGroupName();

  // Pointer to an object that allows us to manage site permissions.
  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(browser_context_.get());
  }

  // Pointer to a browser context whose permissions are being updated.
  raw_ptr<content::BrowserContext> browser_context_;

  // Observes user profile prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_
