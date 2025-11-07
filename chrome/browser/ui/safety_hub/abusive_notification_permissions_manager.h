// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"

class GURL;
class Profile;

namespace {
// Maximum time in milliseconds to wait for the Safe Browsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
inline constexpr int kCheckUrlTimeoutMs = 5000;

// Key of the base::Value dictionary we assign to the
// REVOKED_ABUSIVE_NOTIFICATION_PERMISSION to specify revocation reason.
inline constexpr char kAbusiveRevocationSourceKeyStr[] = "revocation_source";
inline constexpr char kSocialEngineeringBlocklistStr[] = "social_engineering";
inline constexpr char kSafeBrowsingUnwantedRevocationStr[] = "manual";
inline constexpr char kSuspiciousContentAutoRevocationStr[] =
    "suspicious_content";
}  // namespace

namespace safe_browsing {
struct ThreatMetadata;
}  // namespace safe_browsing

// This class keeps track of abusive notification permissions by checking URLs
// against the Safe Browsing social engineering blocklist. This also handles
// automatic revocation and responding to user decisions in Safety Hub.
class AbusiveNotificationPermissionsManager {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(AbusiveNotificationPermissionsInteractions)
  enum class AbusiveNotificationPermissionsInteractions {
    kOpenReviewUI = 0,
    kAllowAgain = 1,
    kAcknowledgeAll = 2,
    kUndoAllowAgain = 3,
    kUndoAcknowledgeAll = 4,
    kMinimize = 5,
    kGoToSettings = 6,
    kMaxValue = kGoToSettings,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:SafetyCheckUnusedSitePermissionsModuleInteractions)

  explicit AbusiveNotificationPermissionsManager(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      scoped_refptr<HostContentSettingsMap> hcsm,
      PrefService* pref_service);

  AbusiveNotificationPermissionsManager(
      const AbusiveNotificationPermissionsManager&) = delete;
  AbusiveNotificationPermissionsManager& operator=(
      const AbusiveNotificationPermissionsManager&) = delete;

  ~AbusiveNotificationPermissionsManager();

  // Handles the auto-revocation of abusive notification permissions for both
  // social engineering blocklist and manual enforcement. This includes updating
  // the settings with the correct expiration, and logging metrics.
  static void ExecuteAbusiveNotificationAutoRevocation(
      HostContentSettingsMap* hcsm,
      GURL url,
      safe_browsing::NotificationRevocationSource revocation_source,
      const raw_ptr<const base::Clock> clock);

