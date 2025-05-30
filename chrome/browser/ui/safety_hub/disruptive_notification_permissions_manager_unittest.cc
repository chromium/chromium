// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
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
using FalsePositiveReason =
    DisruptiveNotificationPermissionsManager::FalsePositiveReason;

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::IsEmpty;
using testing::Not;
using testing::Optional;
using testing::Pair;

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
  using RevocationEntry =
      DisruptiveNotificationPermissionsManager::RevocationEntry;
  using ContentSettingHelper =
      DisruptiveNotificationPermissionsManager::ContentSettingHelper;
  using RevocationState =
      DisruptiveNotificationPermissionsManager::RevocationState;

  void SetUp() override {
    manager_ = std::make_unique<DisruptiveNotificationPermissionsManager>(
        hcsm(), site_engagement_service());
    manager_->SetNotificationWrapperForTesting(
        std::make_unique<SafetyHubNotificationWrapperForTesting>(
            display_notification_function_called_with_,
            update_notification_function_called_with_));
    manager_->SetClockForTesting(clock());
    clock()->SetNow(base::Time::Now());
  }

  void TearDown() override {
    manager_->SetClockForTesting(base::DefaultClock::GetInstance());
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

  void SetupFalsePositiveRevocation(GURL url,
                                    int days_since_revocation,
                                    RevocationState revocation_state) {
    const int kDailyNotificationCount = 4;

    RevocationEntry entry(
        /*revocation_state=*/revocation_state,
        /*site_engagement=*/0.0,
        /*daily_notification_count=*/kDailyNotificationCount,
        /*timestamp=*/base::Time::Now() - base::Days(days_since_revocation));
    ContentSettingHelper(*hcsm()).PersistRevocationEntry(url, entry);
  }

  DisruptiveNotificationPermissionsManager* manager() { return manager_.get(); }

  TestingProfile* profile() { return &profile_; }

  base::SimpleTestClock* clock() { return &clock_; }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;

  std::vector<int> display_notification_function_called_with_;
  std::vector<int> update_notification_function_called_with_;
  std::unique_ptr<DisruptiveNotificationPermissionsManager> manager_;
  base::SimpleTestClock clock_;
};

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       ContentSettingHelperCorrectLifetime) {
  GURL url("https://example.com");

  for (const auto& [revocation_state, expected_lifetime] :
       std::initializer_list<std::pair<RevocationState, base::TimeDelta>>{
           {RevocationState::kProposed, base::Days(0)},
           {RevocationState::kRevoked,
            content_settings::features::
                kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold
                    .Get()},
           {RevocationState::kIgnore, base::Days(0)},
           {RevocationState::kAcknowledged,
            content_settings::features::
                kSafetyCheckUnusedSitePermissionsRevocationCleanUpThreshold
                    .Get()},
       }) {
    ContentSettingHelper(*hcsm()).PersistRevocationEntry(
        url, RevocationEntry(
                 /*revocation_state=*/revocation_state,
                 /*site_engagement=*/0.0,
                 /*daily_notification_count=*/3));
    content_settings::SettingInfo info;
    base::Value stored_value = hcsm()->GetWebsiteSetting(
        url, url,
        ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
        &info);
    EXPECT_TRUE(stored_value.is_dict());
    EXPECT_EQ(info.metadata.lifetime(), expected_lifetime);
    ContentSettingHelper(*hcsm()).DeleteRevocationEntry(url);
    EXPECT_TRUE(
        hcsm()
            ->GetWebsiteSetting(url, url,
                                ContentSettingsType::
                                    REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
                                &info)
            .is_none());
  }
}

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       ProposedRevocationsWithWrongVersionAreIgnored) {
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kSafetyHubDisruptiveNotificationRevocation,
      {{features::kSafetyHubDisruptiveNotificationRevocationExperimentVersion
            .name,
        "1"}});

  GURL proposed_url("https://www.example1.com");
  GURL revoked_url("https://www.example2.com");
  GURL ignore_url("https://www.example3.com");
  RevocationEntry proposed_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  RevocationEntry revoked_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kRevoked,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  RevocationEntry ignore_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kIgnore,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(proposed_url,
                                                       proposed_entry);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(revoked_url,
                                                       revoked_entry);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(ignore_url,
                                                       ignore_entry);

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(revoked_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_url),
              Optional(Eq(ignore_entry)));

  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      features::kSafetyHubDisruptiveNotificationRevocation,
      {{features::kSafetyHubDisruptiveNotificationRevocationExperimentVersion
            .name,
        "2"}});

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Eq(std::nullopt));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(revoked_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_url),
              Optional(Eq(ignore_entry)));
}

