// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time_override.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_pref_names.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/private_metrics/private_metrics_features.h"
#include "components/metrics/private_metrics/private_metrics_pref_names.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/cose.h"
#include "third_party/federated_compute/src/fcp/confidentialcompute/crypto.h"

namespace metrics::dwa {
namespace {

const char kDwaInitSequenceHistogramName[] = "DWA.InitSequence";

class DwaServiceTest : public testing::Test {
 public:
  DwaServiceTest() {
    DwaRecorder::Get()->Purge();

    MetricsStateManager::RegisterPrefs(prefs_.registry());
    DwaService::RegisterPrefs(prefs_.registry());

    scoped_feature_list_.InitWithFeatures(
        {dwa::kDwaFeature, private_metrics::kPrivateMetricsFeature}, {});
  }

  DwaServiceTest(const DwaServiceTest&) = delete;
  DwaServiceTest& operator=(const DwaServiceTest&) = delete;

  ~DwaServiceTest() override = default;

  void TearDown() override { DwaRecorder::Get()->Purge(); }

  void SetEncryptionPublicKeyForTesting(DwaService* dwa_service) {
    fcp::confidential_compute::MessageDecryptor decryptor;
    auto recipient_public_key =
        decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
    dwa_service->SetEncryptionPublicKeyForTesting(recipient_public_key.value());
    dwa_service->SetEncryptionPublicKeyVerifierForTesting(base::BindRepeating(
        [](const fcp::confidential_compute::OkpCwt&) -> bool { return true; }));
  }

  int GetPersistedLogCount() {
    if (base::FeatureList::IsEnabled(private_metrics::kPrivateMetricsFeature)) {
      return prefs_.GetList(private_metrics::prefs::kUnsentLogStoreName).size();
    }
    return prefs_.GetList(prefs::kUnsentLogStoreName).size();
  }

 protected:
  // Check that the values in |coarse_system_info| are filled in and expected
  // ones correspond to the test data.
  void CheckCoarseSystemInformation(
      const ::dwa::CoarseSystemInfo& coarse_system_info) {
    EXPECT_TRUE(coarse_system_info.has_channel());
    // TestMetricsServiceClient::GetChannel() returns CHANNEL_BETA, so we should
    // expect |coarse_system_info| channel to be "NOT_STABLE".
    EXPECT_EQ(coarse_system_info.channel(),
              ::dwa::CoarseSystemInfo::CHANNEL_NOT_STABLE);
    EXPECT_TRUE(coarse_system_info.has_platform());
    EXPECT_TRUE(coarse_system_info.has_geo_designation());
    EXPECT_TRUE(coarse_system_info.has_client_age());
    EXPECT_TRUE(coarse_system_info.has_milestone_prefix_trimmed());
    EXPECT_TRUE(coarse_system_info.has_is_ukm_enabled());
  }

  // Records a test data DWA metric into DwaRecorder.
  void RecordTestMetric() {
    ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
    builder.SetContent("https://adtech.com");
    builder.AddToStudiesOfInterest("test_trial_1");
    builder.AddToStudiesOfInterest("test_trial_2");
    builder.SetMetric("Length", 5);
    builder.Record(DwaRecorder::Get());
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DwaServiceEnvironmentTest : public DwaServiceTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(DwaServiceTest, RecordCoarseSystemInformation) {
  TestingPrefServiceSimple pref_service;
  MetricsStateManager::RegisterPrefs(pref_service.registry());
  ::dwa::CoarseSystemInfo coarse_system_info;
  DwaService::RecordCoarseSystemInformation(client_, pref_service,
                                            &coarse_system_info);
  CheckCoarseSystemInformation(coarse_system_info);
}

TEST_F(DwaServiceTest, ClientIdIsGenerated) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  base::Time expected_time_at_daily_resolution;
  EXPECT_TRUE(base::Time::FromString("1 May 2024 00:00:00 GMT",
                                     &expected_time_at_daily_resolution));

  uint64_t client_id;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_GT(pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId), 0u);
  EXPECT_EQ(client_id,
            pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId));
  EXPECT_EQ(expected_time_at_daily_resolution,
            pref_service.GetTime(metrics::dwa::prefs::kDwaClientIdLastUpdated));
}

TEST_F(DwaServiceTest, ClientIdOnlyChangesBetweenDays) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  uint64_t client_id_day1;
  uint64_t client_id_day2;
  uint64_t client_id_day2_later;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day1 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 16:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2_later = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_NE(client_id_day1, client_id_day2);
  EXPECT_EQ(client_id_day2, client_id_day2_later);
}

