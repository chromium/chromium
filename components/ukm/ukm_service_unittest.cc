// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/unsent_log_store_metrics_impl.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace ukm {

// Some arbitrary events used in tests.
using TestEvent1 = ukm::builders::PageLoad;
const char* kTestEvent1Metric1 =
    TestEvent1::kPaintTiming_NavigationToFirstContentfulPaintName;
const char* kTestEvent1Metric2 = TestEvent1::kNet_CacheBytesName;
using TestEvent2 = ukm::builders::Memory_Experimental;
const char* kTestEvent2Metric1 = TestEvent2::kArrayBufferName;
const char* kTestEvent2Metric2 = TestEvent2::kBlinkGCName;
using TestEvent3 = ukm::builders::Previews;

std::string Entry1And2Whitelist() {
  return std::string(TestEvent1::kEntryName) + ',' + TestEvent2::kEntryName;
}

// A small shim exposing UkmRecorder methods to tests.
class TestRecordingHelper {
 public:
  explicit TestRecordingHelper(UkmRecorder* recorder) : recorder_(recorder) {
    recorder_->DisableSamplingForTesting();
  }

  void UpdateSourceURL(SourceId source_id, const GURL& url) {
    recorder_->UpdateSourceURL(source_id, url);
  }

  void RecordNavigation(SourceId source_id,
                        const UkmSource::NavigationData& navigation_data) {
    recorder_->RecordNavigation(source_id, navigation_data);
  }

  void MarkSourceForDeletion(SourceId source_id) {
    recorder_->MarkSourceForDeletion(source_id);
  }

 private:
  UkmRecorder* recorder_;

  DISALLOW_COPY_AND_ASSIGN(TestRecordingHelper);
};

namespace {

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == "bhcnanendmgjjeghamaccjnochlnhcgj");
}

class ScopedUkmFeatureParams {
 public:
  explicit ScopedUkmFeatureParams(const base::FieldTrialParams& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(kUkmFeature,
                                                            params);
  }

  ~ScopedUkmFeatureParams() {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUkmFeatureParams);
};

class MockDemographicMetricsProvider
    : public metrics::UkmDemographicMetricsProvider {
 public:
  ~MockDemographicMetricsProvider() override {}

  // DemographicMetricsProvider:
  MOCK_METHOD1(ProvideSyncedUserNoisedBirthYearAndGenderToReport,
               void(ukm::Report* report));
};

class UkmServiceTest : public testing::Test {
 public:
  UkmServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());
    ClearPrefs();
  }

  void ClearPrefs() {
    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmUnsentLogStore);
  }

  int GetPersistedLogCount() {
    const base::ListValue* list_value =
        prefs_.GetList(prefs::kUkmUnsentLogStore);
    return list_value->GetSize();
  }

  Report GetPersistedReport() {
    EXPECT_GE(GetPersistedLogCount(), 1);
    metrics::UnsentLogStore result_unsent_log_store(
        std::make_unique<ukm::UnsentLogStoreMetricsImpl>(), &prefs_,
        prefs::kUkmUnsentLogStore,
        3,     // log count limit
        1000,  // byte limit
        0, std::string());

    result_unsent_log_store.LoadPersistedUnsentLogs();
    result_unsent_log_store.StageNextLog();

    std::string uncompressed_log_data;
    EXPECT_TRUE(compression::GzipUncompress(
      result_unsent_log_store.staged_log(), &uncompressed_log_data));

    Report report;
    EXPECT_TRUE(report.ParseFromString(uncompressed_log_data));
    return report;
  }

  static SourceId GetWhitelistedSourceId(int64_t id) {
    return ConvertToSourceId(id, SourceIdType::NAVIGATION_ID);
  }

  static SourceId GetNonWhitelistedSourceId(int64_t id) {
    return ConvertToSourceId(id, SourceIdType::UKM);
  }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmServiceTest);
};

}  // namespace

TEST_F(UkmServiceTest, ClientIdMigration) {
  prefs_.SetInt64(prefs::kUkmClientId, -1);
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  service.Initialize();
  uint64_t migrated_id = prefs_.GetUint64(prefs::kUkmClientId);
  // -1 migrates to the max UInt 64 value.
  EXPECT_EQ(migrated_id, 18446744073709551615ULL);
}

TEST_F(UkmServiceTest, EnableDisableSchedule) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  service.Initialize();
  EXPECT_FALSE(task_runner_->HasPendingTask());
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();
  EXPECT_TRUE(task_runner_->HasPendingTask());
  service.DisableReporting();
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(UkmServiceTest, PersistAndPurge) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Should init, generate a log, and start an upload for source.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Flushes the generated log to disk and generates a new entry.
  TestEvent1(id).Record(&service);
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 2);
  service.Purge();
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

