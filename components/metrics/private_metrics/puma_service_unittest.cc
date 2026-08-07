// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/puma_service.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/private_metrics/lom_recorder.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/private_metrics_pref_names.h"
#include "components/metrics/private_metrics/puma_histogram_functions.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/private_metrics/system_profiles/coarse_system_profile.pb.h"
#include "third_party/metrics_proto/private_metrics/system_profiles/rc_coarse_system_profile.pb.h"

namespace metrics::private_metrics {

namespace {

using ::private_metrics::RcCoarseSystemProfile;

class PumaServiceTest : public testing::Test {
 public:
  PumaServiceTest() = default;
  PumaServiceTest(const PumaServiceTest&) = delete;
  PumaServiceTest& operator=(const PumaServiceTest&) = delete;
  ~PumaServiceTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(GetEnabledFeatures(), {});

    PumaService::RegisterPrefs(prefs_.registry());
    PrivateMetricsReportingService::RegisterPrefs(prefs_.registry());

    client_.set_country_id_holder(
        regional_capabilities::CountryIdHolder(country_codes::CountryId("BE")));
    puma_service_ = std::make_unique<PumaService>(&client_, &prefs_);
  }

  void TearDown() override { puma_service_ = nullptr; }

  virtual std::vector<base::test::FeatureRef> GetEnabledFeatures() {
    return {kPrivateMetricsPuma};
  }

  size_t GetUnsentLogCount() {
    return puma_service_->reporting_service()->unsent_log_store()->size();
  }

  size_t GetPersistedLogCount() {
    return prefs_.GetList(prefs::kUnsentLogStoreName).size();
  }

 protected:
  TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<PumaService> puma_service_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class PumaServiceFeaturesTest
    : public PumaServiceTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<base::test::FeatureRef>, bool>> {
 public:
  PumaServiceFeaturesTest() = default;

  PumaServiceFeaturesTest(const PumaServiceFeaturesTest&) = delete;
  PumaServiceFeaturesTest& operator=(const PumaServiceFeaturesTest&) = delete;

  ~PumaServiceFeaturesTest() override = default;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    return std::get<0>(GetParam());
  }
};

class PumaServiceRcTest : public PumaServiceTest,
                          public testing::WithParamInterface<bool> {
 public:
  PumaServiceRcTest() = default;

  PumaServiceRcTest(const PumaServiceRcTest&) = delete;
  PumaServiceRcTest& operator=(const PumaServiceRcTest&) = delete;

  ~PumaServiceRcTest() override = default;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRef> features = {kPrivateMetricsPuma,
                                                    kPrivateMetricsPumaRc};
    if (IsLomFeatureEnabled()) {
      features.push_back(kLomFeature);
    }
    return features;
  }

  bool IsLomFeatureEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, PumaServiceRcTest, testing::Bool());

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    PumaServiceFeaturesTest,
    testing::ValuesIn(
        std::vector<std::tuple<std::vector<base::test::FeatureRef>, bool>>{
            {{}, false},
            {{kPrivateMetricsFeature}, false},
            {{kPrivateMetricsPuma}, true},
            {{kPrivateMetricsPuma, kPrivateMetricsFeature}, false},
        }));

TEST_P(PumaServiceFeaturesTest, IsPumaEnabled) {
  EXPECT_EQ(PumaService::IsPumaEnabled(), std::get<1>(GetParam()));
}

TEST_P(PumaServiceRcTest, RcRecordCoarseSystemProfile) {
  client_.set_is_extended_stable_channel(true);

  RcCoarseSystemProfile rc_profile;

  puma_service_->RecordRcProfile(&rc_profile);

  EXPECT_TRUE(rc_profile.has_channel());
  EXPECT_EQ(rc_profile.channel(), RcCoarseSystemProfile::CHANNEL_BETA);

  EXPECT_TRUE(rc_profile.has_is_extended_stable_channel());
  EXPECT_EQ(rc_profile.is_extended_stable_channel(), true);

  EXPECT_TRUE(rc_profile.has_milestone());
  EXPECT_TRUE(rc_profile.has_platform());

  EXPECT_TRUE(rc_profile.has_profile_country_id());
  EXPECT_EQ(rc_profile.profile_country_id(),
            country_codes::CountryId("BE").Serialize());
}

