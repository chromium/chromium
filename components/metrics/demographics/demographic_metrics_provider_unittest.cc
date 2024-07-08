// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/demographic_metrics_provider.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {
namespace {

constexpr int kTestBirthYear = 1983;
constexpr UserDemographicsProto::Gender kTestGender =
    UserDemographicsProto::GENDER_FEMALE;

enum TestSyncServiceState {
  NULL_SYNC_SERVICE,
  SYNC_FEATURE_NOT_ENABLED,
  SYNC_FEATURE_ENABLED,
  SYNC_FEATURE_ENABLED_BUT_PAUSED,
  SYNC_FEATURE_DISABLED_BUT_PREFERENCES_ENABLED,
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Represents the user clearing sync data via dashboard. On all platforms
  // except ChromeOS (Ash), this clears the primary account (which is basically
  // SYNC_FEATURE_NOT_ENABLED). On ChromeOS Ash, Sync enters a special state.
  SYNC_FEATURE_DISABLED_ON_CHROMEOS_ASH_VIA_DASHBOARD,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Profile client for testing that gets fake Profile information and services.
class TestProfileClient : public DemographicMetricsProvider::ProfileClient {
 public:
  TestProfileClient(const TestProfileClient&) = delete;
  TestProfileClient& operator=(const TestProfileClient&) = delete;

  ~TestProfileClient() override = default;

  TestProfileClient(int number_of_profiles,
                    TestSyncServiceState sync_service_state)
      : number_of_profiles_(number_of_profiles) {
    RegisterDemographicsLocalStatePrefs(pref_service_.registry());
    RegisterDemographicsProfilePrefs(pref_service_.registry());

    switch (sync_service_state) {
      case NULL_SYNC_SERVICE:
        break;

      case SYNC_FEATURE_NOT_ENABLED:
        sync_service_ = std::make_unique<syncer::TestSyncService>();
        // Set an arbitrary disable reason to mimic sync feature being unable to
        // start.
        sync_service_->SetHasUnrecoverableError(true);
        break;

      case SYNC_FEATURE_ENABLED:
        // TestSyncService by default behaves as everything enabled/active.
        sync_service_ = std::make_unique<syncer::TestSyncService>();

        CHECK(sync_service_->GetDisableReasons().empty());
        CHECK_EQ(syncer::SyncService::TransportState::ACTIVE,
                 sync_service_->GetTransportState());
        break;

      case SYNC_FEATURE_ENABLED_BUT_PAUSED:
        sync_service_ = std::make_unique<syncer::TestSyncService>();
        // Mimic the user signing out from content are (sync paused).
        sync_service_->SetPersistentAuthError();

        CHECK(sync_service_->GetDisableReasons().empty());
        CHECK_EQ(syncer::SyncService::TransportState::PAUSED,
                 sync_service_->GetTransportState());
        break;

      case SYNC_FEATURE_DISABLED_BUT_PREFERENCES_ENABLED:
        sync_service_ = std::make_unique<syncer::TestSyncService>();
        sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
        CHECK(sync_service_->GetUserSettings()->GetSelectedTypes().Has(
            syncer::UserSelectableType::kPreferences));
        CHECK(!sync_service_->IsSyncFeatureEnabled());
        CHECK(sync_service_->GetDisableReasons().empty());
        CHECK_EQ(syncer::SyncService::TransportState::ACTIVE,
                 sync_service_->GetTransportState());
        break;

#if BUILDFLAG(IS_CHROMEOS_ASH)
      case SYNC_FEATURE_DISABLED_ON_CHROMEOS_ASH_VIA_DASHBOARD:
        sync_service_ = std::make_unique<syncer::TestSyncService>();
        sync_service_->GetUserSettings()->SetSyncFeatureDisabledViaDashboard(
            true);

        // On ChromeOS Ash, IsInitialSyncFeatureSetupComplete always returns
        // true but IsSyncFeatureEnabled() stays false because the user needs to
        // manually resume sync the feature.
        CHECK(sync_service_->GetUserSettings()
                  ->IsInitialSyncFeatureSetupComplete());
        CHECK(!sync_service_->IsSyncFeatureEnabled());
        break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }

  int GetNumberOfProfilesOnDisk() override { return number_of_profiles_; }

  syncer::SyncService* GetSyncService() override { return sync_service_.get(); }

  PrefService* GetLocalState() override { return &pref_service_; }

  PrefService* GetProfilePrefs() override { return &pref_service_; }

  base::Time GetNetworkTime() const override {
    base::Time time;
    auto result = base::Time::FromString("17 Jun 2019 00:00:00 UDT", &time);
    DCHECK(result);
    return time;
  }

  void SetDemographicsInPrefs(int birth_year,
                              metrics::UserDemographicsProto_Gender gender) {
    base::Value::Dict dict;
    dict.Set(kSyncDemographicsBirthYearPath, birth_year);
    dict.Set(kSyncDemographicsGenderPath, static_cast<int>(gender));
    pref_service_.SetDict(kSyncDemographicsPrefName, std::move(dict));
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  const int number_of_profiles_;
  base::SimpleTestClock clock_;
};

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_FeatureEnabled) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    SYNC_FEATURE_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Set birth year noise offset to not have it randomized.
  const int kBirthYearOffset = 3;
  client->GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                                      kBirthYearOffset);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Verify provided demographics.
  EXPECT_EQ(kTestBirthYear + kBirthYearOffset,
            uma_proto.user_demographics().birth_year());
  EXPECT_EQ(kTestGender, uma_proto.user_demographics().gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSuccess, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_NoSyncService) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    NULL_SYNC_SERVICE);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kNoSyncService, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_SyncEnabledButPaused) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(
      /*number_of_profiles=*/1, SYNC_FEATURE_ENABLED_BUT_PAUSED);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSyncNotEnabled, 1);
}

