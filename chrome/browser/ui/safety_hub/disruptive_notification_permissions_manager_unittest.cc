// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/disruptive_notification_permissions_manager.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_os_notification_display_manager_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safety_check/safety_check.h"
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
constexpr char kSafeBrowsingNotificationRevocationSourceHistogram[] =
    "SafeBrowsing.NotificationRevocationSource";

class SafetyHubNotificationWrapperForTesting
    : public RevokedPermissionsOSNotificationDisplayManager::
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

class MockRevokedPermissionsOSNotificationDisplayManager
    : public RevokedPermissionsOSNotificationDisplayManager {
 public:
  explicit MockRevokedPermissionsOSNotificationDisplayManager(
      HostContentSettingsMap* hcsm)
      : RevokedPermissionsOSNotificationDisplayManager(hcsm, nullptr) {}
  MOCK_METHOD(void, DisplayNotification, (), (override));
  MOCK_METHOD(void, UpdateNotification, (), (override));
};
}  // namespace

class DisruptiveNotificationPermissionsMigrationTest : public ::testing::Test {
 public:
  using RevocationEntry =
      DisruptiveNotificationPermissionsManager::RevocationEntry;
  using ContentSettingHelper =
      DisruptiveNotificationPermissionsManager::ContentSettingHelper;
  using RevocationState =
      DisruptiveNotificationPermissionsManager::RevocationState;

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(&profile_);
  }

  RevokedPermissionsOSNotificationDisplayManager* notification_manager() {
    return RevokedPermissionsOSNotificationDisplayManagerFactory::GetForProfile(
        profile());
  }

  site_engagement::SiteEngagementService* site_engagement_service() {
    return site_engagement::SiteEngagementServiceFactory::GetForProfile(
        &profile_);
  }

  void SetupIgnoreContentSettingEntry(const GURL& url,
                                      base::TimeDelta lifetime) {
    base::Value::Dict dict;
    dict.Set("revoked_status", "ignore");
    dict.Set("site_engagement", 0.0);
    dict.Set("daily_notification_count", 4);
    dict.Set("timestamp", base::TimeToValue(base::Time::Now()));
    dict.Set("page_visit", 0);
    dict.Set("notification_click_count", 0);

    content_settings::ContentSettingConstraints constraints(base::Time::Now());
    constraints.set_lifetime(lifetime);
    hcsm()->SetWebsiteSettingCustomScope(
        ContentSettingsPattern::FromURLNoWildcard(url),
        ContentSettingsPattern::Wildcard(),
        ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
        base::Value(std::move(dict)), constraints);
  }

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
};

TEST_F(DisruptiveNotificationPermissionsMigrationTest,
       MigrateIgnoreEntriesWithoutExpiration) {
  GURL migrated_url("https://www.example1.com");
  GURL correct_url("https://www.example2.com");

  // Set up ignored entry without expiration.
  SetupIgnoreContentSettingEntry(migrated_url, base::TimeDelta());

  // Set up ignored entry with expiration.
  SetupIgnoreContentSettingEntry(correct_url, base::Days(30));

  auto manager = std::make_unique<DisruptiveNotificationPermissionsManager>(
      hcsm(), site_engagement_service(), notification_manager());
  CHECK(manager);

  // The content setting expiration was migrated on start up.
  std::optional<RevocationEntry> migrated_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(migrated_url);
  EXPECT_EQ(migrated_entry->lifetime, base::Days(365));

  // The content setting expiration is not migrated if the expiration is set.
  std::optional<RevocationEntry> correct_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(correct_url);
  EXPECT_EQ(correct_entry->lifetime, base::Days(30));
}