TEST_F(DwaServiceTest, HashCoarseSystemInfoCreatesPersistentHash) {
  ::dwa::CoarseSystemInfo coarse_system_info_1;
  coarse_system_info_1.set_channel(::dwa::CoarseSystemInfo::CHANNEL_STABLE);
  coarse_system_info_1.set_platform(::dwa::CoarseSystemInfo::PLATFORM_WINDOWS);
  coarse_system_info_1.set_geo_designation(
      ::dwa::CoarseSystemInfo::GEO_DESIGNATION_ROW);
  coarse_system_info_1.set_client_age(
      ::dwa::CoarseSystemInfo::CLIENT_AGE_RECENT);
  coarse_system_info_1.set_milestone_prefix_trimmed(8);
  coarse_system_info_1.set_is_ukm_enabled(true);
  EXPECT_THAT(DwaService::HashCoarseSystemInfo(coarse_system_info_1),
              testing::Eq(5379665033289076337u));

  ::dwa::CoarseSystemInfo coarse_system_info_2;
  coarse_system_info_2.set_channel(::dwa::CoarseSystemInfo::CHANNEL_STABLE);
  coarse_system_info_2.set_platform(::dwa::CoarseSystemInfo::PLATFORM_WINDOWS);
  coarse_system_info_2.set_geo_designation(
      ::dwa::CoarseSystemInfo::GEO_DESIGNATION_ROW);
  coarse_system_info_2.set_client_age(
      ::dwa::CoarseSystemInfo::CLIENT_AGE_RECENT);
  coarse_system_info_2.set_milestone_prefix_trimmed(9);
  coarse_system_info_2.set_is_ukm_enabled(true);
  EXPECT_THAT(DwaService::HashCoarseSystemInfo(coarse_system_info_2),
              testing::Eq(150860663309450601u));

  ::dwa::CoarseSystemInfo coarse_system_info_3;
  coarse_system_info_3.set_channel(::dwa::CoarseSystemInfo::CHANNEL_STABLE);
  coarse_system_info_3.set_platform(::dwa::CoarseSystemInfo::PLATFORM_LINUX);
  coarse_system_info_3.set_geo_designation(
      ::dwa::CoarseSystemInfo::GEO_DESIGNATION_EEA);
  coarse_system_info_3.set_client_age(
      ::dwa::CoarseSystemInfo::CLIENT_AGE_NOT_RECENT);
  coarse_system_info_3.set_milestone_prefix_trimmed(3);
  coarse_system_info_3.set_is_ukm_enabled(true);
  EXPECT_THAT(DwaService::HashCoarseSystemInfo(coarse_system_info_3),
              testing::Eq(5124987072588276635u));
}

TEST_F(DwaServiceTest, HashRepeatedFieldTrialsCreatesPersistentHash) {
  ::metrics::SystemProfileProto::FieldTrial field_trial_1;
  field_trial_1.set_name_id(0x11111111);
  field_trial_1.set_group_id(0x22222222);
  ::metrics::SystemProfileProto::FieldTrial field_trial_2;
  field_trial_2.set_name_id(0x11111111);
  field_trial_2.set_group_id(0x66666666);
  ::metrics::SystemProfileProto::FieldTrial field_trial_3;
  field_trial_3.set_name_id(0x33333333);
  field_trial_3.set_group_id(0x44444444);
  ::metrics::SystemProfileProto::FieldTrial field_trial_4;
  field_trial_4.set_name_id(0x55555555);
  field_trial_4.set_group_id(0x66666666);

  uint64_t expected_result_from_field_trial_1_and_field_trial_3 =
      784123498318573506u;

  struct {
    std::vector<::metrics::SystemProfileProto::FieldTrial> input;
    std::optional<uint64_t> expected_output;
  } test_cases[] = {
      {std::vector<::metrics::SystemProfileProto::FieldTrial>{field_trial_1,
                                                              field_trial_3},
       expected_result_from_field_trial_1_and_field_trial_3},
      {std::vector<::metrics::SystemProfileProto::FieldTrial>{field_trial_3,
                                                              field_trial_1},
       expected_result_from_field_trial_1_and_field_trial_3},
      {std::vector<::metrics::SystemProfileProto::FieldTrial>{field_trial_2,
                                                              field_trial_3},
       10506435849301764974u},
      {std::vector<::metrics::SystemProfileProto::FieldTrial>{
           field_trial_1, field_trial_3, field_trial_4},
       13321506181621468176u},
  };

  for (const auto& test_case : test_cases) {
    google::protobuf::RepeatedPtrField<
        ::metrics::SystemProfileProto::FieldTrial>
        repeated_ptr_field;
    repeated_ptr_field.Add(std::make_move_iterator(test_case.input.begin()),
                           std::make_move_iterator(test_case.input.end()));

    EXPECT_THAT(DwaService::HashRepeatedFieldTrials(repeated_ptr_field),
                testing::Eq(test_case.expected_output));
  }
}