class DisruptiveNotificationPermissionsManagerRevocationTest
    : public DisruptiveNotificationPermissionsManagerTest {
 public:
  DisruptiveNotificationPermissionsManagerRevocationTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSafetyHubDisruptiveNotificationRevocation,
        {{features::kSafetyHubDisruptiveNotificationRevocationShadowRun.name,
          "false"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationMinFalsePositiveCooldown
                  .name,
          "3"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationMaxFalsePositivePeriod
                  .name,
          "10"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationMinSiteEngagementScoreDelta
                  .name,
          "3.0"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationWaitingTimeAsProposed
                  .name,
          "1d"},
         {features::
              kSafetyHubDisruptiveNotificationRevocationWaitingForMetricsDays
                  .name,
          "7"}});
  }
};

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokeDisruptivePermission) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::has_reported_proposal, false)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::site_engagement, 0)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::daily_notification_count, 3)));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 1);

  clock()->Advance(base::Days(3));

  // Log metrics (happens when a notification is shown).
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(url));
  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  // On the next run, site goes from proposed to actual revocation.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Revoke.DaysSinceProposedRevocation",
      3, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "HasReportedMetricsBeforeRevocation",
      true, 1);

  // After that, no new metrics are reported since there is no notification
  // content setting exception.
  clock()->Advance(base::Days(1));
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.DailyDistribution."
      "Proposed.SiteEngagement0.DaysSinceRevocation",
      0, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.DailyDistribution."
      "Revoked.SiteEngagement0.DaysSinceRevocation",
      0, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.DailyDistribution."
      "Revoked.SiteEngagement0.DaysSinceRevocation",
      1, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DoNotRevokeDisruptivePermissionBeforeWaitingTime) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kProposed)));
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);

  // Log metrics (happens when a notification is shown).
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(url));
  DisruptiveNotificationPermissionsManager::LogMetrics(profile(), url,
                                                       source_id);

  // The waiting time of 1 day has not passed yet so the notification permission
  // won't be revoked.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kAlreadyInProposedRevokeList, 1);

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kProposed)));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokeDisruptivePermissionHaventReportedMetrics) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kProposed)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::has_reported_proposal, false)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::site_engagement, 0.0)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::daily_notification_count, 3)));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);

  // Metrics weren't yet reported so the notification won't be revoked.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kAlreadyInProposedRevokeList, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "HasReportedMetricsBeforeRevocation",
      false, 0);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "HasReportedMetricsBeforeRevocation",
      true, 0);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokeDisruptivePermissionAfterCooldown) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::has_reported_proposal, false)));
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);

  clock()->Advance(base::Days(10));

  // On the next run, site goes from proposed to actual revocation even without
  // metrics being reported because the waiting for metrics has expired.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 1);
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), ElementsAre(1));
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Revoke.DaysSinceProposedRevocation",
      10, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "HasReportedMetricsBeforeRevocation",
      false, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DoNotRevokeProposedPermissionWhichIsNotAnymoreDisruptive) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 3);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kProposed)));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 3, 1);
  EXPECT_THAT(GetDisplayNotificationFunctionCalledWith(), IsEmpty());

  site_engagement_service()->ResetBaseScoreForURL(url, 10);

  // On the next run, the revocation entry has been cleaned up because the site
  // is not disruptive anymore.
  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevocationResultHistogram, RevocationResult::kRevoke, 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
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

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));

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

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));

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

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));

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

  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/kDailyNotificationCount);

  ContentSettingHelper(*hcsm()).PersistRevocationEntry(url, entry);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

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

  // After the metric is reported, has_reported_proposal flag is set.
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::has_reported_proposal, true)));

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
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      url, RevocationEntry(
               /*revocation_state=*/RevocationState::kRevoked,
               /*site_engagement=*/0.0,
               /*daily_notification_count=*/3,
               /*timestamp=*/clock()->Now()));

  clock()->Advance(base::Days(5));

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
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);

  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kIgnore)));

  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "InSafetyHub.DaysSinceProposedRevocation",
      5, 1);
  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "InSafetyHub.NewSiteEngagement",
      0, 1);
  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "InSafetyHub.PreviousNotificationCount",
      3, 1);

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
  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kIgnore,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3,
      /*timestamp=*/base::Time::Now());

  ContentSettingHelper(*hcsm()).PersistRevocationEntry(url, entry);

  // Undo the regrant (return to revoked state).
  content_settings::ContentSettingConstraints constraints(base::Time::Now());
  constraints.set_lifetime(base::Days(30));
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::NOTIFICATIONS}, std::move(constraints));

  // Notifications are again ask.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // The content setting was updated to "revoke".
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);

  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kRevoked)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::site_engagement, 0.0)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::daily_notification_count, 3)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::timestamp, base::Time::Now())));
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
  content_settings::ContentSettingConstraints constraints(base::Time::Now());
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::NOTIFICATIONS}, std::move(constraints));

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
  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kIgnore,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3,
      /*timestamp=*/base::Time::Now());

  ContentSettingHelper(*hcsm()).PersistRevocationEntry(url, entry);

  // Attempt to undo the regrant (return to revoked state).
  content_settings::ContentSettingConstraints constraints(base::Time::Now());
  manager()->UndoRegrantPermissionForUrl(
      url, {ContentSettingsType::GEOLOCATION}, std::move(constraints));

  // Notifications are still allow because there were no "ignore" value stored
  // therefore no revocation to undo.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ClearAndRestoreRevokedPermissionsList) {
  base::HistogramTester t;

  // Set up a revoked permission.
  GURL revoked_url("https://www.example1.com");
  RevocationEntry revoked_entry(
      /*revocation_state=*/RevocationState::kRevoked,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(revoked_url,
                                                       revoked_entry);

  // Set up a proposed permission.
  GURL proposed_url("https://www.example2.com");
  RevocationEntry proposed_entry(
      /*revocation_state=*/RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(proposed_url,
                                                       proposed_entry);

  // Set up an ignored permission.
  GURL ignored_url("https://www.example3.com");
  RevocationEntry ignored_entry(
      /*revocation_state=*/RevocationState::kIgnore,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/3);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(ignored_url,
                                                       ignored_entry);

  EXPECT_EQ(GetRevokedPermissionsCount(), 3);
  manager()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsCount(), 3);

  RevocationEntry acknowledged_entry = revoked_entry;
  acknowledged_entry.revocation_state = RevocationState::kAcknowledged;

  // Only revoked value is set to acknowledged, others are not affected.
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(acknowledged_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_url),
              Optional(Eq(ignored_entry)));

  content_settings::ContentSettingConstraints constraints;
  manager()->RestoreDeletedRevokedPermission(
      ContentSettingsPattern::FromURLNoWildcard(revoked_url),
      constraints.Clone());

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(revoked_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_url),
              Optional(Eq(ignored_entry)));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       UpdateNotificationContentSettingsChanged) {
  GURL url("https://chrome.test/");
  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kRevoked,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4,
      /*timestamp=*/base::Time::Now() - base::Days(3));

  ContentSettingHelper(*hcsm()).PersistRevocationEntry(url, entry);
  EXPECT_THAT(GetUpdateNotificationFunctionCalledWith(), ElementsAre(1));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       SetIgnoreOnContentSettingsChanged) {
  for (auto [initial_state, new_content_setting, expected_state] :
       std::initializer_list<
           std::tuple<RevocationState, ContentSetting, RevocationState>>{
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_ALLOW,
            RevocationState::kIgnore},
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_BLOCK,
            RevocationState::kRevoked},
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_ASK,
            RevocationState::kRevoked},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_ALLOW,
            RevocationState::kProposed},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_BLOCK,
            RevocationState::kProposed},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_ASK,
            RevocationState::kProposed},
           {RevocationState::kIgnore, ContentSetting::CONTENT_SETTING_ALLOW,
            RevocationState::kIgnore},
           {RevocationState::kIgnore, ContentSetting::CONTENT_SETTING_BLOCK,
            RevocationState::kIgnore},
           {RevocationState::kIgnore, ContentSetting::CONTENT_SETTING_ASK,
            RevocationState::kIgnore},
       }) {
    GURL url("https://www.example1.com");
    ContentSettingHelper(*hcsm()).PersistRevocationEntry(
        url, RevocationEntry(
                 /*revocation_state=*/initial_state,
                 /*site_engagement=*/0.0,
                 /*daily_notification_count=*/3));
    SetNotificationPermission(url, new_content_setting);
    std::optional revocation_entry =
        ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
    EXPECT_THAT(
        revocation_entry,
        Optional(Field(&RevocationEntry::revocation_state, expected_state)));

    // Clean up.
    ContentSettingHelper(*hcsm()).DeleteRevocationEntry(url);
  }
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ReportMetricsOnUserRegrant) {
  base::HistogramTester t;
  GURL url("https://www.example1.com");
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      url, RevocationEntry(
               /*revocation_state=*/RevocationState::kRevoked,
               /*site_engagement=*/0.0,
               /*daily_notification_count=*/3));
  clock()->Advance(base::Days(5));
  site_engagement_service()->ResetBaseScoreForURL(url, 7.0);
  SetNotificationPermission(url, ContentSetting::CONTENT_SETTING_ALLOW);
  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "OutsideSafetyHub.DaysSinceProposedRevocation",
      5, 1);
  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "OutsideSafetyHub.NewSiteEngagement",
      7, 1);
  t.ExpectUniqueSample(
      "Settings.SafetyHub.DisruptiveNotificationRevocations.UserRegrant."
      "OutsideSafetyHub.PreviousNotificationCount",
      3, 1);
}