TEST_F(PumaServiceTest, RcClientId_IsNullWhenPumaRcIsDisabled) {
  EXPECT_FALSE(puma_service_->GetPumaRcClientId().has_value());
}

TEST_P(PumaServiceRcTest, RcClientId_IsNonNull) {
  EXPECT_TRUE(puma_service_->GetPumaRcClientId().has_value());
}

TEST_P(PumaServiceRcTest, RcClientId_SameWhenExecutedMultipleTimes) {
  std::optional<uint64_t> client_id_1 = puma_service_->GetPumaRcClientId();
  std::optional<uint64_t> client_id_2 = puma_service_->GetPumaRcClientId();

  EXPECT_TRUE(client_id_1.has_value());
  EXPECT_EQ(client_id_1, client_id_2);
}

TEST_P(PumaServiceRcTest, RcClientId_UpdatesPref) {
  EXPECT_EQ(prefs_.GetUint64(prefs::kPumaRcClientId), 0u);

  std::optional<uint64_t> client_id_1 = puma_service_->GetPumaRcClientId();
  uint64_t pref_value_1 = prefs_.GetUint64(prefs::kPumaRcClientId);

  EXPECT_NE(pref_value_1, 0u);
  EXPECT_EQ(client_id_1, pref_value_1);

  std::optional<uint64_t> client_id_2 = puma_service_->GetPumaRcClientId();
  uint64_t pref_value_2 = prefs_.GetUint64(prefs::kPumaRcClientId);

  EXPECT_EQ(client_id_2, pref_value_2);
  EXPECT_EQ(pref_value_1, pref_value_2);
}

TEST_P(PumaServiceRcTest, RcBuildReport_DoesNotCreateReportWithoutEvents) {
  auto report = puma_service_->BuildPrivateMetricRcReport();
  EXPECT_FALSE(report.has_value());

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaReportBuildingOutcomeRc,
      PumaService::ReportBuildingOutcome::kNotBuiltNoData, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaReportBuildingOutcomeRc, 1);
}

TEST_P(PumaServiceRcTest, RcBuildReport_DoesCreateReportWithEvents) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean1",
                       true);

  auto report = puma_service_->BuildPrivateMetricRcReport();
  EXPECT_TRUE(report.has_value());

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaReportBuildingOutcomeRc,
      PumaService::ReportBuildingOutcome::kBuilt, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaReportBuildingOutcomeRc, 1);
}

TEST_F(PumaServiceTest, RcBuildReport_DoesNotCreateReportWithFeatureDisabled) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean2",
                       true);

  auto report = puma_service_->BuildPrivateMetricRcReport();
  EXPECT_FALSE(report.has_value());

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaReportBuildingOutcomeRc,
      PumaService::ReportBuildingOutcome::kNotBuiltFeatureDisabled, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaReportBuildingOutcomeRc, 1);
}