TEST_F(UkmServiceTest, Purge) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  // Record some data
  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));
  TestEvent1(id).Record(&service);

  // Purge should delete data, so there shouldn't be anything left to upload.
  service.Purge();
  service.Flush();
  EXPECT_EQ(0, GetPersistedLogCount());
}

TEST_F(UkmServiceTest, PurgeExtensionDataFromUnsentLogStore) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  ukm::Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);

  std::string non_extension_url = "https://www.google.ca";
  std::string extension_url =
      "chrome-extension://bmnlcjabgnpnenekpadlanbbkooimhnj/manifest.json";

  // Add both extension- and non-extension-related sources to the Report.
  ukm::Source* proto_source_1 = report.add_sources();
  ukm::SourceId source_id_1 =
      ukm::ConvertToSourceId(1, ukm::SourceIdType::NAVIGATION_ID);
  proto_source_1->set_id(source_id_1);
  proto_source_1->add_urls()->set_url(non_extension_url);
  ukm::Source* proto_source_2 = report.add_sources();
  ukm::SourceId source_id_2 =
      ukm::ConvertToSourceId(2, ukm::SourceIdType::NAVIGATION_ID);
  proto_source_2->set_id(source_id_2);
  proto_source_2->add_urls()->set_url(extension_url);

  // Add some entries for both sources.
  ukm::Entry* entry_1 = report.add_entries();
  entry_1->set_source_id(source_id_2);
  ukm::Entry* entry_2 = report.add_entries();
  entry_2->set_source_id(source_id_1);
  ukm::Entry* entry_3 = report.add_entries();
  entry_3->set_source_id(source_id_2);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  unsent_log_store->StoreLog(serialized_log);

  // Do extension purging.
  service.PurgeExtensions();

  // Get the Report in the log store and verify extension-related data have been
  // filtered.
  unsent_log_store->StageNextLog();
  const std::string& compressed_log_data = unsent_log_store->staged_log();

  std::string uncompressed_log_data;
  compression::GzipUncompress(compressed_log_data, &uncompressed_log_data);
  ukm::Report filtered_report;
  filtered_report.ParseFromString(uncompressed_log_data);

  // Only proto_source_1  with non-extension URL is kept.
  EXPECT_EQ(1, filtered_report.sources_size());
  EXPECT_EQ(source_id_1, filtered_report.sources(0).id());
  EXPECT_EQ(non_extension_url, filtered_report.sources(0).urls(0).url());

  // Only entry_2 from the non-extension source is kept.
  EXPECT_EQ(1, filtered_report.entries_size());
  EXPECT_EQ(source_id_1, filtered_report.entries(0).source_id());
}

TEST_F(UkmServiceTest, SourceSerialization) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  UkmSource::NavigationData navigation_data;
  navigation_data.urls = {GURL("https://google.com/initial"),
                          GURL("https://google.com/final")};

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.RecordNavigation(id, navigation_data);

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  EXPECT_TRUE(proto_report.has_session_id());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(id, proto_source.id());
  EXPECT_EQ(GURL("https://google.com/final").spec(),
            proto_source.urls(1).url());
}

TEST_F(UkmServiceTest, AddEntryWithEmptyMetrics) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  ASSERT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  TestEvent1(id).Record(&service);
  service.Flush();
  ASSERT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.entries_size());
}

TEST_F(UkmServiceTest, MetricsProviderTest) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);

  metrics::TestMetricsProvider* provider = new metrics::TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(provider));

  service.Initialize();

  // Providers have not supplied system profile information yet.
  EXPECT_FALSE(provider->provide_system_profile_metrics_called());

  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  EXPECT_EQ(1, proto_report.entries_size());

  // Providers have now supplied system profile information.
  EXPECT_TRUE(provider->provide_system_profile_metrics_called());
}

