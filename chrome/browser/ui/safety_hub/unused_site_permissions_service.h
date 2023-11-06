// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefChangeRegistrar;
class PrefService;

constexpr char kUnusedSitePermissionsResultKey[] = "permissions";
constexpr char kUnusedSitePermissionsResultPermissionTypesKey[] =
    "permissionTypes";
constexpr char kUnusedSitePermissionsResultExpirationKey[] = "expiration";

namespace url {
class Origin;
}

namespace content {
class Page;
}  // namespace content

// This class keeps track of unused permissions, updates their last_visit date
// on navigations and clears them periodically.
class UnusedSitePermissionsService : public SafetyHubService,
                                     public content_settings::Observer {
 public:
  struct RevokedPermission {
   public:
    RevokedPermission(ContentSettingsPattern origin,
                      std::set<ContentSettingsType> permission_types,
                      base::Time expiration);

    RevokedPermission(const RevokedPermission&);
    RevokedPermission& operator=(const RevokedPermission&) = delete;

    ~RevokedPermission();

    ContentSettingsPattern origin;
    std::set<ContentSettingsType> permission_types;
    base::Time expiration;
  };

  struct ContentSettingEntry {
    ContentSettingsType type;
    ContentSettingPatternSource source;
  };

  // The result of the periodic update of unused site permissions contains
  // the permissions that have been revoked. These revoked permissions will be
  // stored until the clean-up threshold has been reached.
  class UnusedSitePermissionsResult : public SafetyHubService::Result {
   public:
    UnusedSitePermissionsResult();

    explicit UnusedSitePermissionsResult(const base::Value::Dict& dict);

    UnusedSitePermissionsResult(const UnusedSitePermissionsResult&);
    UnusedSitePermissionsResult& operator=(const UnusedSitePermissionsResult&) =
        default;

    ~UnusedSitePermissionsResult() override;

    std::unique_ptr<SafetyHubService::Result> Clone() const override;

    using UnusedPermissionMap =
        std::map<std::string, std::list<ContentSettingEntry>>;

    void AddRevokedPermission(ContentSettingsPattern origin,
                              std::set<ContentSettingsType> permission_types,
                              base::Time expiration);

    void SetRecentlyUnusedPermissions(UnusedPermissionMap map) {
      recently_unused_permissions_ = map;
    }

    UnusedPermissionMap GetRecentlyUnusedPermissions() {
      return recently_unused_permissions_;
    }

    std::list<RevokedPermission> GetRevokedPermissions();

    std::set<ContentSettingsPattern> GetRevokedOrigins() const;

    // SafetyHubService::Result implementation
    base::Value::Dict ToDictValue() const override;

    bool IsTriggerForMenuNotification() const override;

    bool WarrantsNewMenuNotification(
        const Result& previousResult) const override;

    std::u16string GetNotificationString() const override;

    int GetNotificationCommandId() const override;

   private:
    std::list<RevokedPermission> revoked_permissions_;
    UnusedPermissionMap recently_unused_permissions_;
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

  explicit UnusedSitePermissionsService(HostContentSettingsMap* hcsm,
                                        PrefService* prefs);

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

  // Returns the list of all permissions that have been revoked.
  std::unique_ptr<Result> GetRevokedPermissions();

  // Stops or restarts permissions autorevocation upon the pref change.
  void OnPermissionsAutorevocationControlChanged();

  // Does most of the heavy lifting of the update process: for each permission,
  // it determines whether it should be considered as recently unused (i.e. one
  // week). This list will be further filtered in the UI task to determine which
  // permissions should be revoked.
  static std::unique_ptr<Result> UpdateOnBackgroundThread(
      base::Clock* clock,
      const scoped_refptr<HostContentSettingsMap> hcsm);

  // SafetyHubService implementation
  // Returns a weak pointer to the service.
  base::WeakPtr<SafetyHubService> GetAsWeakRef() override;

  // Test support:
  void SetClockForTesting(base::Clock* clock);
  std::vector<ContentSettingEntry> GetTrackedUnusedPermissionsForTesting();

  using UnusedPermissionMap =
      std::map<std::string, std::list<ContentSettingEntry>>;

 private:
  // Called by TabHelper when a URL was visited.
  void OnPageVisited(const url::Origin& origin);

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

  // SafetyHubService implementation

  std::unique_ptr<SafetyHubService::Result> InitializeLatestResultImpl()
      override;

  // Returns the interval at which the repeated updates will be run.
  base::TimeDelta GetRepeatedUpdateInterval() override;

  // Returns a reference to the static |UpdateOnBackgroundThread| function,
  // bound with a |Result| containing a reference to the clock and
  // host content settings map.
  base::OnceCallback<std::unique_ptr<Result>()> GetBackgroundTask() override;

  // Uses the |UnusedPermissionMap| from the background task to determine which
  // permissions should be revoked, revokes them and returns the list of revoked
  // permissions.
  std::unique_ptr<Result> UpdateOnUIThread(
      std::unique_ptr<Result> result) override;

  // Returns if the permissions auto-revocation is enabled for unused sites.
  bool IsAutoRevocationEnabled();

  // Set of permissions that haven't been used for at least a week.
  UnusedPermissionMap recently_unused_permissions_;

  const scoped_refptr<HostContentSettingsMap> hcsm_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  // Observes user profile prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<UnusedSitePermissionsService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_H_
