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
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

using RevocationResult =
    DisruptiveNotificationPermissionsManager::RevocationResult;

namespace {

constexpr char kRevocationResultHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevocationResult";
constexpr char kNotificationCountHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.Proposed."
    "NotificationCount";
constexpr char kRevokedWebsitesCountHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.RevokedWebsitesCount";
constexpr char kFalsePositiveSiteEngagementHistogram[] =
    "Settings.SafetyHub.DisruptiveNotificationRevocations.FalsePositive."
    "SiteEngagement";

}  // namespace

class DisruptiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    manager_ = std::make_unique<DisruptiveNotificationPermissionsManager>(
        hcsm(), site_engagement_service());
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

  DisruptiveNotificationPermissionsManager* manager() { return manager_.get(); }

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;

  std::unique_ptr<DisruptiveNotificationPermissionsManager> manager_;
};

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       RevokeDisruptivePermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();

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

  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 1);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kAlreadyInRevokeList, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest, RevokedWebsitesCount) {
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

  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 2);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 2, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 2);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
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

TEST_F(DisruptiveNotificationPermissionsManagerTest,
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

TEST_F(DisruptiveNotificationPermissionsManagerTest,
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

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       NotEligableNotificationContentSettings) {
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

TEST_F(DisruptiveNotificationPermissionsManagerTest, ManagedContentSetting) {
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

TEST_F(DisruptiveNotificationPermissionsManagerTest, NotRevokedDefaultBlock) {
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

TEST_F(DisruptiveNotificationPermissionsManagerTest,
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

TEST_F(DisruptiveNotificationPermissionsManagerTest, FalsePositivePermission) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  // Permission was proposed for revocation.
  content_settings::SettingInfo proposed_info;
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      &proposed_info);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  base::Value::Dict dict = std::move(stored_value).TakeDict();
  EXPECT_EQ(safety_hub::kProposedStr,
            dict.Find(safety_hub::kRevokedStatusDictKeyStr)->GetString());
  EXPECT_FALSE(
      dict.FindBool(safety_hub::kHasReportedMetricsStr).value_or(false));
  EXPECT_EQ(0.0, dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0));
  EXPECT_EQ(3,
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0));
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);

  // After that the site engagement score has increased.
  site_engagement_service()->ResetBaseScoreForURL(url, 5.0);
  manager()->RevokeDisruptiveNotifications();
  // Verify that the permission was marked as a false positive.
  content_settings::SettingInfo false_positive_info;
  stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      &false_positive_info);
  EXPECT_FALSE(stored_value.is_none());
  ASSERT_TRUE(stored_value.is_dict());
  dict = std::move(stored_value).TakeDict();
  EXPECT_EQ(safety_hub::kFalsePositiveStr,
            dict.Find(safety_hub::kRevokedStatusDictKeyStr)->GetString());
  EXPECT_FALSE(
      dict.FindBool(safety_hub::kHasReportedMetricsStr).value_or(false));
  EXPECT_EQ(0.0, dict.FindDouble(safety_hub::kSiteEngagementStr).value_or(0));
  EXPECT_EQ(3,
            dict.FindInt(safety_hub::kDailyNotificationCountStr).value_or(0));
  // Verify that after updating the content setting value expiration didn't
  // change.
  EXPECT_EQ(false_positive_info.metadata.expiration(),
            proposed_info.metadata.expiration());

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kFalsePositive, 1);
  t.ExpectBucketCount(kFalsePositiveSiteEngagementHistogram, 5, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest, ProposedMetrics) {
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

TEST_F(DisruptiveNotificationPermissionsManagerTest, FalsePositiveMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url("https://chrome.test/");
  const int kDailyNotificationCount = 4;

  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kFalsePositiveStr);
  dict.Set(safety_hub::kSiteEngagementStr, 1.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, kDailyNotificationCount);
  dict.Set(safety_hub::kHasReportedMetricsStr, true);
  dict.Set(safety_hub::kTimestampStr,
           base::TimeToValue(base::Time::Now() - base::Days(3)));
  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)));

  site_engagement_service()->ResetBaseScoreForURL(url, 5.0);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(url));

  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  // False positive entry was removed.
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(stored_value.is_none());

  // Check that the correct metric is reported.
  auto entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositive");
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(entries[0], "DaysSinceRevocation", 3);
  ukm_recorder.ExpectEntryMetric(entries[0], "NewSiteEngagement", 5.0);
  ukm_recorder.ExpectEntryMetric(entries[0], "OldSiteEngagement", 1.0);
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       ProposedFalsePositiveMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");
  const int kDailyNotificationCount = 4;

  base::Value::Dict dict;
  dict.Set(safety_hub::kRevokedStatusDictKeyStr, safety_hub::kFalsePositiveStr);
  dict.Set(safety_hub::kSiteEngagementStr, 0.0);
  dict.Set(safety_hub::kDailyNotificationCountStr, kDailyNotificationCount);
  dict.Set(safety_hub::kTimestampStr,
           base::TimeToValue(base::Time::Now() - base::Days(3)));
  hcsm()->SetWebsiteSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
      base::Value(std::move(dict)));

  site_engagement_service()->ResetBaseScoreForURL(url, 5.0);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(url));

  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  // False positive entry was removed.
  base::Value stored_value = hcsm()->GetWebsiteSetting(
      url, url,
      ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS);
  EXPECT_TRUE(stored_value.is_none());

  // Check that the correct metrics are reported.
  auto proposed_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.Proposed");
  EXPECT_EQ(1u, proposed_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(proposed_entries[0], url);
  ukm_recorder.ExpectEntryMetric(proposed_entries[0], "DailyAverageVolume",
                                 kDailyNotificationCount);
  ukm_recorder.ExpectEntryMetric(proposed_entries[0], "SiteEngagement", 0.0);

  auto false_positive_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositive");
  EXPECT_EQ(1u, false_positive_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(false_positive_entries[0], url);
  ukm_recorder.ExpectEntryMetric(false_positive_entries[0],
                                 "DaysSinceRevocation", 3);
  ukm_recorder.ExpectEntryMetric(false_positive_entries[0], "NewSiteEngagement",
                                 5.0);
  ukm_recorder.ExpectEntryMetric(false_positive_entries[0], "OldSiteEngagement",
                                 0.0);
}
