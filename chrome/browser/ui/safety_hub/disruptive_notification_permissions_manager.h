// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;
class Profile;

namespace site_engagement {
class SiteEngagementService;
}  // namespace site_engagement

// This class keeps track of disruptive notification permissions by checking the
// average daily notification counts and site engagement score.
class DisruptiveNotificationPermissionsManager
    : public content_settings::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // kNotAllowedContentSetting, kInvalidContentSetting,
  // kNotSiteScopedContentSetting, kManagedContentSetting, kNoRevokeDefaultBlock
  // are returned for sites where the permission cannot be revoked.
  //
  // Shadow run: the site is proposed for revocation (kProposedRevoke) and
  // returns kAlreadyInProposedRevokeList for all the next runs.
  //
  // Actual revocation: the site first is marked for revocation (returns
  // kProposedRevoke) and then the permission is actually revoked (return
  // kRevoke). After the permission is revoked, the content setting is removed
  // so the site won't be reported anymore.
  //
  // Undo: If the user has undone the revocation, the site is marked as "ignore"
  // so it won't be revoked on the next runs.
  //
  // LINT.IfChange(RevocationResult)
  enum class RevocationResult {
    kNotAllowedContentSetting = 0,
    kInvalidContentSetting = 1,
    kNotSiteScopedContentSetting = 2,
    kManagedContentSetting = 3,
    kAlreadyInProposedRevokeList = 4,
    // kFalsePositive = 5,  // deprecated, now reported as kNotDisruptive
    kNotDisruptive = 6,
    kProposedRevoke = 7,
    kNoRevokeDefaultBlock = 8,
    // kAlreadyFalsePositive = 9,  // deprecated, now reported as kNotDisruptive
    kRevoke = 10,
    kIgnore = 11,
    kMaxValue = kIgnore,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:DisruptiveNotificationRevocationResult)

  // TODO(crbug.com/406472515): Reevaluate if we should continue reporting false
  // positives for non persistent notification clicks. Non persistent
  // notifications are only shown when the site is visited so the site must be
  // visited first.
  //
  // The reason why a disruptive notification revocation was considered a false
  // positive. If the user interacts with a site after revocation, the
  // revocation was a false positive.
  //
  // LINT.IfChange(FalsePositiveReason)
  enum class FalsePositiveReason {
    kPageVisit = 0,
    kPersistentNotificationClick = 1,
    kNonPersistentNotificationClick = 2,
    kMaxValue = kNonPersistentNotificationClick,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:DisruptiveNotificationFalsePositiveReason)

  // LINT.IfChange(RevocationState)
  enum class RevocationState {
    kProposed = 1,
    kRevoked = 2,
    kIgnore = 3,
    kAcknowledged = 4,
    kMaxValue = kAcknowledged,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:DisruptiveNotificationRevocationState)

  class SafetyHubNotificationWrapper {
   public:
    virtual ~SafetyHubNotificationWrapper();
    virtual void DisplayNotification(int num_revoked_permissions) = 0;
    virtual void UpdateNotification(int num_revoked_permissions) = 0;
  };

  explicit DisruptiveNotificationPermissionsManager(
      scoped_refptr<HostContentSettingsMap> hcsm,
      site_engagement::SiteEngagementService* site_engagement_service);

  DisruptiveNotificationPermissionsManager(
      const DisruptiveNotificationPermissionsManager&) = delete;
  DisruptiveNotificationPermissionsManager& operator=(
      const DisruptiveNotificationPermissionsManager&) = delete;

  ~DisruptiveNotificationPermissionsManager() override;

  // Revokes notification permissions for disruptive sites and records
  // the revoked websites in the content setting.
  void RevokeDisruptiveNotifications();

  // Returns the list of revoked disruptive notifications, excluding proposed
  // revocations and false positives.
  ContentSettingsForOneType GetRevokedNotifications();

  // Returns true if settings are being changed due to auto revocation;
  bool IsRunning();

  // If the url has a revoked disruptive notification permission, this method
  // allows the notification permissions again and adds a constraint so that
  // this permission is not auto-revoked during future Safety Hub checks.
  void RegrantPermissionForUrl(const GURL& url);

  // If `permission_types` includes notifications, undo the actions from
  // `RegrantPermissionForUrl` by changing the `NOTIFICATIONS` setting back
  // to `CONTENT_SETTING_ASK` and the
  // `REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS` status value back to
  // `safety_hub::kRevokeStr`.
  void UndoRegrantPermissionForUrl(
      const GURL& url,
      std::set<ContentSettingsType> permission_types,
      content_settings::ContentSettingConstraints constraints);

  // Clear the list of revoked notification permissions so they will no longer
  // be shown to the user. Does not change permissions themselves.
  void ClearRevokedPermissionsList();

  // Restores REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS entry for the
  // primary_pattern after it was deleted after user has accepted the revocation
  // (via `ClearRevokedPermissionsList()`). Only restores the value if there is
  // a matching notification engagement entry.
  void RestoreDeletedRevokedPermission(
      const ContentSettingsPattern& primary_pattern,
      content_settings::ContentSettingConstraints constraints);

  // If the URL is in the revoke or proposed revoke list, report a false
  // positive and record metrics.
  static void MaybeReportFalsePositive(Profile* profile,
                                       const GURL& origin,
                                       FalsePositiveReason reason,
                                       ukm::SourceId source_id);

  // Logs metrics for proposed disruptive notification revocation, to be called
  // when displaying a persistent notification.
  static void LogMetrics(Profile* profile,
                         const GURL& url,
                         ukm::SourceId source_id);

  // Returns true if `url` has been revoked notification permissions because of
  // sending disruptive notifications.
  static bool IsUrlRevokedDisruptiveNotification(HostContentSettingsMap* hcsm,
                                                 const GURL& url);

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Test support:
  void SetClockForTesting(base::Clock* clock);
  void SetNotificationWrapperForTesting(
      std::unique_ptr<SafetyHubNotificationWrapper> wrapper);

 private:
  friend class DisruptiveNotificationPermissionsManagerTest;
  friend class RevokedPermissionsServiceBrowserTest;
  friend class RevokedPermissionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(
      PlatformNotificationServiceTest,
      ProposedDisruptiveNotificationRevocationMetricsPersistent);
  FRIEND_TEST_ALL_PREFIXES(
      PlatformNotificationServiceTest,
      ProposedDisruptiveNotificationRevocationMetricsNonPersistent);

  // A revocation entry as stored in content settings
  // (ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS).
  struct RevocationEntry {
    RevocationEntry(RevocationState revocation_state,
                    double site_engagement,
                    int daily_notification_count,
                    base::Time timestamp = base::Time::Now());
    RevocationEntry(const RevocationEntry& other);
    RevocationEntry& operator=(const RevocationEntry& other);
    ~RevocationEntry();

    bool operator==(const RevocationEntry& other) const;

    RevocationState revocation_state;
    double site_engagement;
    int daily_notification_count;

    // Timestamp of proposed or actual revocation.
    base::Time timestamp;

    bool has_reported_proposal = false;
    bool has_reported_false_positive = false;

    int page_visit_count = 0;
    int notification_click_count = 0;
  };

  // Helper class to manage content settings for
  // ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS.
  class ContentSettingHelper {
   public:
    explicit ContentSettingHelper(HostContentSettingsMap& hcsm);

    // Get/store/delete the `REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS`
    // setting.
    std::optional<RevocationEntry> GetRevocationEntry(const GURL& url);
    void PersistRevocationEntry(const GURL& url, const RevocationEntry& entry);
    void DeleteRevocationEntry(const GURL& url);
    std::vector<std::pair<GURL, RevocationEntry>> GetAllEntries();

   private:
    std::optional<RevocationEntry> ToRevocationEntry(
        const base::Value& value,
        const content_settings::RuleMetaData& metadata);

    base::raw_ref<HostContentSettingsMap> hcsm_;
  };

  // If the notifications should be revoked based on whether the metrics were
  // already reported or the cooldown period has run out.
  bool CanRevokeNotifications(const GURL& url,
                              const RevocationEntry& revocation_entry);

  // Revokes notification permission, updates the content setting value to
  // revoke and reports metrics.
  void RevokeNotifications(const GURL& url, RevocationEntry revocation_entry);

  // Whether the notification is disruptive based on the site engagement score
  // for the URL and the daily average notification count.
  bool IsNotificationDisruptive(const GURL& url, int daily_notification_count);

  // Displays the safety hub notification informing the users about revoked
  // notification permissions.
  void DisplayNotification();

  // Updates the count of revoked notification permissions in the safety hub
  // notification informing the users about revoked notification permissions.
  void UpdateNotificationCount();

  // Updates content settings for notification permissions.
  void UpdateNotificationPermission(const GURL& url,
                                    ContentSetting setting_value);

  // Ignores this url for future revocation and reports regrant metrics.
  void OnPermissionRegranted(const GURL& url,
                             RevocationEntry revocation_entry,
                             bool regranted_in_safety_hub);

  // Report metrics for the daily run.
  void ReportDailyRunMetrics();

  scoped_refptr<HostContentSettingsMap> hcsm_;

  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;

  // Observer to watch for content settings changed.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  bool is_revocation_running_ = false;

  // Track whether this service is responsible for changing notification
  // permissions, in order to ignore this case inside OnContentSettingChanged.
  bool is_changing_notification_permission_ = false;

  std::unique_ptr<SafetyHubNotificationWrapper> notification_wrapper_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