  // Sets the `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` value for a url,
  // revocation source, given the constraints, and whether the user wants to
  // ignore future auto-revocation.
  static void SetRevokedAbusiveNotificationPermission(
      HostContentSettingsMap* hcsm,
      GURL url,
      bool is_ignored,
      safe_browsing::NotificationRevocationSource revocation_source,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Perform `SetRevokedAbusiveNotificationPermission`, preserving revocation
  // source if exists.
  static void SetRevokedAbusiveNotificationPermission(
      HostContentSettingsMap* hcsm,
      GURL url,
      bool is_ignored,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Revoke notification permission for `url` if suspicious notification
  // criteria are met, setting `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`.
  // Return true if the notification has been revoked.
  static bool MaybeRevokeSuspiciousNotificationPermission(Profile* profile,
                                                          GURL url);

  // Return `NotificationRevocationSource` if there is  a
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting value for the
  // `setting_url` with the `safety_hub::kAbusiveRevocationSourceKeyStr`.
  // Returns `NotificationRevocationSource::kUnknown` if the setting does not
  // exist, the key does not exist, or the value is invalid.
  static safe_browsing::NotificationRevocationSource
  GetRevokedAbusiveNotificationRevocationSource(HostContentSettingsMap* hcsm,
                                                GURL url);

  // Returns true if `url` belongs to a site with revoked abusive notifications
  // with `NotificationRevocationSource::kSuspiciousContentAutoRevocation` as
  // notification revocation source.
  static bool IsUrlRevokedDueToSuspiciousContent(HostContentSettingsMap* hcsm,
                                                 GURL url);

  // Calls `PerformSafeBrowsingChecks` on URLs which have notifications
  // enabled and haven't been marked as a URL to be ignored.
  void CheckNotificationPermissionOrigins();

  // If the url has a revoked abusive notification permission, this method
  // allows notification permissions again and adds a constraint so that this
  // permission is not auto-revoked during future Safety Hub checks.
  void RegrantPermissionForOriginIfNecessary(const GURL& url);

  // If `permission_types` includes notifications, undo the actions from
  // `RegrantPermissionForOrigin` by changing the `NOTIFICATIONS` setting back
  // to `CONTENT_SETTING_ASK` and the
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting back to a dictionary
  // with `safety_hub::kRevokedStatusDictKeyStr` set to
  // `safety_hub::kRevokeStr`.
  void UndoRegrantPermissionForOriginIfNecessary(
      const GURL& url,
      std::set<ContentSettingsType> permission_types,
      content_settings::ContentSettingConstraints constraints);

  // Clear the list of abusive notification permissions so they will no longer
  // be shown to the user. Does not change permissions themselves.
  void ClearRevokedPermissionsList();

  // Remove the `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting for the given
  // pattern pairs.
  void DeletePatternFromRevokedAbusiveNotificationList(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern);

  // Log metric for permission changed and remove
  // `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting for the given pattern
  // pairs.
  void OnPermissionChanged(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern);

  // Restores REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS entry for the
  // primary_pattern after it was deleted after user
  // has accepted the revocation (via `ClearRevokedPermissionsList()`).
  void RestoreDeletedRevokedPermission(
      const ContentSettingsPattern& primary_pattern,
      content_settings::ContentSettingConstraints constraints);

  // If there's a clock for testing, return that. Otherwise, return an instance
  // of a default clock.
  const base::Clock* GetClock();

  // Returns true if settings are being changed by
  // `AbusiveNotificationPermissionManager` activities and should be ignored by
  // `RevokedPermissionsService::OnContentSettingChanged`.
  bool IsRevocationRunning();

  // Helper method for logging the
  // `AbusiveNotificationPermissionRevocation.Interactions` UKM.
  void LogAbusiveNotificationPermissionRevocationUKM(
      const GURL& origin,
      AbusiveNotificationPermissionsInteractions interaction,
      safe_browsing::NotificationRevocationSource source);

  // Test support:
  // TODO(crbug/342210522): Use a unique_ptr here if possible.
  void SetClockForTesting(base::Clock* clock) { clock_for_testing_ = clock; }

 private:
  friend class AbusiveNotificationPermissionsManagerTest;
  friend class AbusiveNotificationPermissionsRevocationTest;
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
                           SetRevokedAbusiveNotificationPermission);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           SetIgnoreRevokedAbusiveNotificationPermission);
  FRIEND_TEST_ALL_PREFIXES(AbusiveNotificationPermissionsManagerTest,
                           UndoRegrantPermissionForOriginIfNecessary);