TEST_F(DwaServiceTest, BuildsKAnonymityBuckets) {
  base::FieldTrialList::CreateFieldTrial("test_trial_1", "test_group_2")
      ->Activate();
  base::FieldTrialList::CreateFieldTrial("test_trial_2", "test_group_1")
      ->Activate();
  DwaRecorder::Get()->EnableRecording();

  // Records a test metric and generate a vector of k_anonymity_buckets values.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());

  auto dwa_events = DwaRecorder::Get()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 1u);

  auto k_anonymity_buckets =
      DwaService::BuildKAnonymityBuckets(dwa_events.at(0));
  EXPECT_FALSE(k_anonymity_buckets.empty());
  ASSERT_EQ(k_anonymity_buckets.size(), 2u);
  auto previous_bucket_value_0 = k_anonymity_buckets.at(0);
  auto previous_bucket_value_1 = k_anonymity_buckets.at(1);

  // Records another test metric and validate the two vector of
  // k_anonymity_buckets values match.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());

  dwa_events = DwaRecorder::Get()->TakeDwaEvents();
  EXPECT_FALSE(dwa_events.empty());
  ASSERT_EQ(dwa_events.size(), 1u);

  k_anonymity_buckets = DwaService::BuildKAnonymityBuckets(dwa_events.at(0));
  EXPECT_FALSE(k_anonymity_buckets.empty());
  ASSERT_EQ(k_anonymity_buckets.size(), 2u);
  ASSERT_EQ(previous_bucket_value_0, k_anonymity_buckets.at(0));
  ASSERT_EQ(previous_bucket_value_1, k_anonymity_buckets.at(1));
}

TEST_F(DwaServiceTest, ValidateEncryptionPublicKey) {
  fcp::confidential_compute::MessageDecryptor decryptor;
  auto recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_TRUE(recipient_public_key.ok());

  auto cwt = fcp::confidential_compute::OkpCwt::Decode(*recipient_public_key);
  ASSERT_TRUE(cwt.ok());

  // Validate DwaService::ValidateEncryptionPublicKey() returns false when key
  // is already expired.
  auto now = absl::Now();
  cwt->expiration_time = std::make_optional(now - absl::Hours(8));
  EXPECT_FALSE(DwaService::ValidateEncryptionPublicKey(*cwt));

  // Validate DwaService::ValidateEncryptionPublicKey() returns false when key
  // is close to expiring (less than 12 hours).
  cwt->expiration_time = std::make_optional(now + absl::Hours(11));
  EXPECT_FALSE(DwaService::ValidateEncryptionPublicKey(*cwt));

  // Validate DwaService::ValidateEncryptionPublicKey() returns true when key
  // valid and far from expiring (more than 12 hours).
  cwt->expiration_time = std::make_optional(now + absl::Hours(13));
  EXPECT_TRUE(DwaService::ValidateEncryptionPublicKey(*cwt));
}

TEST_F(DwaServiceTest, EncryptPrivateMetricReport) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  fcp::confidential_compute::MessageDecryptor decryptor;
  auto recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  auto decoded_public_key =
      fcp::confidential_compute::OkpCwt::Decode(*recipient_public_key);
  decoded_public_key->public_key.value().key_id = "key-id";

  ::private_metrics::PrivateMetricReport report;
  report.set_ephemeral_id(DwaService::GetEphemeralClientId(pref_service));
  auto epoch_id = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  report.set_epoch_id(epoch_id);

  auto encrypted_report = DwaService::EncryptPrivateMetricReport(
      report, *recipient_public_key, *decoded_public_key);
  ASSERT_TRUE(encrypted_report.has_value());

  EXPECT_TRUE(encrypted_report->has_encrypted_report());
  EXPECT_TRUE(encrypted_report->has_serialized_report_header());
  EXPECT_TRUE(encrypted_report->has_report_header());
  EXPECT_TRUE(encrypted_report->has_report_type());
}

