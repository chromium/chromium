// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

  // Does most of the heavy lifting of the update process: for each permission,
  // it determines whether it should be considered as recently unused (i.e. one
  // week). This list will be further filtered in the UI task to determine which
  // permissions should be revoked.
  static std::unique_ptr<SafetyHubResult> UpdateOnBackgroundThread(
      base::Clock* clock,
      const scoped_refptr<HostContentSettingsMap> hcsm);

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

  // Revokes permissions from sites whose last visit is older than a defined
  // threshold (e.g. currently 60 days).
  void RevokeUnusedPermissions(std::unique_ptr<SafetyHubResult> result);

  // Returns true if settings are being changed due to auto revocation of
  // unused site permissions.
  bool IsRevocationRunning();

  // Called by TabHelper when a URL was visited.
  void OnPageVisited(const url::Origin& origin);

  // Re-grants permissions that are auto-revoked ones and removes the origin
  // from revoked permissions list.
  void RegrantPermissionsForOrigin(const url::Origin& origin);

  // Reverse changes made by |RegrantPermissionsForOrigin|. Adds this origin to
  // the removed permissions list and resets its permissions.
  void UndoRegrantPermissionsForOrigin(const PermissionsData& permission);

  // Removes a pattern from the list of revoked permissions so that the entry is
  // no longer shown to the user. Does not affect permissions themselves.
  void DeletePatternFromRevokedUnusedSitePermissionList(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Stores revoked permissions data on HCSM.
  void StorePermissionInUnusedSitePermissionSetting(
      const std::set<ContentSettingsType>& permissions,
      const base::Value::Dict& chooser_permissions_data,
      const std::optional<content_settings::ContentSettingConstraints>
          constraint,
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Test support:
  void SetClockForTesting(base::Clock* clock);
  std::vector<ContentSettingEntry> GetTrackedUnusedPermissionsForTesting();

  using UnusedPermissionMap = RevokedPermissionsResult::UnusedPermissionMap;

 private:
  FRIEND_TEST_ALL_PREFIXES(UnusedSitePermissionsManagerTest,
                           UpdateIntegerValuesToGroupName_AllContentSettings);
  FRIEND_TEST_ALL_PREFIXES(
      UnusedSitePermissionsManagerTest,
      UpdateIntegerValuesToGroupName_SubsetOfContentSettings);
  FRIEND_TEST_ALL_PREFIXES(
      UnusedSitePermissionsManagerTest,
      UpdateIntegerValuesToGroupName_UnknownContentSettings);

  // If the user clicked "Allow again" for an auto-revoked origin, the
  // permissions for that site should not be auto-revoked again by the service.
  void IgnoreOriginForAutoRevocation(const url::Origin& origin);

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

  // Set of permissions that haven't been used for at least a week.
  UnusedPermissionMap recently_unused_permissions_;

  // Returns true if automatic check and revocation of unused site permissions
  // is occurring. This value is used in `OnContentSettingChanged` to help
  // decide whether to clean up revoked permission data.
  bool is_unused_site_revocation_running_ = false;

  // Clock to implement revocation based on last visited time.
  raw_ptr<base::Clock> clock_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MANAGER_H_