TEST_F(UkmServiceTest, AddUserDemograhicsWhenAvailableAndFeatureEnabled) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      UkmService::kReportUserNoisedUserBirthYearAndGender);

  int number_of_invocations = 0;
  int test_birth_year = 1983;
  metrics::UserDemographicsProto::Gender test_gender =
      metrics::UserDemographicsProto::GENDER_FEMALE;

  auto provider = std::make_unique<MockDemographicMetricsProvider>();
  // Expect the synced user's noised birth year and gender to be added 2 times
  // to the UKM report: on the event trigger and when tearing down the UKM
  // service.
  EXPECT_CALL(*provider,
              ProvideSyncedUserNoisedBirthYearAndGenderToReport(testing::_))
      .Times(2)
      .WillRepeatedly([&number_of_invocations, test_gender,
                       test_birth_year](ukm::Report* report) {
        report->mutable_user_demographics()->set_birth_year(test_birth_year);
        report->mutable_user_demographics()->set_gender(test_gender);
        ++number_of_invocations;
      });

  UkmService service(&prefs_, &client_,
                     /*restrict_to_whitelisted_entries=*/true,
                     std::move(provider));
  TestRecordingHelper recorder(&service);

  service.Initialize();

  // Verify that the provider hasn't provided the synced user's noised birth
  // year and gender yet.
  EXPECT_EQ(0, number_of_invocations);

  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  // Verify that the synced user's noised birth year and gender were added to
  // the UKM report.
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(test_birth_year, proto_report.user_demographics().birth_year());
  EXPECT_EQ(test_gender, proto_report.user_demographics().gender());

  // Verify that the provider's method was only called once before the
  // destruction of the service.
  EXPECT_EQ(1, number_of_invocations);
}

TEST_F(UkmServiceTest,
       DontAddUserDemograhicsWhenNotAvailableAndFeatureEnabled) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndEnableFeature(
      UkmService::kReportUserNoisedUserBirthYearAndGender);

  auto provider = std::make_unique<MockDemographicMetricsProvider>();
  EXPECT_CALL(*provider,
              ProvideSyncedUserNoisedBirthYearAndGenderToReport(testing::_))
      .Times(2)
      .WillRepeatedly([](ukm::Report* report) {});

  UkmService service(&prefs_, &client_,
                     /*restrict_to_whitelisted_entries=*/true,
                     std::move(provider));
  TestRecordingHelper recorder(&service);
  service.Initialize();

  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  // Verify that the synced user's noised birth year and gender are not in the
  // report when they are not available.
  Report proto_report = GetPersistedReport();
  EXPECT_FALSE(proto_report.has_user_demographics());
}

TEST_F(UkmServiceTest, DontAddUserDemograhicsWhenFeatureDisabled) {
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  // The demographics provider should not be called.
  auto provider = std::make_unique<MockDemographicMetricsProvider>();
  EXPECT_CALL(*provider,
              ProvideSyncedUserNoisedBirthYearAndGenderToReport(testing::_))
      .Times(0);

  UkmService service(&prefs_, &client_,
                     /*restrict_to_whitelisted_entries=*/true,
                     std::move(provider));
  TestRecordingHelper recorder(&service);

  service.Initialize();

  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  // Verify that the synced user's noised birth year and gender are not in the
  // report when they are not available.
  Report proto_report = GetPersistedReport();
  EXPECT_FALSE(proto_report.has_user_demographics());
}

TEST_F(UkmServiceTest, LogsRotation) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  EXPECT_EQ(0, service.report_count());

  // Log rotation should generate a log.
  const ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, service.report_count());
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Rotation shouldn't generate a log due to one being pending.
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  task_runner_->RunPendingTasks();
  EXPECT_EQ(1, service.report_count());
  EXPECT_TRUE(client_.uploader()->is_uploading());

  // Completing the upload should clear pending log, then log rotation should
  // generate another log.
  client_.uploader()->CompleteUpload(200);
  task_runner_->RunPendingTasks();
  EXPECT_EQ(2, service.report_count());

  // Check that rotations keep working.
  for (int i = 3; i < 6; i++) {
    task_runner_->RunPendingTasks();
    client_.uploader()->CompleteUpload(200);
    recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
    task_runner_->RunPendingTasks();
    EXPECT_EQ(i, service.report_count());
  }
}

TEST_F(UkmServiceTest, LogsUploadedOnlyWhenHavingSourcesOrEntries) {
  // Testing two whitelisted Entries.
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  EXPECT_TRUE(task_runner_->HasPendingTask());
  // Neither rotation or Flush should generate logs
  task_runner_->RunPendingTasks();
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 0);

  ukm::SourceId id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Includes a Source, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  TestEvent1(id).Record(&service);
  // Includes an Entry, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 2);

  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  // Do not keep the source in the recorder after the current log.
  recorder.MarkSourceForDeletion(id);
  // Includes a Source and an Entry, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 3);

  // The recorder contains no Sources or Entries thus will not create a new log.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 3);
}

