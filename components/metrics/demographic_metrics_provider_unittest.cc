// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographic_metrics_provider.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace metrics {
namespace {

// Profile client for testing that gets fake Profile information and services.
class TestProfileClient : public DemographicMetricsProvider::ProfileClient {
 public:
  ~TestProfileClient() override = default;

  TestProfileClient(std::unique_ptr<syncer::SyncService> sync_service,
                    int number_of_profiles)
      : sync_service_(std::move(sync_service)),
        number_of_profiles_(number_of_profiles) {}

  int GetNumberOfProfilesOnDisk() override { return number_of_profiles_; }

  syncer::SyncService* GetSyncService() override { return sync_service_.get(); }

  base::Time GetNetworkTime() const override {
    base::Time time;
    auto result = base::Time::FromString("17 Jun 2019 00:00:00 UDT", &time);
    DCHECK(result);
    return time;
  }

 private:
  std::unique_ptr<syncer::SyncService> sync_service_;
  const int number_of_profiles_;
  base::SimpleTestClock clock_;

  DISALLOW_COPY_AND_ASSIGN(TestProfileClient);
};

// Make arbitrary user demographics to provide.
syncer::UserDemographicsResult GetDemographics() {
  syncer::UserDemographics user_demographics;
  user_demographics.birth_year = 1983;
  user_demographics.gender = UserDemographicsProto::GENDER_FEMALE;
  return syncer::UserDemographicsResult::ForValue(std::move(user_demographics));
}

std::unique_ptr<TestProfileClient> MakeTestProfileClient(
    const syncer::UserDemographicsResult& user_demographics_result,
    int number_of_profiles,
    bool has_sync_service) {
  std::unique_ptr<syncer::TestSyncService> sync_service = nullptr;
  if (has_sync_service) {
    sync_service = std::make_unique<syncer::TestSyncService>();
    sync_service->SetUserDemographics(user_demographics_result);
  }
  return std::make_unique<TestProfileClient>(std::move(sync_service),
                                             number_of_profiles);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_FeatureEnabled) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/1,
                            /*has_sync_service=*/true),
      MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Verify provided demographics.
  EXPECT_EQ(GetDemographics().value().birth_year,
            uma_proto.user_demographics().birth_year());
  EXPECT_EQ(GetDemographics().value().gender,
            uma_proto.user_demographics().gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               syncer::UserDemographicsStatus::kSuccess, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_NoSyncService) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(),
                            /*number_of_profiles=*/1,
                            /*has_sync_service=*/false),
      MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect the proto fields to be not set and left to default.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                               syncer::UserDemographicsStatus::kNoSyncService,
                               1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_FeatureDisabled) {
  // Disable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/1,
                            /*has_sync_service=*/true),
      MetricsLogUploader::MetricServiceType::UMA);
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
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider with not exactly one Profile on disk.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/2,
                            /*has_sync_service=*/true),
      MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify histograms.
  histogram.ExpectUniqueSample(
      "UMA.UserDemographics.Status",
      syncer::UserDemographicsStatus::kMoreThanOneProfile, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGender_NoUserDemographics) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider with a ProfileClient that does not provide
  // demographics because of some error.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(
          syncer::UserDemographicsResult::ForStatus(
              syncer::UserDemographicsStatus::kIneligibleDemographicsData),
          /*number_of_profiles=*/1,
          /*has_sync_service=*/true),
      MetricsLogUploader::MetricServiceType::UMA);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideSyncedUserNoisedBirthYearAndGender(&uma_proto);

  // Expect that the UMA proto is untouched.
  EXPECT_FALSE(uma_proto.user_demographics().has_birth_year());
  EXPECT_FALSE(uma_proto.user_demographics().has_gender());

  // Verify that there are no histograms for user demographics. We expect
  // histograms to be logged by the sync libraries.
  histogram.ExpectUniqueSample(
      "UMA.UserDemographics.Status",
      syncer::UserDemographicsStatus::kIneligibleDemographicsData, 1);
}

TEST(DemographicMetricsProviderTest,
     ProvideSyncedUserNoisedBirthYearAndGenderToReport) {
  // Enable demographics reporting feature.
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      DemographicMetricsProvider::kDemographicMetricsReporting);

  base::HistogramTester histogram;

  // Run demographics provider.
  DemographicMetricsProvider provider(
      MakeTestProfileClient(GetDemographics(), /*number_of_profiles=*/1,
                            /*has_sync_service=*/true),
      MetricsLogUploader::MetricServiceType::UKM);
  ukm::Report report;
  provider.ProvideSyncedUserNoisedBirthYearAndGenderToReport(&report);

  // Verify provided demographics.
  EXPECT_EQ(GetDemographics().value().birth_year,
            report.user_demographics().birth_year());
  EXPECT_EQ(GetDemographics().value().gender,
            report.user_demographics().gender());

  // Verify histograms.
  histogram.ExpectUniqueSample("UKM.UserDemographics.Status",
                               syncer::UserDemographicsStatus::kSuccess, 1);
}

}  // namespace
}  // namespace metrics