class DisruptiveNotificationPermissionsManagerTest : public ::testing::Test {
 public:
  using RevocationEntry =
      DisruptiveNotificationPermissionsManager::RevocationEntry;
  using ContentSettingHelper =
      DisruptiveNotificationPermissionsManager::ContentSettingHelper;
  using RevocationState =
      DisruptiveNotificationPermissionsManager::RevocationState;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        RevokedPermissionsOSNotificationDisplayManagerFactory::GetInstance(),
        base::BindRepeating(
            &DisruptiveNotificationPermissionsManagerTest::
                BuildRevokedPermissionsOSNotificationDisplayManager,
            base::Unretained(this)));
    profile_ = builder.Build();
    manager_ = std::make_unique<DisruptiveNotificationPermissionsManager>(
        hcsm(), site_engagement_service(), mock_notification_manager());
    manager_->SetClockForTesting(clock());
    clock()->SetNow(base::Time::Now());
  }
  void TearDown() override {
    manager_->SetClockForTesting(base::DefaultClock::GetInstance());
  }

  HostContentSettingsMap* hcsm() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  site_engagement::SiteEngagementService* site_engagement_service() {
    return site_engagement::SiteEngagementServiceFactory::GetForProfile(
        profile());
  }

  void SetNotificationPermission(GURL url, ContentSetting setting) {
    hcsm()->SetContentSettingDefaultScope(
        url, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
  }

  void SetDailyAverageNotificationCount(GURL url, int daily_average_count) {
    auto* notifications_engagement_service =
        NotificationsEngagementServiceFactory::GetForProfile(profile());
    notifications_engagement_service->RecordNotificationDisplayed(
        url, daily_average_count * 7);
  }

  int GetRevokedPermissionsCount() {
    return hcsm()
        ->GetSettingsForOneType(
            ContentSettingsType::REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS)
        .size();
  }

  void SetupRevocationEntry(const GURL& url,
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

  TestingProfile* profile() { return profile_.get(); }

  base::SimpleTestClock* clock() { return &clock_; }

  std::unique_ptr<KeyedService>
  BuildRevokedPermissionsOSNotificationDisplayManager(
      content::BrowserContext* context) {
    auto notification_manager =
        std::make_unique<MockRevokedPermissionsOSNotificationDisplayManager>(
            HostContentSettingsMapFactory::GetForProfile(context));
    return notification_manager;
  }

  MockRevokedPermissionsOSNotificationDisplayManager*
  mock_notification_manager() {
    return static_cast<MockRevokedPermissionsOSNotificationDisplayManager*>(
        RevokedPermissionsOSNotificationDisplayManagerFactory::GetForProfile(
            profile()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<DisruptiveNotificationPermissionsManager> manager_;
  base::SimpleTestClock clock_;
};

TEST_F(DisruptiveNotificationPermissionsManagerTest,
       ContentSettingHelperCorrectLifetime) {
  GURL url("https://example.com");

  base::TimeDelta lifetime =
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();

  for (const auto& [revocation_state, expected_lifetime] :
       std::initializer_list<std::pair<RevocationState, base::TimeDelta>>{
           {RevocationState::kProposed, lifetime},
           {RevocationState::kRevoked, lifetime},
           {RevocationState::kIgnoreInsideSH, base::Days(365)},
           {RevocationState::kIgnoreOutsideSH, base::Days(90)},
           {RevocationState::kAcknowledged, lifetime},
       }) {
    ContentSettingHelper(*hcsm()).PersistRevocationEntry(
        url, RevocationEntry(
                 /*revocation_state=*/revocation_state,
                 /*site_engagement=*/0.0,
                 /*daily_notification_count=*/4));
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
  GURL ignore_inside_sh_url("https://www.example3.com");
  GURL ignore_outside_sh_url("https://www.example4.com");
  RevocationEntry proposed_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  proposed_entry.lifetime =
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  RevocationEntry revoked_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kRevoked,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  revoked_entry.lifetime =
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  RevocationEntry ignore_inside_sh_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kIgnoreInsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  ignore_inside_sh_entry.lifetime = base::Days(365);
  RevocationEntry ignore_outside_sh_entry = RevocationEntry(
      /*revocation_state=*/RevocationState::kIgnoreOutsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  ignore_outside_sh_entry.lifetime = base::Days(90);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(proposed_url,
                                                       proposed_entry);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(revoked_url,
                                                       revoked_entry);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(ignore_inside_sh_url,
                                                       ignore_inside_sh_entry);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(ignore_outside_sh_url,
                                                       ignore_outside_sh_entry);

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(revoked_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_inside_sh_url),
      Optional(Eq(ignore_inside_sh_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_outside_sh_url),
      Optional(Eq(ignore_outside_sh_entry)));

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
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_inside_sh_url),
      Optional(Eq(ignore_inside_sh_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignore_outside_sh_url),
      Optional(Eq(ignore_outside_sh_entry)));
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
  SetDailyAverageNotificationCount(url, 4);
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
              Optional(Field(&RevocationEntry::daily_notification_count, 4)));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 4, 1);

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
  t.ExpectBucketCount(
      kSafeBrowsingNotificationRevocationSourceHistogram,
      safe_browsing::NotificationRevocationSource::kDisruptiveAutoRevocation,
      1);

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
  SetDailyAverageNotificationCount(url, 4);
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
  SetDailyAverageNotificationCount(url, 4);
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
              Optional(Field(&RevocationEntry::daily_notification_count, 4)));

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
  SetDailyAverageNotificationCount(url, 4);
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

  EXPECT_CALL(*mock_notification_manager(), DisplayNotification);
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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  EXPECT_CALL(*mock_notification_manager(), DisplayNotification).Times(0);
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
  t.ExpectBucketCount(kNotificationCountHistogram, 4, 1);

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
  SetDailyAverageNotificationCount(first_url, 4);
  site_engagement_service()->ResetBaseScoreForURL(first_url, 0);

  GURL second_url("https://www.chrome.com");
  SetNotificationPermission(second_url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(second_url, 1);
  site_engagement_service()->ResetBaseScoreForURL(second_url, 0);

  GURL third_url("https://www.anothersite.com");
  SetNotificationPermission(third_url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(third_url, 4);
  site_engagement_service()->ResetBaseScoreForURL(third_url, 0);

  manager()->RevokeDisruptiveNotifications();

  EXPECT_EQ(GetRevokedPermissionsCount(), 2);

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 2);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kNotDisruptive, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 2, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 4, 2);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       DontRevokePermissionHighEngagement) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
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
  SetDailyAverageNotificationCount(url, 4);
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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kManagedContentSetting, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       IgnoredForAbusiveRevocation) {
  base::HistogramTester t;
  GURL url("https://www.example.com");
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url), GURL(url),
      ContentSettingsType::REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
      base::Value(base::Value::Dict().Set(safety_hub::kRevokedStatusDictKeyStr,
                                          safety_hub::kIgnoreStr)));

  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  manager()->RevokeDisruptiveNotifications();
  EXPECT_EQ(GetRevokedPermissionsCount(), 0);
  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kAbusiveRevocationIgnored, 1);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NotRevokedDefaultBlock) {
  base::HistogramTester t;
  hcsm()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK);

  GURL url("https://www.example.com");
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // Set up a revoked content setting.
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      url, RevocationEntry(
               /*revocation_state=*/RevocationState::kRevoked,
               /*site_engagement=*/0.0,
               /*daily_notification_count=*/4,
               /*timestamp=*/clock()->Now()));

  clock()->Advance(base::Days(5));

  manager()->RegrantPermissionForUrl(url);
  // Notifications are again allowed.
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      hcsm()->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));

  // The content setting was updated to "ignore" to prevent auto-revoking in
  // the future.
  EXPECT_EQ(GetRevokedPermissionsCount(), 1);
  std::optional<RevocationEntry> revocation_entry =
      ContentSettingHelper(*hcsm()).GetRevocationEntry(url);

  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::revocation_state,
                             RevocationState::kIgnoreInsideSH)));

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
      4, 1);

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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // Set up an ignored value.
  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kIgnoreInsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4,
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
              Optional(Field(&RevocationEntry::daily_notification_count, 4)));
  EXPECT_THAT(revocation_entry,
              Optional(Field(&RevocationEntry::timestamp, base::Time::Now())));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       NoUndoRegrantPermissionMissingContentSetting) {
  base::HistogramTester t;
  GURL url("https://www.example.com");

  // Set up a disruptive notification.
  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // Set up an ignored value.
  RevocationEntry entry(
      /*revocation_state=*/RevocationState::kIgnoreInsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4,
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
      /*daily_notification_count=*/4);
  revoked_entry.lifetime =
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(revoked_url,
                                                       revoked_entry);

  // Set up a proposed permission.
  GURL proposed_url("https://www.example2.com");
  RevocationEntry proposed_entry(
      /*revocation_state=*/RevocationState::kProposed,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  proposed_entry.lifetime =
      safety_check::GetUnusedSitePermissionsRevocationCleanUpThreshold();
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(proposed_url,
                                                       proposed_entry);

  // Set up an ignored inside SH permission.
  GURL ignored_inside_SH_url("https://www.example3.com");
  RevocationEntry ignored_inside_SH_entry(
      /*revocation_state=*/RevocationState::kIgnoreInsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  ignored_inside_SH_entry.lifetime = base::Days(365);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(ignored_inside_SH_url,
                                                       ignored_inside_SH_entry);

  // Set up an ignored outside SH permission.
  GURL ignored_outside_SH_url("https://www.example4.com");
  RevocationEntry ignored_outside_SH_entry(
      /*revocation_state=*/RevocationState::kIgnoreOutsideSH,
      /*site_engagement=*/0.0,
      /*daily_notification_count=*/4);
  ignored_outside_SH_entry.lifetime = base::Days(90);
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      ignored_outside_SH_url, ignored_outside_SH_entry);

  EXPECT_EQ(GetRevokedPermissionsCount(), 4);
  manager()->ClearRevokedPermissionsList();
  EXPECT_EQ(GetRevokedPermissionsCount(), 4);

  RevocationEntry acknowledged_entry = revoked_entry;
  acknowledged_entry.revocation_state = RevocationState::kAcknowledged;

  // Only revoked value is set to acknowledged, others are not affected.
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(acknowledged_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_inside_SH_url),
      Optional(Eq(ignored_inside_SH_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_outside_SH_url),
      Optional(Eq(ignored_outside_SH_entry)));

  content_settings::ContentSettingConstraints constraints;
  manager()->RestoreDeletedRevokedPermission(
      ContentSettingsPattern::FromURLNoWildcard(revoked_url),
      constraints.Clone());

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(revoked_url),
              Optional(Eq(revoked_entry)));
  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(proposed_url),
              Optional(Eq(proposed_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_inside_SH_url),
      Optional(Eq(ignored_inside_SH_entry)));
  EXPECT_THAT(
      ContentSettingHelper(*hcsm()).GetRevocationEntry(ignored_outside_SH_url),
      Optional(Eq(ignored_outside_SH_entry)));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       SetIgnoreOnContentSettingsChanged) {
  for (auto [initial_state, new_content_setting, expected_state] :
       std::initializer_list<std::tuple<RevocationState, ContentSetting,
                                        std::optional<RevocationState>>>{
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_ALLOW,
            RevocationState::kIgnoreOutsideSH},
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_BLOCK,
            std::nullopt},
           {RevocationState::kRevoked, ContentSetting::CONTENT_SETTING_ASK,
            std::nullopt},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_ALLOW,
            std::nullopt},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_BLOCK,
            std::nullopt},
           {RevocationState::kProposed, ContentSetting::CONTENT_SETTING_ASK,
            std::nullopt},
           {RevocationState::kIgnoreOutsideSH,
            ContentSetting::CONTENT_SETTING_ALLOW, std::nullopt},
           {RevocationState::kIgnoreOutsideSH,
            ContentSetting::CONTENT_SETTING_BLOCK, std::nullopt},
           {RevocationState::kIgnoreOutsideSH,
            ContentSetting::CONTENT_SETTING_ASK, std::nullopt},
       }) {
    GURL url("https://www.example1.com");
    ContentSettingHelper(*hcsm()).PersistRevocationEntry(
        url, RevocationEntry(
                 /*revocation_state=*/initial_state,
                 /*site_engagement=*/0.0,
                 /*daily_notification_count=*/4));

    SetNotificationPermission(url, new_content_setting);
    manager()->OnPermissionChanged(
        ContentSettingsPattern::FromURLNoWildcard(url),
        ContentSettingsPattern::Wildcard());

    std::optional<RevocationEntry> revocation_entry =
        ContentSettingHelper(*hcsm()).GetRevocationEntry(url);
    if (expected_state) {
      EXPECT_THAT(
          revocation_entry,
          Optional(Field(&RevocationEntry::revocation_state, expected_state)));
    } else {
      EXPECT_EQ(revocation_entry, std::nullopt);
    }

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
               /*daily_notification_count=*/4));
  clock()->Advance(base::Days(5));
  site_engagement_service()->ResetBaseScoreForURL(url, 7.0);
  SetNotificationPermission(url, ContentSetting::CONTENT_SETTING_ALLOW);
  manager()->OnPermissionChanged(ContentSettingsPattern::FromURLNoWildcard(url),
                                 ContentSettingsPattern::Wildcard());

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
      4, 1);
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
  SetDailyAverageNotificationCount(url, 4);
  site_engagement_service()->ResetBaseScoreForURL(url, 0);

  // The shadow run should never display notifications.
  EXPECT_CALL(*mock_notification_manager(), DisplayNotification).Times(0);
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
              Optional(Field(&RevocationEntry::daily_notification_count, 4)));

  t.ExpectBucketCount(kRevocationResultHistogram,
                      RevocationResult::kProposedRevoke, 1);
  t.ExpectBucketCount(kRevokedWebsitesCountHistogram, 1, 1);
  t.ExpectBucketCount(kNotificationCountHistogram, 4, 1);

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
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ProposedFalsePositive) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
  SetupRevocationEntry(url, /*days_since_revocation=*/5,
                       RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 1.0);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  // First two interactions will be reported only as interactions.
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPersistentNotificationClick,
      source_id);

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Proposed.FalsePositiveInteraction",
      FalsePositiveReason::kPageVisit, 1);

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Proposed.FalsePositiveInteraction",
      FalsePositiveReason::kPersistentNotificationClick, 1);

  site_engagement_service()->ResetBaseScoreForURL(url, 3.0);

  // The next safety check run, the entry is removed from proposal list and
  // reported as not disruptive anymore.
  manager()->RevokeDisruptiveNotifications();

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "NotDisruptiveAnymore.DaysSinceProposedRevocation",
      5, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "NotDisruptiveAnymore.SiteEngagementIncreased",
      3, 1);

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));

  // Check that the correct UKM metrics are reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositiveInteraction");
  EXPECT_EQ(2u, interaction_entries.size());
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
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       RevokedFalsePositive) {
  base::HistogramTester t;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetDailyAverageNotificationCount(url, 4);
  SetupRevocationEntry(url, /*days_since_revocation=*/5,
                       RevocationState::kRevoked);
  site_engagement_service()->ResetBaseScoreForURL(url, 1.0);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);
  site_engagement_service()->ResetBaseScoreForURL(url, 3.0);
  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "Revoked.FalsePositiveInteraction",
      FalsePositiveReason::kPageVisit, 2);

  // Check that the correct UKM metrics are reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositiveInteraction");
  EXPECT_EQ(2u, interaction_entries.size());
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
                                 static_cast<int>(RevocationState::kRevoked));

  ukm_recorder.ExpectEntrySourceHasUrl(interaction_entries[1], url);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(
      interaction_entries[0], "Reason",
      static_cast<int>(FalsePositiveReason::kPageVisit));
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "NewSiteEngagement",
                                 3.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "DailyAverageVolume",
                                 4);
  ukm_recorder.ExpectEntryMetric(interaction_entries[1], "RevocationState",
                                 static_cast<int>(RevocationState::kRevoked));

  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations."
      "FalsePositiveRevocation");
  EXPECT_EQ(1u, revocation_entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(revocation_entries[0], url);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "DaysSinceRevocation",
                                 5);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "PageVisitCount", 2);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0],
                                 "NotificationClickCount", 0);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "NewSiteEngagement",
                                 3.0);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "OldSiteEngagement",
                                 0.0);
  ukm_recorder.ExpectEntryMetric(revocation_entries[0], "DailyAverageVolume",
                                 4);
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       ProposedNotificationCountDecreased) {
  base::HistogramTester t;
  GURL url("https://chrome.test/");

  SetNotificationPermission(url, CONTENT_SETTING_ALLOW);
  SetupRevocationEntry(url, /*days_since_revocation=*/5,
                       RevocationState::kProposed);

  SetDailyAverageNotificationCount(url, 1);

  // The next safety check run, the entry is removed from proposal list because
  // the notification count is low (1) and reported as not disruptive anymore.
  manager()->RevokeDisruptiveNotifications();

  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "NotDisruptiveAnymore.DaysSinceProposedRevocation",
      5, 1);
  t.ExpectBucketCount(
      "Settings.SafetyHub.DisruptiveNotificationRevocations."
      "NotDisruptiveAnymore.NotificationCountDecreased",
      1, 1);

  EXPECT_THAT(ContentSettingHelper(*hcsm()).GetRevocationEntry(url),
              Eq(std::nullopt));
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       FalsePositiveMinFalsePositiveCooldown) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupRevocationEntry(url, /*days_since_revocation=*/1,
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

  SetupRevocationEntry(url, /*days_since_revocation=*/13,
                       RevocationState::kProposed);
  site_engagement_service()->ResetBaseScoreForURL(url, 5);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, url);

  DisruptiveNotificationPermissionsManager::MaybeReportFalsePositive(
      profile(), url, FalsePositiveReason::kPageVisit, source_id);

  // Check that the interaction is still reported.
  auto interaction_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveInteraction");
  EXPECT_EQ(1u, interaction_entries.size());

  // Check that the no false positive revocation metrics are reported.
  auto revocation_entries = ukm_recorder.GetEntriesByName(
      "SafetyHub.DisruptiveNotificationRevocations.FalsePositiveRevocation");
  EXPECT_EQ(0u, revocation_entries.size());
}

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       FalsePositiveMinSiteEngagementDelta) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL url("https://chrome.test/");

  SetupRevocationEntry(url, /*days_since_revocation=*/1,
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

TEST_F(DisruptiveNotificationPermissionsManagerRevocationTest,
       IsUrlIgnoredForRevokedDisruptiveNotification) {
  GURL ignored_inside_sh_url("https://www.example1.com");
  GURL ignored_outside_sh_url("https://www.example2.com");
  GURL revoked_url("https://www.example3.com");
  GURL non_existent_url("https://www.example4.com");

  // Set up ignored entries.
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      ignored_inside_sh_url,
      RevocationEntry(/*revocation_state=*/RevocationState::kIgnoreInsideSH,
                      /*site_engagement=*/0.0,
                      /*daily_notification_count=*/4));
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      ignored_outside_sh_url,
      RevocationEntry(/*revocation_state=*/RevocationState::kIgnoreOutsideSH,
                      /*site_engagement=*/0.0,
                      /*daily_notification_count=*/4));
  // Set up a revoked entry.
  ContentSettingHelper(*hcsm()).PersistRevocationEntry(
      revoked_url,
      RevocationEntry(/*revocation_state=*/RevocationState::kRevoked,
                      /*site_engagement=*/0.0,
                      /*daily_notification_count=*/4));

  EXPECT_TRUE(DisruptiveNotificationPermissionsManager::
                  IsUrlIgnoredForRevokedDisruptiveNotification(
                      hcsm(), ignored_inside_sh_url));
  EXPECT_TRUE(DisruptiveNotificationPermissionsManager::
                  IsUrlIgnoredForRevokedDisruptiveNotification(
                      hcsm(), ignored_outside_sh_url));
  EXPECT_FALSE(
      DisruptiveNotificationPermissionsManager::
          IsUrlIgnoredForRevokedDisruptiveNotification(hcsm(), revoked_url));
  EXPECT_FALSE(DisruptiveNotificationPermissionsManager::
                   IsUrlIgnoredForRevokedDisruptiveNotification(
                       hcsm(), non_existent_url));
}