  // On object creation, checks the Safe Browsing blocklist for `url_`
  // and revokes notification permissions if blocklisted.
  // This subclass is necessary to avoid concurrency issues - each
  // request needs its own Client and the AbusiveNotificationPermissionsManager
  // makes multiple Safe Browsing requests.
  class SafeBrowsingCheckClient
      : safe_browsing::SafeBrowsingDatabaseManager::Client {
   public:
    SafeBrowsingCheckClient(
        base::PassKey<safe_browsing::SafeBrowsingDatabaseManager::Client>
            pass_key,
        safe_browsing::SafeBrowsingDatabaseManager* database_manager,
        raw_ptr<std::map<SafeBrowsingCheckClient*,
                         std::unique_ptr<SafeBrowsingCheckClient>>>
            safe_browsing_request_clients,
        raw_ptr<HostContentSettingsMap> hcsm,
        PrefService* pref_service,
        GURL url,
        int safe_browsing_check_delay,
        const base::Clock* clock);

    ~SafeBrowsingCheckClient() override;

    // Trigger the call to check the Safe Browsing social engineering blocklist.
    void CheckSocialEngineeringBlocklist();

   private:
    // safe_browsing::SafeBrowsingDatabaseManager::Client:
    void OnCheckBrowseUrlResult(
        const GURL& url,
        safe_browsing::SBThreatType threat_type,
        const safe_browsing::ThreatMetadata& metadata) override;

    // Callback to be run if a Safe Browsing blocklist request does not return
    // a response within `kCheckUrlTimeoutMs` time.
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

    // A pointer to the `pref_service_` of the
    // `AbusiveNotificationPermissionsManager`.
    raw_ptr<PrefService> pref_service_;

    // The URL that is being checked against the Safe Browsing blocklist.
    GURL url_;

    // Delay amount allowed for blocklist checks.
    int safe_browsing_check_delay_;

    // Timer for running Safe Browsing blocklist checks. If `kCheckUrlTimeoutMs`
    // time has passed, run `OnCheckBlocklistTimeout`.
    base::OneShotTimer timer_;

    // To enable automatic cleanup after the threshold has passed, this is used
    // to set the lifetime of the `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`
    // value.
    const raw_ptr<const base::Clock> clock_;

    base::WeakPtrFactory<
        AbusiveNotificationPermissionsManager::SafeBrowsingCheckClient>
        weak_factory_{this};
  };

  // Primarily used for tests.
  void SetNullSBCheckDelayForTesting() { safe_browsing_check_delay_ = 0; }

  // Create a `SafeBrowsingCheckClient` object, triggering a blocklist check,
  // and add it to `safe_browsing_request_clients_`.
  void PerformSafeBrowsingChecks(GURL url);

  // Returns true if the notification permission is allowed and the setting
  // does not indicate "ignore".
  bool ShouldCheckOrigin(const ContentSettingPatternSource& setting) const;

  // Clears any helper members that stored state from the previous safety check.
  // Called each time Safe Browsing checks are performed on a set of URLs.
  void ResetSafeBrowsingCheckHelpers();

  // Update Notification permission setting in response to Safety Hub actions
  // (e.g. re-grant and undo re-grant). This will set
  // `is_abusive_site_revocation_running_` to signal
  // `RevokedPermissionsService::OnContentSettingChanged` to ignore the setting
  // change.
  void UpdateNotificationPermissionForSafetyHubAction(
      HostContentSettingsMap* hcsm,
      GURL url,
      ContentSetting setting_value);

  // Convert `NotificationRevocationSource` to its string representation for
  // storing in `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`.
  static std::optional<std::string> GetRevocationSourceString(
      safe_browsing::NotificationRevocationSource source);

  // Used for interactions with the local database, when checking the blocklist.
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  // Object that allows us to manage site permissions.
  scoped_refptr<HostContentSettingsMap> hcsm_;

  // Used for updating prefs after performing blocklist checks.
  raw_ptr<PrefService> pref_service_;

  // Safe Browsing blocklist check clients. Each object is responsible for a
  // single Safe Browsing check, given a URL. Stored this way so that the object
  // can delete itself from this map within the SafeBrowsingCheckClient.
  std::map<SafeBrowsingCheckClient*, std::unique_ptr<SafeBrowsingCheckClient>>
      safe_browsing_request_clients_;

  // Length of time allowed for Safe Browsing check before timeout. This allows
  // us to test timeout behavior.
  int safe_browsing_check_delay_;

  // To enable automatic cleanup after the threshold has passed, this is used to
  // set the lifetime of the `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` value.
  // Pass this into each instance of the SafeBrowsingCheckClient class.
  raw_ptr<base::Clock> clock_for_testing_;

  // True if automatic check is occurring or
  // `AbusiveNotificationPermissionManager` is changing notification setting.
  // This value is used to help decide whether to handle setting change inside
  // `OnContentSettingChanged`.
  bool is_abusive_site_revocation_running_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_ABUSIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
