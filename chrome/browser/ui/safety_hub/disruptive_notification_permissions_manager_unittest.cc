// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using RevocationResult =
    DisruptiveNotificationPermissionsManager::RevocationResult;
using testing::ElementsAre;
using testing::IsEmpty;

constexpr char kRevocationResultHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevocationResult";
constexpr char kNotificationCountHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.Proposed."
    "NotificationCount";
constexpr char kRevokedWebsitesCountHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevokedWebsitesCount";

class SafetyHubNotificationWrapperForTesting
    : public DisruptiveNotificationPermissionsManager::
          SafetyHubNotificationWrapper {
 public:
  SafetyHubNotificationWrapperForTesting(std::vector<int>& display_called_with,
                                         std::vector<int>& update_called_with)
      : display_called_with_(display_called_with),
        update_called_with_(update_called_with) {}

  void DisplayNotification(int num_revoked_permissions) override {
    display_called_with_->push_back(num_revoked_permissions);
  }
  void UpdateNotification(int num_revoked_permissions) override {
    update_called_with_->push_back(num_revoked_permissions);
  }

 private:
  base::raw_ref<std::vector<int>> display_called_with_;
  base::raw_ref<std::vector<int>> update_called_with_;
};

}  // namespace

class DisruptiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<DisruptiveNotificationPermissionsManager>(
        hcsm(), site_engagement_service());
    manager_->SetNotificationWrapperForTesting(
        std::make_unique<SafetyHubNotificationWrapperForTesting>(
            display_notification_function_called_with_,
            update_notification_function_called_with_));
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  site_engagement::SiteEngagementService* site_engagement_service() {
    return site_engagement::SiteEngagementServiceFactory::GetForProfile(
        &profile_);
  }

  void SetNotificationPermission(GURL url, ContentSetting setting) {
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
  }

  void SetDailyAverageNotificationCount(GURL url, int daily_average_count) {
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(&profile_);
    notifications_engagement_service->RecordNotificationDisplayed(
        url, daily_average_count * 7);
  }

  int GetRevokedPermissionsCount() {
    return hcsm()
        ->GetSettingsForOneType(
            ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS)
        .size();
  }

  const std::vector<int>& GetDisplayNotificationFunctionCalledWith() {
    return display_notification_function_called_with_;
  }

  const std::vector<int>& GetUpdateNotificationFunctionCalledWith() {
    return update_notification_function_called_with_;
  }

  DisruptiveNotificationPermissionsManager* manager() { return manager_.get(); }

  TestingProfile* profile() { return &profile_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;

  std::vector<int> display_notification_function_called_with_;
  std::vector<int> update_notification_function_called_with_;
  std::unique_ptr<DisruptiveNotificationPermissionsManager> manager_;
};