TEST_F(UkmServiceTest, GetNewSourceID) {
  ukm::SourceId id1 = UkmRecorder::GetNewSourceID();
  ukm::SourceId id2 = UkmRecorder::GetNewSourceID();
  ukm::SourceId id3 = UkmRecorder::GetNewSourceID();
  EXPECT_NE(id1, id2);
  EXPECT_NE(id1, id3);
  EXPECT_NE(id2, id3);
}

TEST_F(UkmServiceTest, RecordRedirectedUrl) {
  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  ukm::SourceId id = GetWhitelistedSourceId(0);
  UkmSource::NavigationData navigation_data;
  navigation_data.urls = {GURL("https://google.com/initial"),
                          GURL("https://google.com/final")};
  recorder.RecordNavigation(id, navigation_data);

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(id, proto_source.id());
  EXPECT_EQ(GURL("https://google.com/initial").spec(),
            proto_source.urls(0).url());
  EXPECT_EQ(GURL("https://google.com/final").spec(),
            proto_source.urls(1).url());
}

TEST_F(UkmServiceTest, RestrictToWhitelistedSourceIds) {
  const GURL kURL = GURL("https://example.com/");
  for (bool restrict_to_whitelisted_source_ids : {true, false}) {
    ScopedUkmFeatureParams params(
        {{"RestrictToWhitelistedSourceIds",
          restrict_to_whitelisted_source_ids ? "true" : "false"},
         {"WhitelistEntries", Entry1And2Whitelist()}});

    ClearPrefs();
    UkmService service(&prefs_, &client_,
                       true /* restrict_to_whitelisted_entries */,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);
    EXPECT_EQ(GetPersistedLogCount(), 0);
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording(/*extensions=*/false);
    service.EnableReporting();

    ukm::SourceId id1 = GetWhitelistedSourceId(0);
    recorder.UpdateSourceURL(id1, kURL);
    TestEvent1(id1).Record(&service);

    // Create a non-navigation-based sourceid, which should not be whitelisted.
    ukm::SourceId id2 = GetNonWhitelistedSourceId(1);
    recorder.UpdateSourceURL(id2, kURL);
    TestEvent1(id2).Record(&service);

    service.Flush();
    ASSERT_EQ(GetPersistedLogCount(), 1);
    Report proto_report = GetPersistedReport();
    ASSERT_GE(proto_report.sources_size(), 1);

    // The whitelisted source should always be recorded.
    const Source& proto_source1 = proto_report.sources(0);
    EXPECT_EQ(id1, proto_source1.id());
    EXPECT_EQ(kURL.spec(), proto_source1.urls(0).url());

    // The non-whitelisted source should only be recorded if we aren't
    // restricted to whitelisted source ids.
    if (restrict_to_whitelisted_source_ids) {
      ASSERT_EQ(1, proto_report.sources_size());
    } else {
      ASSERT_EQ(2, proto_report.sources_size());
      const Source& proto_source2 = proto_report.sources(1);
      EXPECT_EQ(id2, proto_source2.id());
      EXPECT_EQ(kURL.spec(), proto_source2.urls(0).url());
    }
  }
}

TEST_F(UkmServiceTest, RecordSessionId) {
  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  EXPECT_TRUE(proto_report.has_session_id());
  EXPECT_EQ(1, proto_report.report_id());
}

TEST_F(UkmServiceTest, SourceSize) {
  // Set a threshold of number of Sources via Feature Params.
  ScopedUkmFeatureParams params({{"MaxSources", "2"}});

  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));
  id = GetWhitelistedSourceId(1);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar2"));
  id = GetWhitelistedSourceId(2);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar3"));

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  // Note, 2 instead of 3 sources, since we overrode the max number of sources
  // via Feature params.
  EXPECT_EQ(2, proto_report.sources_size());
}

TEST_F(UkmServiceTest, PurgeMidUpload) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));
  // Should init, generate a log, and start an upload.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Purge should delete all logs, including the one being sent.
  service.Purge();
  // Upload succeeds after logs was deleted.
  client_.uploader()->CompleteUpload(200);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  EXPECT_FALSE(client_.uploader()->is_uploading());
}