TEST_P(PumaServiceRcTest, RcBuildReport_PayloadProperlyFilled) {
  PumaHistogramExactLinear(PumaType::kRc,
                           "PUMA.PumaServiceTestHistogram.Linear1", 12, 100);

  auto report = puma_service_->BuildPrivateMetricRcReport();

  EXPECT_TRUE(report.has_value());

  EXPECT_TRUE(report->has_client_id());
  EXPECT_NE(report->client_id(), 0u);

  EXPECT_TRUE(report->has_rc_profile());

  if (IsLomFeatureEnabled()) {
    ASSERT_EQ(report->profile_keyed_histogram_events_size(), 1);
    const auto& profile_keyed_event = report->profile_keyed_histogram_events(0);
    EXPECT_EQ(profile_keyed_event.profile_id(), 0u);
    EXPECT_EQ(profile_keyed_event.histogram_events_size(), 1);
    auto histogram_event = profile_keyed_event.histogram_events().at(0);
    EXPECT_TRUE(histogram_event.has_name_hash());
    EXPECT_EQ(histogram_event.bucket_size(), 1);
  } else {
    ASSERT_EQ(report->histogram_events_size(), 1);
    auto histogram_event = report->histogram_events().at(0);
    EXPECT_TRUE(histogram_event.has_name_hash());
    EXPECT_EQ(histogram_event.bucket_size(), 1);
  }
}

TEST_P(PumaServiceRcTest, RcBuildReportAndStore_DoesCreateAndStoreReport) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean3",
                       true);

  EXPECT_EQ(GetUnsentLogCount(), 0u);

  puma_service_->BuildPrivateMetricRcReportAndStoreLog(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);

  EXPECT_EQ(GetUnsentLogCount(), 1u);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaReportStoringOutcomeRc,
      PumaService::ReportStoringOutcome::kStored, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaReportStoringOutcomeRc, 1);
}

TEST_P(PumaServiceRcTest, RcBuildReportAndStore_DoesNotStoreReportWithNoData) {
  EXPECT_EQ(GetUnsentLogCount(), 0u);

  puma_service_->BuildPrivateMetricRcReportAndStoreLog(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);

  EXPECT_EQ(GetUnsentLogCount(), 0u);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaReportStoringOutcomeRc,
      PumaService::ReportStoringOutcome::kNotStoredNoReport, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaReportStoringOutcomeRc, 1);
}

TEST_P(PumaServiceRcTest, RcLogsArePersistedAfterFlush) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean4",
                       true);

  EXPECT_EQ(GetPersistedLogCount(), 0u);
  puma_service_->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1u);
}

TEST_P(PumaServiceRcTest, RcLogsArePersistedAfterDisablingReporting) {
  puma_service_->EnableReporting();

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean5",
                       true);

  EXPECT_EQ(GetPersistedLogCount(), 0u);
  puma_service_->DisableReporting();
  EXPECT_EQ(GetPersistedLogCount(), 1u);
}

TEST_P(PumaServiceRcTest,
       RcLogsAreNotPersistedAfterDisablingReportingWhenReportingWasDisabled) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean6",
                       true);

  EXPECT_EQ(GetPersistedLogCount(), 0u);
  puma_service_->DisableReporting();
  EXPECT_EQ(GetPersistedLogCount(), 0u);
}

TEST_P(PumaServiceRcTest, RcLogsArePersistedAfterDestruction) {
  puma_service_->EnableReporting();

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean7",
                       true);

  EXPECT_EQ(GetPersistedLogCount(), 0u);
  puma_service_ = nullptr;
  EXPECT_EQ(GetPersistedLogCount(), 1u);
}

TEST_P(PumaServiceRcTest,
       RcLogsAreNotPersistedAfterDestructionWhenReportingWasInactive) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean8",
                       true);

  EXPECT_EQ(GetPersistedLogCount(), 0u);
  puma_service_ = nullptr;
  EXPECT_EQ(GetPersistedLogCount(), 0u);
}

TEST_F(PumaServiceTest, EnableReportingStartsSchedulerTask) {
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // When the reporting is enabled, we should have:
  //  1. the log rotation scheduler, and
  //  2. the uploader.
  puma_service_->EnableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);

  puma_service_->DisableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(PumaServiceTest, EnableReportingTwiceDoesNotStartAdditionalTasks) {
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  puma_service_->EnableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);

  puma_service_->EnableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);
}

