// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_
#define COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_

#include <list>
#include <map>
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class HostContentSettingsMap;

namespace url {
class Origin;
}

namespace content {
class Page;
}  // namespace content

namespace permissions {

// This task keeps track of unused permissions, updates their last_visit date
// on navigations and clears them periodically.
class UnusedSitePermissionsService
    : public KeyedService,
      public base::SupportsWeakPtr<UnusedSitePermissionsService>,
      public content_settings::Observer {
 public:
  struct ContentSettingEntry {
    ContentSettingsType type;
    ContentSettingPatternSource source;
  };

  class TabHelper : public content::WebContentsObserver,
                    public content::WebContentsUserData<TabHelper> {
   public:
    TabHelper(const TabHelper&) = delete;
    TabHelper& operator=(const TabHelper&) = delete;
    ~TabHelper() override;

    // WebContentsObserver:
    void PrimaryPageChanged(content::Page& page) override;

   private:
    explicit TabHelper(
        content::WebContents* web_contents,
        UnusedSitePermissionsService* unused_site_permission_service);

    base::WeakPtr<UnusedSitePermissionsService> unused_site_permission_service_;

    friend class content::WebContentsUserData<TabHelper>;
    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

  explicit UnusedSitePermissionsService(HostContentSettingsMap* hcsm);

  UnusedSitePermissionsService(const UnusedSitePermissionsService&) = delete;
  UnusedSitePermissionsService& operator=(const UnusedSitePermissionsService&) =
      delete;

  ~UnusedSitePermissionsService() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // KeyedService implementation.
  void Shutdown() override;

  // If the user clicked "Allow again" for an auto-revoked origin, the
  // permissions for that site should not be auto-revoked again by the service.
  void IgnoreOriginForAutoRevocation(const url::Origin& origin);

  // Triggers an update of the unused permission map. Automatically registers
  // a delayed task for another update after 24h.
  void StartRepeatedUpdates();

  // Re-grants permissions that are auto-revoked ones and removes the origin
  // from revoked permissions list.
  void RegrantPermissionsForOrigin(const url::Origin& origin);

  // Reverse changes made by |RegrantPermissionsForOrigin|. Adds this origin to
  // the removed permissions list and resets its permissions.
  void UndoRegrantPermissionsForOrigin(
      const std::set<ContentSettingsType> permissions,
      const absl::optional<content_settings::ContentSettingConstraints>
          constraint,
      const url::Origin origin);

  // Clear the list of revoked permissions so they will no longer be shown to
  // the user. Does not change permissions themselves.
  void ClearRevokedPermissionsList();

  // Stores revoked permissions data on HCSM.
  void StorePermissionInRevokedPermissionSetting(
      const std::set<ContentSettingsType> permissions,
      const absl::optional<content_settings::ContentSettingConstraints>
          constraint,
      const url::Origin origin);

  static absl::optional<uint32_t> GetDaysSinceRevocation(
      const GURL& origin,
      ContentSettingsType content_settings_type,
      base::Time current_time,
      HostContentSettingsMap* hcsm);

  // Test support:
  void SetClockForTesting(base::Clock* clock);
  std::vector<ContentSettingEntry> GetTrackedUnusedPermissionsForTesting();
  void UpdateUnusedPermissionsForTesting();

  using UnusedPermissionMap =
      std::map<std::string, std::list<ContentSettingEntry>>;

 private:
  // Called by TabHelper when a URL was visited.
  void OnPageVisited(const url::Origin& origin);

  // Called on UI thread
  void UpdateUnusedPermissionsAsync(base::RepeatingClosure callback);

  // Called on UI thread.
  void OnUnusedPermissionsMapRetrieved(base::OnceClosure callback,
                                       UnusedPermissionMap map);

  // Removes a pattern from the list of revoked permissions so that the entry is
  // no longer shown to the user. Does not affect permissions themselves.
  void DeletePatternFromRevokedPermissionList(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Revokes permissions that belong to sites that were last visited over 60
  // days ago.
  void RevokeUnusedPermissions();

  // Stores revoked permissions data on HCSM.
  void StorePermissionInRevokedPermissionSetting(
      const std::set<ContentSettingsType> permissions,
      const absl::optional<content_settings::ContentSettingConstraints>
          constraint,
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Set of permissions that haven't been used for at least a week.
  UnusedPermissionMap recently_unused_permissions_;
  // Repeating timer that updates the recently_unused_permissions_ map.
  base::RepeatingTimer update_timer_;

  const scoped_refptr<HostContentSettingsMap> hcsm_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  raw_ptr<base::Clock> clock_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_UNUSED_SITE_PERMISSIONS_SERVICE_H_