TEST_F(UkmServiceTest, WhitelistEntryTest) {
  // Testing two whitelisted Entries.
  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));

  TestEvent1(id).Record(&service);
  TestEvent2(id).Record(&service);
  // Note that this third entry is not in the whitelist.
  TestEvent3(id).Record(&service);

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();

  // Verify we've added one source and 2 entries.
  EXPECT_EQ(1, proto_report.sources_size());
  ASSERT_EQ(2, proto_report.entries_size());

  const Entry& proto_entry_a = proto_report.entries(0);
  EXPECT_EQ(id, proto_entry_a.source_id());
  EXPECT_EQ(base::HashMetricName(TestEvent1::kEntryName),
            proto_entry_a.event_hash());

  const Entry& proto_entry_b = proto_report.entries(1);
  EXPECT_EQ(id, proto_entry_b.source_id());
  EXPECT_EQ(base::HashMetricName(TestEvent2::kEntryName),
            proto_entry_b.event_hash());
}

TEST_F(UkmServiceTest, SourceURLLength) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);

  // This URL is too long to be recorded fully.
  const std::string long_string =
      "https://example.com/" + std::string(10000, 'a');
  recorder.UpdateSourceURL(id, GURL(long_string));

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);
  EXPECT_EQ("URLTooLong", proto_source.urls(0).url());
}

TEST_F(UkmServiceTest, UnreferencedNonWhitelistedSources) {
  const GURL kURL("https://google.com/foobar");
  for (bool restrict_to_whitelisted_source_ids : {true, false}) {
    // Set a threshold of number of Sources via Feature Params.
    ScopedUkmFeatureParams params(
        {{"MaxKeptSources", "3"},
         {"WhitelistEntries", Entry1And2Whitelist()},
         {"RestrictToWhitelistedSourceIds",
          restrict_to_whitelisted_source_ids ? "true" : "false"}});

    ClearPrefs();
    UkmService service(&prefs_, &client_,
                       true /* restrict_to_whitelisted_entries */,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);
    EXPECT_EQ(0, GetPersistedLogCount());
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording(/*extensions=*/false);
    service.EnableReporting();

    // Record with whitelisted ID to whitelist the URL.
    // Use a larger ID to make it last in the proto.
    ukm::SourceId whitelisted_id = GetWhitelistedSourceId(100);
    recorder.UpdateSourceURL(whitelisted_id, kURL);

    std::vector<SourceId> ids;
    base::TimeTicks last_time = base::TimeTicks::Now();
    for (int i = 0; i < 6; ++i) {
      // Wait until base::TimeTicks::Now() no longer equals |last_time|. This
      // ensures each source has a unique timestamp to avoid flakes. Should take
      // between 1-15ms per documented resolution of base::TimeTicks.
      while (base::TimeTicks::Now() == last_time) {
        base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
      }

      ids.push_back(GetNonWhitelistedSourceId(i));
      recorder.UpdateSourceURL(ids.back(), kURL);
      last_time = base::TimeTicks::Now();
    }

    // Add whitelisted entries for 0, 2 and non-whitelisted entries for 2, 3.
    TestEvent1(ids[0]).Record(&service);
    TestEvent2(ids[2]).Record(&service);
    TestEvent3(ids[2]).Record(&service);
    TestEvent3(ids[3]).Record(&service);

    service.Flush();
    EXPECT_EQ(1, GetPersistedLogCount());
    auto proto_report = GetPersistedReport();

    // The non-whitelisted source should only be recorded if we aren't
    // restricted to whitelisted source ids.
    if (restrict_to_whitelisted_source_ids) {
      // Only the one whitelisted source (whitelisted_id) is recorded.
      EXPECT_EQ(1, proto_report.source_counts().observed());
      // The one whitelisted source is of navigation type.
      EXPECT_EQ(1, proto_report.source_counts().navigation_sources());
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      // The one whitelisted source is also deferred for inclusion in future
      // reports.
      EXPECT_EQ(1, proto_report.source_counts().deferred_sources());
      EXPECT_EQ(0, proto_report.source_counts().carryover_sources());

      ASSERT_EQ(1, proto_report.sources_size());
    } else {
      // 1 whitelisted source and 6 non-whitelisted source.
      EXPECT_EQ(7, proto_report.source_counts().observed());
      // The one whitelisted source is of navigation type.
      EXPECT_EQ(1, proto_report.source_counts().navigation_sources());
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      // Source 0 of navigation type, and entryless sources 1, 3, 4, 5 of
      // non-whitelisted type are eligible to be deferred, but MaxKeptSources
      // restricts deferral to the 3 latest created ones.
      EXPECT_EQ(3, proto_report.source_counts().deferred_sources());
      EXPECT_EQ(0, proto_report.source_counts().carryover_sources());

      ASSERT_EQ(3, proto_report.sources_size());
      EXPECT_EQ(ids[0], proto_report.sources(0).id());
      EXPECT_EQ(kURL.spec(), proto_report.sources(0).urls(0).url());
      EXPECT_EQ(ids[2], proto_report.sources(1).id());
      EXPECT_EQ(kURL.spec(), proto_report.sources(1).urls(0).url());
    }
    // Since MaxKeptSources is 3, only Sources 5, 4, 3 should be retained.
    // Log entries under 0, 1, 3 and 4. Log them in reverse order - which
    // shouldn't affect source ordering in the output.
    //  - Source 0 should not be re-transmitted since it was sent before.
    //  - Source 1 should not be transmitted due to MaxKeptSources param.
    //  - Sources 3 and 4 should be transmitted since they were not sent before.
    TestEvent1(ids[4]).Record(&service);
    TestEvent1(ids[3]).Record(&service);
    TestEvent1(ids[1]).Record(&service);
    TestEvent1(ids[0]).Record(&service);

    service.Flush();
    EXPECT_EQ(2, GetPersistedLogCount());
    proto_report = GetPersistedReport();

    // The non-whitelisted source should only be recorded if we aren't
    // restricted to whitelisted source ids.
    if (restrict_to_whitelisted_source_ids) {
      EXPECT_EQ(0, proto_report.source_counts().observed());
      EXPECT_EQ(0, proto_report.source_counts().navigation_sources());
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      // The one whitelisted source is deferred again in future reports.
      EXPECT_EQ(1, proto_report.source_counts().deferred_sources());
      // Number of sources carried over from the previous report to this report.
      EXPECT_EQ(1, proto_report.source_counts().carryover_sources());
      // Only the navigation source is again included in current report and
      // there is a new entry associated to it.
      ASSERT_EQ(1, proto_report.sources_size());
    } else {
      EXPECT_EQ(0, proto_report.source_counts().observed());
      EXPECT_EQ(0, proto_report.source_counts().navigation_sources());
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      // Only the navigation type source is deferred.
      EXPECT_EQ(1, proto_report.source_counts().deferred_sources());
      // Number of sources carried over from the previous report to this report.
      EXPECT_EQ(3, proto_report.source_counts().carryover_sources());
      // Out of sources 3, 4, 5 that were retained from the previous cycle,
      // sources 3 and 4 got new entries are thus included in this report.
      ASSERT_EQ(2, proto_report.sources_size());
      EXPECT_EQ(ids[3], proto_report.sources(0).id());
      EXPECT_EQ(kURL.spec(), proto_report.sources(0).urls(0).url());
      EXPECT_EQ(ids[4], proto_report.sources(1).id());
      EXPECT_EQ(kURL.spec(), proto_report.sources(1).urls(0).url());
    }
  }
}