class DisruptiveNotificationPermissionsManagerShadowRunTest
    : public DisruptiveNotificationPermissionsManagerTest {
 public:
  DisruptiveNotificationPermissionsManagerShadowRunTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSafetyHubDisruptiveNotificationRevocation,
        {
            {features::kSafetyHubDisruptiveNotificationRevocationShadowRun.name,
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
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);

  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::has_reported_proposal, false)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::site_engagement, 0)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::daily_notification_count, 3)));

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

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ProposedFalsePositive) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupFalsePositiveRevocation(url, /*days_since_revocation=*/5,
                               RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 1.0);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  // First two interactions will be reported only as interactions since the SES
  // delta doesn't satisfy the min SES delta requirement.
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPersistentNotificationClick,
      source_id);

  site_engagement_service()->ResetBaseScoreForURL(url, 5.0);

  // After SES increase, the next interaction will be reported as both an
  // interaction and a revocation.
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  // Check that the correct metric is reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositiveInteraction");
  EXPECT_EQ(3u, interaction_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[0], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[0], "Reason",
      static_cast<int>(FalsePositiveReason::kPageVisit));
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "NewSiteEngagement",
                                 1.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DailyAverageVolume",
                                 4);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "RevocationState",
                                 static_cast<int>(RevocationState::kProposed));

  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[1], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[1], "Reason",
      static_cast<int>(FalsePositiveReason::kPersistentNotificationClick));
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "NewSiteEngagement",
                                 1.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "DailyAverageVolume",
                                 4);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "RevocationState",
                                 static_cast<int>(RevocationState::kProposed));

  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[2], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[2], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[2], "Reason",
      static_cast<int>(FalsePositiveReason::kPageVisit));
  ukm_recorder.ExpectEntryMetric(interaction_entries[2], "NewSiteEngagement",
                                 5.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[2], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[2], "DailyAverageVolume",
                                 4);
  ukm_recorder.ExpectEntryMetric(interaction_entries[2], "RevocationState",
                                 static_cast<int>(RevocationState::kProposed));

  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositiveRevocation");
  EXPECT_EQ(1u, revocation_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(revocation_entries[0], url);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "PageVisitCount", 2);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0],
                                 "NotificationClickCount", 1);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "NewSiteEngagement",
                                 5.0);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "DailyAverageVolume",
                                 4);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       FalsePositiveMinFalsePositiveCooldown) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupFalsePositiveRevocation(url, /*days_since_revocation=*/1,
                               RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 5);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  // Check that the interaction is anyway reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  EXPECT_EQ(1u, interaction_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[0], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DaysSinceRevocation",
                                 1);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[0], "Reason",
      static_cast<int>(FalsePositiveReason::kPageVisit));
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "NewSiteEngagement",
                                 5.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DailyAverageVolume",
                                 4);

  // Check that the no false positive revocation metrics are reported.
  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  EXPECT_EQ(0u, revocation_entries.size());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       FalsePositiveMaxFalsePositivePeriod) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupFalsePositiveRevocation(url, /*days_since_revocation=*/13,
                               RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 5);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  // Check that the interaction is not reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  EXPECT_EQ(0u, interaction_entries.size());

  // Check that the no false positive revocation metrics are reported.
  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  EXPECT_EQ(0u, revocation_entries.size());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       FalsePositiveMinSiteEngagementDelta) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupFalsePositiveRevocation(url, /*days_since_revocation=*/1,
                               RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 1);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  // Check that the interaction is anyway reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  EXPECT_EQ(1u, interaction_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[0], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DaysSinceRevocation",
                                 1);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[0], "Reason",
      static_cast<int>(FalsePositiveReason::kPageVisit));
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "NewSiteEngagement",
                                 1.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[0], "DailyAverageVolume",
                                 4);

  // Check that the no false positive revocation metrics are reported.
  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  EXPECT_EQ(0u, revocation_entries.size());
}