TEST(
    DemographicMetricsProviderTest,
    ProvideSyncedUserNoisedBirthYearAndGender_SyncFeatureDisabledButPreferencesEnabled_WithSyncToSignin) {
  base::test::ScopedFeatureList sync_to_signin_enabled;
  sync_to_signin_enabled.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(
      /*number_of_profiles=*/1, SYNC_FEATURE_DISABLED_BUT_PREFERENCES_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Set birth year noise offset to not have it randomized.
  const int kBirthYearOffset = 3;
  client->GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                                      kBirthYearOffset);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Verify provided demographics.
  EXPECT_EQ(kTestBirthYear + kBirthYearOffset,
            uma_proto.user_demographics().birth_year());
  EXPECT_EQ(kTestGender, uma_proto.user_demographics().gender());

  // Verify histograms: Demographics should be provided.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSuccess, 1);
}

TEST(
    DemographicMetricsProviderTest,
    ProvideSyncedUserNoisedBirthYearAndGender_SyncFeatureDisabledButPreferencesEnabled_WithoutSyncToSignin) {
  base::test::ScopedFeatureList sync_to_signin_disabled;
  sync_to_signin_disabled.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(
      /*number_of_profiles=*/1, SYNC_FEATURE_DISABLED_BUT_PREFERENCES_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Set birth year noise offset to not have it randomized.
  const int kBirthYearOffset = 3;
  client->GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                                      kBirthYearOffset);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms: Demographics should NOT be provided.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSyncNotEnabled, 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(
    DemographicMetricsProviderTest,
    ProvideSyncedUserNoisedBirthYearAndGender_SyncFeatureDisabledOnChromeOsAshViaSyncDashboard) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(
      /*number_of_profiles=*/1,
      SYNC_FEATURE_DISABLED_ON_CHROMEOS_ASH_VIA_DASHBOARD);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSyncNotEnabled, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_SyncNotEnabled) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    SYNC_FEATURE_NOT_ENABLED);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSyncNotEnabled, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_FeatureDisabled) {
  // Disable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(kDemographicMetricsReporting);

  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    SYNC_FEATURE_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify that there are no histograms for user demographics.
  histogram.ExpectTotalCount("UMA.UserDemographics.Status", 0);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_NotExactlyOneProfile) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/2,
                                                    SYNC_FEATURE_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Run demographics provider with not exactly one Profile on disk.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kMoreThanOneProfile, 1);
#else
  // On ChromeOS, we have a profile selection strategy, so expect UMA reporting
  // to work.
  EXPECT_TRUE(uma_proto.user_demographics().has_birth_year());
  EXPECT_TRUE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               UserDemographicsStatus::kSuccess, 1);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_NoUserDemographics) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    SYNC_FEATURE_ENABLED);
  // Set some ineligible values to prefs.
  client->SetDemographicsInPrefs(/*birth_year=*/-17,
                                 UserDemographicsProto::GENDER_UNKNOWN);

  // Run demographics provider with a ProfileClient that does not provide
  // demographics because of some error.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify that there are no histograms for user demographics.
  histogram.ExpectUniqueSample(
      "UMA.UserDemographics.Status",
      UserDemographicsStatus::kIneligibleDemographicsData, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGenderToUkmReport) {
  base::HistogramTester histogram;

  auto client = std::make_unique<TestProfileClient>(/*number_of_profiles=*/1,
                                                    SYNC_FEATURE_ENABLED);
  client->SetDemographicsInPrefs(kTestBirthYear, kTestGender);

  // Set birth year noise offset to not have it randomized.
  const int kBirthYearOffset = 3;
  client->GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                                      kBirthYearOffset);

  // Run demographics provider.
  DemographicMetricsProvider provider(
      std::move(client), MetricsLogUploader::MetricServiceType::UKM);
  ukm::Report report;
  provider.ProvideSyncedUserNoisedBirthYearAndGenderToReport(&report);

  // Verify provided demographics.
  EXPECT_EQ(kTestBirthYear + kBirthYearOffset,
            report.user_demographics().birth_year());
  EXPECT_EQ(kTestGender, report.user_demographics().gender());
}

}  // namespace
}  // namespace metrics