TEST_P(PumaServiceRcTest, UploadUnsentLogs) {
  puma_service_->EnableReporting();

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean9",
                       true);

  EXPECT_EQ(GetUnsentLogCount(), 0u);
  EXPECT_EQ(GetPersistedLogCount(), 0u);

  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 0);

  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaLogRotationOutcome,
      PumaService::LogRotationOutcome::kLogRotationPerformed, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 1);

  EXPECT_EQ(GetUnsentLogCount(), 1u);
  EXPECT_EQ(GetPersistedLogCount(), 0u);

  EXPECT_NE(client_.uploader(), nullptr);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  EXPECT_EQ(GetUnsentLogCount(), 0u);
  EXPECT_EQ(GetPersistedLogCount(), 0u);
}

TEST_P(PumaServiceRcTest, UploadPersistedLogs) {
  puma_service_->EnableReporting();

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean10",
                       true);

  EXPECT_EQ(GetUnsentLogCount(), 0u);
  EXPECT_EQ(GetPersistedLogCount(), 0u);

  // Simulate forced shutdown
  puma_service_ = nullptr;

  EXPECT_EQ(GetPersistedLogCount(), 1u);

  puma_service_ = std::make_unique<PumaService>(&client_, &prefs_);

  EXPECT_EQ(GetUnsentLogCount(), 1u);
  EXPECT_EQ(GetPersistedLogCount(), 1u);

  puma_service_->EnableReporting();

  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 0);

  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaLogRotationOutcome,
      PumaService::LogRotationOutcome::kLogRotationSkipped, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 1);

  EXPECT_EQ(GetUnsentLogCount(), 1u);
  EXPECT_EQ(GetPersistedLogCount(), 1u);

  EXPECT_NE(client_.uploader(), nullptr);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  EXPECT_EQ(GetUnsentLogCount(), 0u);
  EXPECT_EQ(GetPersistedLogCount(), 0u);

  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1u);
}

TEST_P(PumaServiceRcTest, LogsUploadedPeriodically) {
  puma_service_->EnableReporting();

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean11",
                       true);

  EXPECT_EQ(GetUnsentLogCount(), 0u);

  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 0);

  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaLogRotationOutcome,
      PumaService::LogRotationOutcome::kLogRotationPerformed, 1);
  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 1);

  EXPECT_EQ(GetUnsentLogCount(), 1u);

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);

  EXPECT_EQ(GetUnsentLogCount(), 0u);

  PumaHistogramBoolean(PumaType::kRc, "PUMA.PumaServiceTestHistogram.Boolean12",
                       true);

  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 1);

  task_environment_.FastForwardBy(upload_interval);

  histogram_tester_.ExpectBucketCount(
      kHistogramPumaLogRotationOutcome,
      PumaService::LogRotationOutcome::kLogRotationPerformed, 2);
  histogram_tester_.ExpectTotalCount(kHistogramPumaLogRotationOutcome, 2);

  EXPECT_EQ(GetUnsentLogCount(), 1u);

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);

  EXPECT_EQ(GetUnsentLogCount(), 0u);
}

struct ExpectedReportBucket {
  int64_t min;
  int64_t max;
  int64_t count;
};

const metrics::HistogramEventProto* FindReportEvent(
    const ::private_metrics::ProfileKeyedHistogramEvent& profile_keyed_event,
    uint64_t name_hash) {
  for (const auto& event : profile_keyed_event.histogram_events()) {
    if (event.name_hash() == name_hash) {
      return &event;
    }
  }
  return nullptr;
}

const metrics::HistogramEventProto::Bucket* FindReportBucket(
    const metrics::HistogramEventProto& event,
    int64_t min,
    int64_t max) {
  for (int i = 0; i < event.bucket_size(); ++i) {
    const auto& bucket = event.bucket(i);
    if (bucket.min() == min && bucket.max() == max) {
      return &bucket;
    }
  }
  return nullptr;
}