class DisruptiveNotificationPermissionsManagerRevocationTest
    : public DisruptiveNotificationPermissionsManagerTest {
 public:
  DisruptiveNotificationPermissionsManagerRevocationTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kSafetyHubDisruptiveNotificationRevocation,
        {
            {safe_browsing::kSafetyHubDisruptiveNotificationRevocationShadowRun
                 .name,
             "false"},
        });
  }
};

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokeDisruptivePermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  base::Value::Dict dict = std::move(stored_value).TakeDict();
  EXPECT_FALSE(
      dict.FindBool(safety_hub::kHasReportedMetricsStr).value_or(false));
  EXPECT_EQ(0.0, dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0));
  EXPECT_EQ(3,
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 1);
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), IsEmpty());

  // On the next run, site goes from proposed to actual revocation.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), ElementsAre(1));

  // After that, no new metrics are reported since there is no notification
  // content setting exception.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), ElementsAre(1));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokedWebsitesCount) {
  base::HistogramTester t;

  GURL first_url("https://www.example.com");
  SetNotificationPermission(first_url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(first_url, 3);
  site_engagement_service()->ResetBaseScoreForURL(first_url, 0);

  GURL second_url("https://www.chrome.com");
  SetNotificationPermission(second_url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(second_url, 1);
  site_engagement_service()->ResetBaseScoreForURL(second_url, 0);

  GURL third_url("https://www.anothersite.com");
  SetNotificationPermission(third_url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(third_url, 3);
  site_engagement_service()->ResetBaseScoreForURL(third_url, 0);

  manager()->RevokeDisruptiveNotifications();

  EXPECT_EQ(GetRevokedPermissionsCount(), 2);

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 2);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 2, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 2);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DontRevokePermissionHighEngagement) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 90);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(stored_value.is_none());

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DontRevokePermissionLowNotificationCount) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 1);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(stored_value.is_none());

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DontRevokePermissionZeroNotificationCount) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  // No notification engagement entry by default.
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();

  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(stored_value.is_none());

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NotEligibleNotificationContentSettings) {
  base::HistogramTester t;
  // Already blocked notification.
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_BLOCK);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotAllowedContentSetting, 1);

  // Broad content setting.
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("[*.]example.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotSiteScopedContentSetting, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ManagedContentSetting) {
  base::HistogramTester t;
  content_settings::TestUtils::OverrideProvider(
      hcsm(), std::make_unique<content_settings::MockProvider>(),
      content_settings::ProviderType::kPolicyProvider);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kManagedContentSetting, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NotRevokedDefaultBlock) {
  base::HistogramTester t;
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNoRevokeDefaultBlock, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NotDisruptiveDefaultBlock) {
  base::HistogramTester t;
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 1);
  site_engagement_service()->ResetBaseScoreForURL(url, 5.0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ProposedMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url("https://chrome.test/");
  const int kDailyNotificationCount = 4;

  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kProposedStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, kDailyNotificationCount);
  auto constraint =
      content_settings::ContentSettingConstraints(base::Time::Now());
  constraint.set_lifetime(base::Days(30));
  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraint);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(url));

  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  // Check that the correct metric is reported.
  auto entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.Proposed");
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(entries[0], "DailyAverageVolume",
                                 kDailyNotificationCount);
  ukm_recorder.ExpectEntryMetric(entries[0], "SiteEngagement", 0.0);

  // After the metric is reported, has_reported_metrics flag is set.
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  ASSERT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict()
                  .FindBool(safety_hub::kHasReportedMetricsStr)
                  .value_or(false));
  EXPECT_EQ(info.metadata.expiration(), constraint.expiration());

  // UKM is reported once per site.
  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  EXPECT_EQ(1u, ukm_recorder
                    .GetEntriesByName(
                        "SafetyHub.DisruptiveNotificationRevocations.Proposed")
                    .size());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RegrantPermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");

  // Set up a revoked disruptive notification.
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // Set up a revoked content setting.
  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  dict.Set(safety_hub::kTimestampStr, base::TimeToValue(base::Time::Now()));

  content_settings::ContentSettingConstraints constraint(base::Time::Now());
  constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)), constraint);

  manager()->RegrantPermissionForUrl(url);
  // Notifications are again allowed.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // The notification count has been updated.
  EXPECT_THAT(GetUpdateNotificationFunctionCalledWith(), ElementsAre(1, 0));

  // The content setting was updated to "ignore" to prevent autorevoking in the
  // future.
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  EXPECT_EQ(safety_hub::kIgnoreStr,
            stored_value.GetDict()
                .Find(safety_hub::kRevokedStatusDictKeyStr)
                ->GetString());
  // The constraint was also reset to not expire.
  EXPECT_TRUE(info.metadata.lifetime().is_zero());

  manager()->RevokeDisruptiveNotifications();
  // The site is reported as ignored for revocation and not revoked.
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kIgnore, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NoRegrantPermissionMissingContentSetting) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  manager()->RegrantPermissionForUrl(url);
  // Notifications are still ask.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       UndoRegrantPermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");

  // Set up a disruptive notification.
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // Set up an ignored value.
  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kIgnoreStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  dict.Set(safety_hub::kTimestampStr, base::TimeToValue(base::Time::Now()));

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)));

  // Undo the regrant (return to revoked state).
  content_settings::ContentSettingConstraints constraint(base::Time::Now());
  constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::NOTIFICATIONS}, constraint.Clone());

  // Notifications are again ask.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // The content setting was updated to "revoke".
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  content_settings::SettingInfo info;
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS, &info);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  dict = std::move(stored_value).TakeDict();
  EXPECT_EQ(safety_hub::kRevokeStr,
            dict.Find(safety_hub::kRevokedStatusDictKeyStr)->GetString());
  EXPECT_EQ(0.0, dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0));
  EXPECT_EQ(3,
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0));
  const base::Value* stored_timestamp = dict.Find(safety_hub::kTimestampStr);
  EXPECT_EQ(base::Time::Now(),
            base::ValueToTime(stored_timestamp).value_or(base::Time()));

  // The constraint was set.
  EXPECT_EQ(info.metadata.lifetime(), safety_hub_util::GetCleanUpThreshold());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NoUndoRegrantPermissionMissingContentSetting) {
  base::HistogramTester t;
  GURL url("https://www.example.com");

  // Set up a disruptive notification.
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // Attempt to undo the regrant (return to revoked state).
  content_settings::ContentSettingConstraints constraint(base::Time::Now());
  constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::NOTIFICATIONS}, constraint.Clone());

  // Notifications are still allow because there were no "ignore" value stored
  // therefore no revocation to undo.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NoUndoRegrantPermissionWrongContentSettingType) {
  base::HistogramTester t;
  GURL url("https://www.example.com");

  // Set up a disruptive notification.
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // Set up an ignored value.
  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kIgnoreStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  dict.Set(safety_hub::kTimestampStr, base::TimeToValue(base::Time::Now()));

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)));

  // Attempt to undo the regrant (return to revoked state).
  content_settings::ContentSettingConstraints constraint(base::Time::Now());
  constraint.set_lifetime(safety_hub_util::GetCleanUpThreshold());
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::GEOLOCATION}, constraint.Clone());

  // Notifications are still allow because there were no "ignore" value stored
  // therefore no revocation to undo.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ClearRevokedPermissionsList) {
  base::HistogramTester t;

  // Set up a revoked permission.
  GURL revoked_url("https://www.example1.com");
  base::Value::Dict revoked_dict;
  revoked_dict.Set(safety_hub::kRevokedStatusDictKeyStr,
                   safety_hub::kRevokeStr);
  revoked_dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  revoked_dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  revoked_dict.Set(safety_hub::kTimestampStr,
                   base::TimeToValue(base::Time::Now()));

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(revoked_url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(revoked_dict)));

  // Set up a proposed permission.
  GURL proposed_url("https://www.example2.com");
  base::Value::Dict proposed_dict;
  proposed_dict.Set(safety_hub::kRevokedStatusDictKeyStr,
                    safety_hub::kProposedStr);
  proposed_dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  proposed_dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  proposed_dict.Set(safety_hub::kTimestampStr,
                    base::TimeToValue(base::Time::Now()));

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(proposed_url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(proposed_dict)));

  // Set up an ignored permission.
  GURL ignored_url("https://www.example3.com");
  base::Value::Dict ignored_dict;
  ignored_dict.Set(safety_hub::kRevokedStatusDictKeyStr,
                   safety_hub::kIgnoreStr);
  ignored_dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  ignored_dict.Set(safety_hub::kDailyNotificationCountStr, 3);
  ignored_dict.Set(safety_hub::kTimestampStr,
                   base::TimeToValue(base::Time::Now()));

  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(ignored_url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(ignored_dict)));

  EXPECT_EQ(GetRevokedPermissionsCount(), 3);
  manager()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsCount(), 2);

  // Only revoked value is cleared, others are not affected.
  base::Value revoked_stored_value = hcsm()->GetWebsiteSetting(
      revoked_url, revoked_url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(revoked_stored_value.is_none());

  base::Value proposed_stored_value = hcsm()->GetWebsiteSetting(
      proposed_url, proposed_url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_FALSE(proposed_stored_value.is_none());

  base::Value ignored_stored_value = hcsm()->GetWebsiteSetting(
      ignored_url, ignored_url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_FALSE(ignored_stored_value.is_none());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       UpdateNotificationContentSettingsChanged) {
  GURL url("https://chrome.test/");
  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kRevokeStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, 4);
  dict.Set(safety_hub::kTimestampStr,
           base::TimeToValue(base::Time::Now() - base::Days(3)));
  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)));
  EXPECT_THAT(GetUpdateNotificationFunctionCalledWith(), ElementsAre(1));
}

class DisruptiveNotificationPermissionsManagerShadowRunTest
    : public DisruptiveNotificationPermissionsManagerTest {
 public:
  DisruptiveNotificationPermissionsManagerShadowRunTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kSafetyHubDisruptiveNotificationRevocation,
        {
            {safe_browsing::kSafetyHubDisruptiveNotificationRevocationShadowRun
                 .name,
             "true"},
        });
  }
};

TEST_F(DisruptiveNotificationPermissionsManagerShadowRunTest,
       ProposeRevokeDisruptivePermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  base::Value::Dict dict = std::move(stored_value).TakeDict();
  EXPECT_FALSE(
      dict.FindBool(safety_hub::kHasReportedMetricsStr).value_or(false));
  EXPECT_EQ(0.0, dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0));
  EXPECT_EQ(3,
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 1);

  // Repeated runs during the shadow run don't revoke the notification but
  // report that the site is already in proposed revocation list instead.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kAlreadyInProposedRevokeList, 1);

  // The shadow run should never display notifications.
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), IsEmpty());
}