TEST_F(DwaServiceTest, BuildPrivateMetricEndpointPayloadFromEncryptedReport) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  fcp::confidential_compute::MessageDecryptor decryptor;
  auto recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  auto decoded_public_key =
      fcp::confidential_compute::OkpCwt::Decode(*recipient_public_key);
  decoded_public_key->public_key.value().key_id = "key-id";

  ::private_metrics::PrivateMetricReport report;
  report.set_ephemeral_id(DwaService::GetEphemeralClientId(pref_service));
  auto epoch_id = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  report.set_epoch_id(epoch_id);

  auto encrypted_report = DwaService::EncryptPrivateMetricReport(
      report, *recipient_public_key, *decoded_public_key);
  ASSERT_TRUE(encrypted_report.has_value());

  auto payload =
      DwaService::BuildPrivateMetricEndpointPayloadFromEncryptedReport(
          std::move(encrypted_report.value()));
  ASSERT_TRUE(payload.has_value());
  EXPECT_TRUE(payload->has_encrypted_private_metric_report());
  EXPECT_TRUE(
      payload->encrypted_private_metric_report().has_encrypted_report());
  EXPECT_TRUE(payload->encrypted_private_metric_report()
                  .has_serialized_report_header());
  EXPECT_TRUE(payload->encrypted_private_metric_report().has_report_header());
  EXPECT_TRUE(payload->has_report_type());
}

TEST_F(DwaServiceEnvironmentTest, Flush) {
  DwaService service(&client_, &prefs_, nullptr);
  SetEncryptionPublicKeyForTesting(&service);

  histogram_tester_.ExpectTotalCount(kDwaInitSequenceHistogramName,
                                     /*expected_count=*/1);
  DwaRecorder::Get()->EnableRecording();

  // Tests Flush() when there are entries.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);
}

TEST_F(DwaServiceEnvironmentTest, Purge) {
  DwaService service(&client_, &prefs_, nullptr);
  SetEncryptionPublicKeyForTesting(&service);

  histogram_tester_.ExpectTotalCount(kDwaInitSequenceHistogramName,
                                     /*expected_count=*/1);
  DwaRecorder::Get()->EnableRecording();

  // Test that Purge() removes all metrics.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  service.Purge();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());

  // Test that Purge() removes all logs.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  service.Purge();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

TEST_F(DwaServiceEnvironmentTest, EnableDisableRecordingAndReporting) {
  DwaService service(&client_, &prefs_, nullptr);
  SetEncryptionPublicKeyForTesting(&service);

  histogram_tester_.ExpectTotalCount(kDwaInitSequenceHistogramName,
                                     /*expected_count=*/1);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // When the reporting is enabled, the scheduler starts and creates tasks to
  // rotate logs.
  service.EnableReporting();
  EXPECT_GE(task_environment_.GetPendingMainThreadTaskCount(), 1u);

  service.DisableReporting();
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // Tests EnableRecording() records an entry.
  DwaRecorder::Get()->EnableRecording();
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // DisableRecording() and persist the above metric to disk.
  DwaRecorder::Get()->DisableRecording();
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  EXPECT_EQ(GetPersistedLogCount(), 1);
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());

  // Tests EnableRecording() with reporting_active() also starts
  // DwaReportingService when there are unsent logs on disk.
  service.EnableReporting();
  DwaRecorder::Get()->EnableRecording();
  // Validate there should be two pending tasks, one to rotate logs, and one to
  // upload unsent logs from disk.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 2u);

  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  // Logs are uploaded and there are no metrics in memory or on disk.
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // Cleans up state of DwaService for the test following.
  service.DisableReporting();
  service.Purge();

  // Tests DisableRecording() does not record an entry.
  DwaRecorder::Get()->DisableRecording();
  RecordTestMetric();
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
}

TEST_F(DwaServiceEnvironmentTest, LogsRotatedPeriodically) {
  DwaService service(&client_, &prefs_, nullptr);
  SetEncryptionPublicKeyForTesting(&service);

  histogram_tester_.ExpectTotalCount(kDwaInitSequenceHistogramName,
                                     /*expected_count=*/1);
  DwaRecorder::Get()->EnableRecording();
  service.EnableReporting();

  RecordTestMetric();

  // Metrics are stored in memory as entries in DwaRecorder, and there
  // are no unsent logs.
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  // Metrics are stored in memory as unsent logs, and DwaRecorder is empty.
  base::TimeDelta upload_interval = client_.GetUploadInterval();
  task_environment_.FastForwardBy(upload_interval);
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());
  // PersistedLogCount is 0 because logs are stored in memory and not on disk.
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Simulate logs upload.
  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  // Logs are uploaded and there are no metrics in memory or on disk.
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);

  // Checks there is another rotation scheduled when the previous one finished.
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 1u);

  // Repeat test above to validate the task scheduled is the task that rotates
  // logs.
  RecordTestMetric();
  EXPECT_TRUE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());

  task_environment_.FastForwardBy(upload_interval);
  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_TRUE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_TRUE(client_.uploader()->is_uploading());

  client_.uploader()->CompleteUpload(200);
  EXPECT_FALSE(client_.uploader()->is_uploading());

  EXPECT_FALSE(DwaRecorder::Get()->HasEntries());
  EXPECT_FALSE(service.unsent_log_store()->has_unsent_logs());
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

}  // namespace
}  // namespace metrics::dwa
