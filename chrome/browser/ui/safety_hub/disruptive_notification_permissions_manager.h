// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;
class Profile;

namespace site_engagement {
class SiteEngagementService;
}  // namespace site_engagement

// This class keeps track of disruptive notification permissions by checking the
// average daily notification counts and site engagement score.
class DisruptiveNotificationPermissionsManager {
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
  // False positives: when the increase in site engagement score is detected for
  // proposed revocation, it's reported as kFalsePositive. In the next runs
  // (until a notification is shown and metrics are reported), the site is
  // reported as kAlreadyFalsePositive.
  //
  // Actual revocation: the site first is marked for revocation (returns
  // kProposedRevoke) and then the permission is actually revoked (return
  // kRevoke). After the permission is revoked, the content setting is removed
  // so the site won't be reported anymore.
  //
  // LINT.IfChange(RevocationResult)
  enum class RevocationResult {
    kNotAllowedContentSetting = 0,
    kInvalidContentSetting = 1,
    kNotSiteScopedContentSetting = 2,
    kManagedContentSetting = 3,
    kAlreadyInProposedRevokeList = 4,
    kFalsePositive = 5,
    kNotDisruptive = 6,
    kProposedRevoke = 7,
    kNoRevokeDefaultBlock = 8,
    kAlreadyFalsePositive = 9,
    kRevoke = 10,
    kMaxValue = kRevoke,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/settings/enums.xml:DisruptiveNotificationRevocationResult)

  explicit DisruptiveNotificationPermissionsManager(
      scoped_refptr<HostContentSettingsMap> hcsm,
      site_engagement::SiteEngagementService* site_engagement_service);

  DisruptiveNotificationPermissionsManager(
      const DisruptiveNotificationPermissionsManager&) = delete;
  DisruptiveNotificationPermissionsManager& operator=(
      const DisruptiveNotificationPermissionsManager&) = delete;

  ~DisruptiveNotificationPermissionsManager();

  // Revokes notification permissions for disruptive sites and records
  // the revoked websites in the content setting.
  void RevokeDisruptiveNotifications();

  // Returns the list of revoked disruptive notifications, excluding proposed
  // revocations and false positives.
  ContentSettingsForOneType GetRevokedNotifications();

  // Returns true if settings are being changed due to auto revocation;
  bool IsRevocationRunning();

  // Logs metrics for proposed disruptive notification revocation, to be called
  // when displaying a persistent notification.
  static void LogMetrics(Profile* profile,
                         const GURL& url,
                         ukm::SourceId source_id);

  // Test support:
  void SetClockForTesting(base::Clock* clock);

 private:
  // Process existing content setting value: record false positive, revoke
  // notifications or report the site as already in the proposed revocation
  // list.
  void HandleExistingValue(const GURL& url,
                           base::Value stored_value,
                           const content_settings::SettingInfo& info);

  // Updates the content setting to false positive and reports metrics.
  void RecordFalsePositive(const GURL& url,
                           base::Value::Dict dict,
                           const content_settings::SettingInfo& info,
                           double new_score);

  // Revokes notification permission, updates the content setting value to
  // revoke and reports metrics.
  void RevokeNotifications(const GURL& url, base::Value::Dict dict);

  // Whether the notification is disruptive based on the site engagement score
  // for the URL and the daily average notification count.
  bool IsNotificationDisruptive(const GURL& url, int daily_notification_count);

  // Stores the URL in REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS content
  // setting with |constraints|. The content setting value is a dictionary.
  // "revoked_status" key value depends on whether the revocation will actually
  // be performed or only proposed as part of shadow run.
  void StoreRevokedDisruptiveNotificationPermission(
      const GURL& url,
      const content_settings::ContentSettingConstraints& constraints,
      int daily_notification_count);

  scoped_refptr<HostContentSettingsMap> hcsm_;

  raw_ptr<site_engagement::SiteEngagementService> site_engagement_service_;

  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();

  // Returns true if the revocation of disruptive notification
  // permissions is happening.
  bool is_revocation_running_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_DISRUPTIVE_NOTIFICATION_PERMISSIONS_MANAGER_H_