TEST_F(UkmServiceTest, NonWhitelistedUrls) {
  // URL to be manually whitelisted using whitelisted source type.
  const GURL kURL("https://google.com/foobar");
  struct {
    GURL url;
    bool expect_in_report;
  } test_cases[] = {
      {GURL("https://google.com/foobar"), true},
      // For origin-only URLs, only the origin needs to be matched.
      {GURL("https://google.com"), true},
      {GURL("https://google.com/foobar2"), false},
      {GURL("https://other.com"), false},
  };

  ScopedUkmFeatureParams params({{"WhitelistEntries", Entry1And2Whitelist()}});

  for (const auto& test : test_cases) {
    ClearPrefs();
    UkmService service(&prefs_, &client_,
                       true /* restrict_to_whitelisted_entries */,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);

    ASSERT_EQ(GetPersistedLogCount(), 0);
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording(/*extensions=*/false);
    service.EnableReporting();

    // Record with whitelisted ID to whitelist the URL.
    ukm::SourceId whitelist_id = GetWhitelistedSourceId(1);
    recorder.UpdateSourceURL(whitelist_id, kURL);

    // Record non whitelisted ID with an entry.
    ukm::SourceId nonwhitelist_id = GetNonWhitelistedSourceId(100);
    recorder.UpdateSourceURL(nonwhitelist_id, test.url);
    TestEvent1(nonwhitelist_id).Record(&service);

    service.Flush();
    ASSERT_EQ(1, GetPersistedLogCount());
    auto proto_report = GetPersistedReport();

    EXPECT_EQ(2, proto_report.source_counts().observed());
    EXPECT_EQ(1, proto_report.source_counts().navigation_sources());

    // If the source id is not whitelisted, don't send it unless it has
    // associated entries and the URL matches that of the whitelisted source.
    if (test.expect_in_report) {
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(2, proto_report.sources_size());
      EXPECT_EQ(whitelist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
      EXPECT_EQ(nonwhitelist_id, proto_report.sources(1).id());
      EXPECT_EQ(test.url, proto_report.sources(1).urls(0).url());
    } else {
      EXPECT_EQ(1, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(1, proto_report.sources_size());
      EXPECT_EQ(whitelist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
    }

    // Do a log rotation again, with the same test URL associated to a new
    // source id. Since the previous source id of the test case is of
    // non-whitelisted type, the carryover URLs list is expected to remain
    // be unchanged, thus the the report should still contain the same numbers
    // of sources as before, that is, non-whitelisted URLs should not have
    // whitelisted themselves during the previous log rotation.
    ukm::SourceId nonwhitelist_id2 = GetNonWhitelistedSourceId(101);
    recorder.UpdateSourceURL(nonwhitelist_id2, test.url);
    TestEvent1(nonwhitelist_id2).Record(&service);
    service.Flush();
    ASSERT_EQ(2, GetPersistedLogCount());
    proto_report = GetPersistedReport();

    if (test.expect_in_report) {
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(2, proto_report.sources_size());
      EXPECT_EQ(whitelist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
      EXPECT_EQ(nonwhitelist_id2, proto_report.sources(1).id());
      EXPECT_EQ(test.url, proto_report.sources(1).urls(0).url());
    } else {
      EXPECT_EQ(1, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(1, proto_report.sources_size());
      EXPECT_EQ(whitelist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
    }
  }
}

TEST_F(UkmServiceTest, SupportedSchemes) {
  struct {
    const char* url;
    bool expected_kept;
  } test_cases[] = {
      {"http://google.ca/", true},
      {"https://google.ca/", true},
      {"ftp://google.ca/", true},
      {"about:blank", true},
      {"chrome://version/", true},
      {"app://play/abcdefghijklmnopqrstuvwxyzabcdef/", true},
      // chrome-extension are controlled by TestIsWebstoreExtension, above.
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/", true},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/", false},
      {"file:///tmp/", false},
      {"abc://google.ca/", false},
      {"www.google.ca/", false},
  };

  ScopedUkmFeatureParams params({});
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  service.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/true);
  service.EnableReporting();

  int64_t id_counter = 1;
  int expected_kept_count = 0;
  for (const auto& test : test_cases) {
    auto source_id = GetWhitelistedSourceId(id_counter++);
    recorder.UpdateSourceURL(source_id, GURL(test.url));
    TestEvent1(source_id).Record(&service);
    if (test.expected_kept)
      ++expected_kept_count;
  }

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);
  Report proto_report = GetPersistedReport();

  EXPECT_EQ(expected_kept_count, proto_report.sources_size());
  for (const auto& test : test_cases) {
    bool found = false;
    for (int i = 0; i < proto_report.sources_size(); ++i) {
      if (proto_report.sources(i).urls(0).url() == test.url) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(test.expected_kept, found) << test.url;
  }
}

TEST_F(UkmServiceTest, SupportedSchemesNoExtensions) {
  struct {
    const char* url;
    bool expected_kept;
  } test_cases[] = {
      {"http://google.ca/", true},
      {"https://google.ca/", true},
      {"ftp://google.ca/", true},
      {"about:blank", true},
      {"chrome://version/", true},
      {"app://play/abcdefghijklmnopqrstuvwxyzabcdef/", true},
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/", false},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/", false},
      {"file:///tmp/", false},
      {"abc://google.ca/", false},
      {"www.google.ca/", false},
  };

  ScopedUkmFeatureParams params({});
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  int64_t id_counter = 1;
  int expected_kept_count = 0;
  for (const auto& test : test_cases) {
    auto source_id = GetWhitelistedSourceId(id_counter++);
    recorder.UpdateSourceURL(source_id, GURL(test.url));
    TestEvent1(source_id).Record(&service);
    if (test.expected_kept)
      ++expected_kept_count;
  }

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);
  Report proto_report = GetPersistedReport();

  EXPECT_EQ(expected_kept_count, proto_report.sources_size());
  for (const auto& test : test_cases) {
    bool found = false;
    for (int i = 0; i < proto_report.sources_size(); ++i) {
      if (proto_report.sources(i).urls(0).url() == test.url) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(test.expected_kept, found) << test.url;
  }
}

TEST_F(UkmServiceTest, SanitizeUrlAuthParams) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelisted_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  auto id = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://username:password@example.com/"));

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);
  EXPECT_EQ("https://example.com/", proto_source.urls(0).url());
}

TEST_F(UkmServiceTest, SanitizeChromeUrlParams) {
  struct {
    const char* url;
    const char* expected_url;
  } test_cases[] = {
      {"chrome://version/?foo=bar", "chrome://version/"},
      {"about:blank?foo=bar", "about:blank"},
      {"chrome://histograms/Variations", "chrome://histograms/Variations"},
      {"http://google.ca/?foo=bar", "http://google.ca/?foo=bar"},
      {"https://google.ca/?foo=bar", "https://google.ca/?foo=bar"},
      {"ftp://google.ca/?foo=bar", "ftp://google.ca/?foo=bar"},
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/foo.html?a=b",
       "chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/"},
  };

  for (const auto& test : test_cases) {
    ClearPrefs();

    UkmService service(&prefs_, &client_,
                       true /* restrict_to_whitelisted_entries */,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);
    service.SetIsWebstoreExtensionCallback(
        base::BindRepeating(&TestIsWebstoreExtension));

    EXPECT_EQ(0, GetPersistedLogCount());
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording(/*extensions=*/true);
    service.EnableReporting();

    auto id = GetWhitelistedSourceId(0);
    recorder.UpdateSourceURL(id, GURL(test.url));

    service.Flush();
    EXPECT_EQ(1, GetPersistedLogCount());

    auto proto_report = GetPersistedReport();
    ASSERT_EQ(1, proto_report.sources_size());
    const Source& proto_source = proto_report.sources(0);
    EXPECT_EQ(test.expected_url, proto_source.urls(0).url());
  }
}

