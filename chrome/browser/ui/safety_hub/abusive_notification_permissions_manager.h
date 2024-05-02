// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"

class GURL;

namespace {
// Maximum time in milliseconds to wait for the Safe Browsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;
}  // namespace

namespace safe_browsing {
struct ThreatMetadata;
}  // namespace safe_browsing

// This class keeps track of abusive notification permissions by checking URLs
// against the Safe Browsing social engineering blocklist. This also handles
// automatic revocation and responding to user decisions in Safety Hub.
class AbusiveNotificationPermissionsManager {
 public:
  explicit AbusiveNotificationPermissionsManager(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<HostContentSettingsMap> hcsm);

  AbusiveNotificationPermissionsManager(
      const AbusiveNotificationPermissionsManager&) = delete;
  AbusiveNotificationPermissionsManager& operator=(
      const AbusiveNotificationPermissionsManager&) = delete;

  ~AbusiveNotificationPermissionsManager();

  // Returns the list of all permissions that have been revoked.
  ContentSettingsForOneType GetRevokedPermissions() const;

  // Calls `PerformSafeBrowsingChecks` on URLs which have notifications
  // enabled and haven't been marked as a URL to be ignored.
  void CheckNotificationPermissionOrigins();

  // The user regrants permission for an origin, so we should allow notification
  // permissions again and add a constraint so that this permission is not
  // auto-revoked during future Safety Hub checks.
  void RegrantPermissionForOrigin(const GURL& url);

  // Undo the actions from `RegrantPermissionForOrigin` by changing the
  // `NOTIFICATIONS` setting back to `CONTENT_SETTING_BLOCK` and the
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting back to a dictionary
  // with `safety_hub::kRevokedStatusDictKeyStr` set to
  // `safety_hub::kRevokeStr`.
  void UndoRegrantPermissionForOrigin(const GURL url);

  // Clear the list of abusive notification permissions so they will no longer
  // be shown to the user. Does not change permissions themselves.
  void ClearRevokedPermissionsList();

  // Add `origin` back into the list of abusive notification permissions we show
  // to the user in Safety Hub. Does not change permissions themselves.
  void UndoRemoveOriginFromRevokedPermissionsList(const GURL url);

 private:
  friend class AbusiveNotificationPermissionsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(
      AbusiveNotificationPermissionsManagerTest,
      AddAllowedAbusiveNotificationSitesToRevokedOriginSet);
  FRIEND_TEST_ALL_PREFIXES(
      AbusiveNotificationPermissionsManagerTest,
      DoesNotAddSafeAbusiveNotificationSitesToRevokedOriginSet);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           DoesNotAddBlockedSettingToRevokedList);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           DoesNotAddIgnoredSettingToRevokedList);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           DoesNotAddAbusiveNotificationSitesOnTimeout);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           RegrantedPermissionShouldNotBeChecked);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           ClearRevokedPermissionsList);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           UndoRemoveOriginFromRevokedPermissionsList);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           UndoRegrantPermissionForOrigin);

  // On object creation, checks the Safe Browsing blocklist for `url_`
  // and revokes notification permissions if blocklisted.
  // This subclass is necessary to avoid concurrency issues - each
  // request needs its own Client and the AbusiveNotificationPermissionsManager
  // makes multiple Safe Browsing requests.
  class SafeBrowsingCheckClient
      : safe_browsing::SafeBrowsingDatabaseManager::Client {
   public:
    SafeBrowsingCheckClient(
        safe_browsing::SafeBrowsingDatabaseManager* database_manager,
        raw_ptr<std::map<SafeBrowsingCheckClient*,
                         std::unique_ptr<SafeBrowsingCheckClient>>>
            safe_browsing_request_clients,
        raw_ptr<HostContentSettingsMap> hcsm,
        GURL url,
        int safe_browsing_check_delay);

    ~SafeBrowsingCheckClient() override;

    // Trigger the call to check the Safe Browsing social engineering blocklist.
    void CheckSocialEngineeringBlocklist();

   private:
    // safe_browsing::SafeBrowsingDatabaseManager::Client:
    void OnCheckBrowseUrlResult(
        const GURL& url,
        safe_browsing::SBThreatType threat_type,
        const safe_browsing::ThreatMetadata& metadata) override;

    // Callback to be run if we make a Safe Browsing blocklist request and have
    // not received a response within `kCheckUrlTimeoutMs` time.
    void OnCheckBlocklistTimeout();

    // A pointer to the `database_manager_` of the
    // `AbusiveNotificationPermissionsManager`.
    raw_ptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

    // A pointer to the `safe_browsing_request_clients_`
    // of the `AbusiveNotificationPermissionsManager`.
    raw_ptr<std::map<SafeBrowsingCheckClient*,
                     std::unique_ptr<SafeBrowsingCheckClient>>>
        safe_browsing_request_clients_;

    // A pointer to the `hcsm_` of the `AbusiveNotificationPermissionsManager`.
    raw_ptr<HostContentSettingsMap> hcsm_;

    // The URL that is being checked against the Safe Browsing blocklist.
    GURL url_;

    // Delay amount allowed for blocklist checks.
    int safe_browsing_check_delay_;

    // Timer for running Safe Browsing blocklist checks. If `kCheckUrlTimeoutMs`
    // time has passed, run `OnCheckBlocklistTimeout`.
    base::OneShotTimer timer_;

    base::WeakPtrFactory<
        AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient>
        weak_factory_{this};
  };

  // Primarily used for tests.
  void SetNullSBCheckDelayForTesting() { safe_browsing_check_delay_ = 0; }

  // Create a `SafeBrowsingCheckClient` object, triggering a blocklist check,
  // and add it to `safe_browsing_request_clients_`.
  void PerformSafeBrowsingChecks(GURL url);

  // Get the dictionary setting value of the
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting. Returns `Type::NONE` if
  // there is no `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting value.
  base::Value GetRevokedAbusiveNotificationPermissionsSettingValue(
      ContentSettingPatternSource content_setting) const;

  // Returns true if there is a `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`
  // setting value for the setting URL with the
  // `safety_hub::kRevokedStatusDictKeyStr` key set to `safety_hub::kIgnoreStr`.
  // Note that the method expects a non-empty
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting as input.
  bool IsAbusiveNotificationRevocationIgnored(
      ContentSettingPatternSource content_setting) const;

  // Returns true if the notification permission is allowed and the setting
  // does not indicate "ignore".
  bool ShouldCheckOrigin(const ContentSettingPatternSource& setting) const;

  // Clears any helper members that stored state from the previous safety check.
  // Called each time we check a set of URLs with Safe Browsing.
  void ResetSafeBrowsingCheckHelpers();

  // Used for interactions with the local database, when checking the blocklist.
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  // Object that allows us to manage site permissions.
  scoped_refptr<HostContentSettingsMap> hcsm_;

  // Safe Browsing blocklist check clients. Each object is responsible for a
  // single Safe Browsing check, given a URL. Stored this way so that the object
  // can delete itself from this map within the SafeBrowsingCheckClient.
  std::map<SafeBrowsingCheckClient*, std::unique_ptr<SafeBrowsingCheckClient>>
      safe_browsing_request_clients_;

  // Length of time allowed for Safe Browsing check before timeout. This allows
  // us to test timeout behavior.
  int safe_browsing_check_delay_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