void ExpectReportEvent(
    const ::private_metrics::ProfileKeyedHistogramEvent& profile_keyed_event,
    uint64_t name_hash,
    int64_t expected_sum,
    const std::vector<ExpectedReportBucket>& expected_buckets) {
  const auto* event = FindReportEvent(profile_keyed_event, name_hash);
  ASSERT_NE(event, nullptr) << "Event not found for name_hash=" << name_hash;
  EXPECT_EQ(event->sum(), expected_sum);
  ASSERT_EQ(event->bucket_size(), static_cast<int>(expected_buckets.size()));
  for (const auto& eb : expected_buckets) {
    const auto* bucket = FindReportBucket(*event, eb.min, eb.max);
    ASSERT_NE(bucket, nullptr)
        << "Bucket [" << eb.min << ", " << eb.max << ") not found";
    EXPECT_EQ(bucket->count(), eb.count);
  }
}

class PumaServiceWithLomFeatureTest : public PumaServiceTest {
 public:
  PumaServiceWithLomFeatureTest() = default;

  PumaServiceWithLomFeatureTest(const PumaServiceWithLomFeatureTest&) = delete;
  PumaServiceWithLomFeatureTest& operator=(
      const PumaServiceWithLomFeatureTest&) = delete;

  ~PumaServiceWithLomFeatureTest() override = default;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    return {kPrivateMetricsPuma, kPrivateMetricsPumaRc, kLomFeature};
  }
};

TEST_F(PumaServiceWithLomFeatureTest, BuildReportWithLomFeature) {
  PumaHistogramBoolean(PumaType::kRc,
                       "PUMA.PumaServiceTestHistogram.LomBoolean", true);
  PumaHistogramBoolean(PumaType::kRc,
                       "PUMA.PumaServiceTestHistogram.LomBoolean", false);
  PumaHistogramExactLinear(PumaType::kRc,
                           "PUMA.PumaServiceTestHistogram.LomLinear", 2, 10);
  PumaHistogramExactLinear(PumaType::kRc,
                           "PUMA.PumaServiceTestHistogram.LomLinear", 3, 10);
  PumaHistogramExactLinear(PumaType::kRc,
                           "PUMA.PumaServiceTestHistogram.LomLinear", 3, 10);
  PumaHistogramExactLinear(PumaType::kRc,
                           "PUMA.PumaServiceTestHistogram.LomLinear", 4, 10);

  auto report = puma_service_->BuildPrivateMetricRcReport();
  ASSERT_TRUE(report.has_value());
  ASSERT_EQ(report->profile_keyed_histogram_events_size(), 1);

  const auto& profile_keyed_event = report->profile_keyed_histogram_events(0);
  EXPECT_EQ(profile_keyed_event.profile_id(), 0u);
  EXPECT_EQ(profile_keyed_event.histogram_events_size(), 2);

  ExpectReportEvent(
      profile_keyed_event,
      base::HashMetricName("PUMA.PumaServiceTestHistogram.LomBoolean"),
      /*expected_sum=*/1,
      {{std::numeric_limits<int64_t>::min(), 1, 1}, {1, 2, 1}});
  ExpectReportEvent(
      profile_keyed_event,
      base::HashMetricName("PUMA.PumaServiceTestHistogram.LomLinear"),
      /*expected_sum=*/12, {{2, 3, 1}, {3, 4, 2}, {4, 5, 1}});

  // Second build report should return nullopt as events were cleared.
  EXPECT_FALSE(puma_service_->BuildPrivateMetricRcReport().has_value());
}