TEST_F(UkmServiceTest, MarkSourceForDeletion) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelist_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  // Seed some dummy sources.
  SourceId id0 = GetWhitelistedSourceId(0);
  recorder.UpdateSourceURL(id0, GURL("https://www.example0.com/"));
  SourceId id1 = GetWhitelistedSourceId(1);
  recorder.UpdateSourceURL(id1, GURL("https://www.example1.com/"));
  SourceId id2 = GetWhitelistedSourceId(2);
  recorder.UpdateSourceURL(id2, GURL("https://www.example2.com/"));

  service.Flush();
  int logs_count = 0;
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  // All sources are present in the report.
  Report proto_report = GetPersistedReport();
  ASSERT_EQ(3, proto_report.sources_size());
  EXPECT_EQ(id0, proto_report.sources(0).id());
  EXPECT_EQ(id1, proto_report.sources(1).id());
  EXPECT_EQ(id2, proto_report.sources(2).id());

  // Mark source 1 for deletion. Next report will still contain source 1 because
  // we might have associated entries. It will no longer be in further report at
  // the following cycle.
  service.MarkSourceForDeletion(id1);
  service.Flush();
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  proto_report = GetPersistedReport();
  ASSERT_EQ(3, proto_report.sources_size());

  service.Flush();
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  proto_report = GetPersistedReport();
  ASSERT_EQ(2, proto_report.sources_size());
  EXPECT_EQ(id0, proto_report.sources(0).id());
  EXPECT_EQ(id2, proto_report.sources(1).id());
}

TEST_F(UkmServiceTest, PurgeNonNavigationSources) {
  UkmService service(&prefs_, &client_,
                     true /* restrict_to_whitelist_entries */,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording(/*extensions=*/false);
  service.EnableReporting();

  // Seed some dummy sources.
  SourceId id0 = ConvertToSourceId(0, SourceIdType::UKM);
  recorder.UpdateSourceURL(id0, GURL("https://www.example0.com/"));
  SourceId id1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(id1, GURL("https://www.example1.com/"));
  SourceId id2 = ConvertToSourceId(2, SourceIdType::APP_ID);
  recorder.UpdateSourceURL(id2, GURL("https://www.example2.com/"));
  SourceId id3 = ConvertToSourceId(3, SourceIdType::HISTORY_ID);
  recorder.UpdateSourceURL(id3, GURL("https://www.example3.com/"));

  service.Flush();
  int logs_count = 0;
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  // All sources are present except id0 of non-whitelisted UKM type.
  Report proto_report = GetPersistedReport();
  ASSERT_EQ(3, proto_report.sources_size());
  EXPECT_EQ(id1, proto_report.sources(0).id());
  EXPECT_EQ(id2, proto_report.sources(1).id());
  EXPECT_EQ(id3, proto_report.sources(2).id());

  service.Flush();
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  // Sources of APP_ID and HISTORY_ID types are not kept between reporting
  // cycles, thus only 1 navigation type source remains.
  proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.sources_size());
  EXPECT_EQ(id1, proto_report.sources(0).id());
}

}  // namespace ukm