TEST_F(PumaServiceWithLomFeatureTest, BuildReportWithLomFeatureAndProfile) {
  constexpr uint64_t kProfileId1 = 12345u;
  constexpr uint64_t kProfileId2 = 67890u;

  LomRecorder::Get()->RecordBoolean(PumaType::kRc,
                                    "PUMA.PumaServiceTestHistogram.ProfileBool",
                                    true, kProfileId1);
  LomRecorder::Get()->RecordBoolean(PumaType::kRc,
                                    "PUMA.PumaServiceTestHistogram.ProfileBool",
                                    true, kProfileId1);
  LomRecorder::Get()->RecordBoolean(PumaType::kRc,
                                    "PUMA.PumaServiceTestHistogram.ProfileBool",
                                    false, kProfileId1);
  LomRecorder::Get()->RecordExactLinear(
      PumaType::kRc, "PUMA.PumaServiceTestHistogram.ProfileLinear", 5, 10,
      kProfileId1);
  LomRecorder::Get()->RecordExactLinear(
      PumaType::kRc, "PUMA.PumaServiceTestHistogram.ProfileLinear", 7, 10,
      kProfileId1);

  auto report = puma_service_->BuildPrivateMetricRcReport();
  ASSERT_TRUE(report.has_value());
  ASSERT_EQ(report->profile_keyed_histogram_events_size(), 1);

  const auto& profile_keyed_event = report->profile_keyed_histogram_events(0);
  const uint64_t profile_id = profile_keyed_event.profile_id();
  EXPECT_EQ(profile_id, kProfileId1);
  EXPECT_EQ(profile_keyed_event.histogram_events_size(), 2);

  ExpectReportEvent(
      profile_keyed_event,
      base::HashMetricName("PUMA.PumaServiceTestHistogram.ProfileBool"),
      /*expected_sum=*/2,
      {{std::numeric_limits<int64_t>::min(), 1, 1}, {1, 2, 2}});
  ExpectReportEvent(
      profile_keyed_event,
      base::HashMetricName("PUMA.PumaServiceTestHistogram.ProfileLinear"),
      /*expected_sum=*/12, {{5, 6, 1}, {7, 8, 1}});

  // Second build report should return nullopt as events were cleared.
  EXPECT_FALSE(puma_service_->BuildPrivateMetricRcReport().has_value());

  // Record again for Profile1 to verify ID matches.
  LomRecorder::Get()->RecordBoolean(PumaType::kRc,
                                    "PUMA.PumaServiceTestHistogram.ProfileBool",
                                    true, kProfileId1);

  auto second_report = puma_service_->BuildPrivateMetricRcReport();
  ASSERT_TRUE(second_report.has_value());
  ASSERT_EQ(second_report->profile_keyed_histogram_events_size(), 1);
  EXPECT_EQ(second_report->profile_keyed_histogram_events(0).profile_id(),
            kProfileId1);

  // Third build report should return nullopt as events were cleared.
  EXPECT_FALSE(puma_service_->BuildPrivateMetricRcReport().has_value());

  // Record for Profile2 to verify profile differentiation.
  LomRecorder::Get()->RecordBoolean(PumaType::kRc,
                                    "PUMA.PumaServiceTestHistogram.ProfileBool",
                                    true, kProfileId2);

  auto third_report = puma_service_->BuildPrivateMetricRcReport();
  ASSERT_TRUE(third_report.has_value());
  ASSERT_EQ(third_report->profile_keyed_histogram_events_size(), 1);

  const uint64_t profile2_id =
      third_report->profile_keyed_histogram_events(0).profile_id();
  EXPECT_EQ(profile2_id, kProfileId2);
  EXPECT_NE(profile2_id, profile_id);

  // Fourth build report should return nullopt as events were cleared.
  EXPECT_FALSE(puma_service_->BuildPrivateMetricRcReport().has_value());

  // Record without a profile ID (global profile).
  LomRecorder::Get()->RecordBoolean(
      PumaType::kRc, "PUMA.PumaServiceTestHistogram.GlobalBool", true);

  auto fourth_report = puma_service_->BuildPrivateMetricRcReport();
  ASSERT_TRUE(fourth_report.has_value());
  ASSERT_EQ(fourth_report->profile_keyed_histogram_events_size(), 1);
  EXPECT_EQ(fourth_report->profile_keyed_histogram_events(0).profile_id(), 0u);
}

}  // namespace metrics::private_metrics
