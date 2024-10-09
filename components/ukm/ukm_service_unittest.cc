// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test/test_metrics_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/observers/ukm_consent_state_observer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_entry_filter.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/ukm/ukm_recorder_observer.h"
#include "components/ukm/unsent_log_store_metrics_impl.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/ukm_recorder_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "third_party/metrics_proto/ukm/web_features.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace ukm {

// Some arbitrary events used in tests.
using TestEvent1 = builders::PageLoad;
const char* kTestEvent1Metric1 =
    TestEvent1::kPaintTiming_NavigationToFirstContentfulPaintName;
const char* kTestEvent1Metric2 = TestEvent1::kNet_CacheBytes2Name;
using TestEvent2 = builders::Memory_Experimental;
const char* kTestEvent2Metric1 = TestEvent2::kArrayBufferName;
const char* kTestEvent2Metric2 = TestEvent2::kBlinkGCName;
using TestEvent3 = builders::PageWithPassword;
using TestProviderEvent = builders::ScreenBrightness;
const int32_t kWebDXFeature1 = 1;
const int32_t kWebDXFeature3 = 3;
const size_t kWebDXFeatureNumberOfFeaturesForTesting = 5;

SourceId ConvertSourceIdToAllowlistedType(SourceId id, SourceIdType type) {
  return ukm::SourceIdObj::FromOtherId(id, type).ToInt64();
}

// A small shim exposing UkmRecorder methods to tests.
class TestRecordingHelper {
 public:
  explicit TestRecordingHelper(UkmRecorder* recorder) : recorder_(recorder) {
    recorder_->SetSamplingForTesting(1);
  }

  TestRecordingHelper(const TestRecordingHelper&) = delete;
  TestRecordingHelper& operator=(const TestRecordingHelper&) = delete;

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

  void RecordWebDXFeatures(SourceId source_id,
                           const std::set<int32_t>& features,
                           const size_t max_feature_value) {
    recorder_->RecordWebDXFeatures(source_id, features, max_feature_value);
  }

 private:
  raw_ptr<UkmRecorder> recorder_;
};

class TestMetricsServiceClientWithClonedInstallDetector
    : public metrics::TestMetricsServiceClient {
 public:
  TestMetricsServiceClientWithClonedInstallDetector() = default;

  TestMetricsServiceClientWithClonedInstallDetector(
      const TestMetricsServiceClientWithClonedInstallDetector&) = delete;
  TestMetricsServiceClientWithClonedInstallDetector& operator=(
      const TestMetricsServiceClientWithClonedInstallDetector&) = delete;

  ~TestMetricsServiceClientWithClonedInstallDetector() override = default;

  // metrics::MetricsServiceClient:
  base::CallbackListSubscription AddOnClonedInstallDetectedCallback(
      base::OnceClosure callback) override {
    return cloned_install_detector_.AddOnClonedInstallDetectedCallback(
        std::move(callback));
  }

  metrics::ClonedInstallDetector* cloned_install_detector() {
    return &cloned_install_detector_;
  }

 private:
  metrics::ClonedInstallDetector cloned_install_detector_;
};

namespace {

bool TestIsWebstoreExtension(std::string_view id) {
  return (id == "bhcnanendmgjjeghamaccjnochlnhcgj");
}

int GetPersistedLogCount(TestingPrefServiceSimple& prefs) {
  return prefs.GetList(prefs::kUkmUnsentLogStore).size();
}

Report GetPersistedReport(TestingPrefServiceSimple& prefs) {
  EXPECT_GE(GetPersistedLogCount(prefs), 1);
  metrics::UnsentLogStore result_unsent_log_store(
      std::make_unique<UnsentLogStoreMetricsImpl>(), &prefs,
      prefs::kUkmUnsentLogStore, /*metadata_pref_name=*/nullptr,
      // Set to 3 so logs are not dropped in the test.
      metrics::UnsentLogStore::UnsentLogStoreLimits{
          .min_log_count = 3,
      },
      /*signing_key=*/std::string(),
      /*logs_event_manager=*/nullptr);

  result_unsent_log_store.LoadPersistedUnsentLogs();
  result_unsent_log_store.StageNextLog();

  Report report;
  EXPECT_TRUE(metrics::DecodeLogDataToProto(
      result_unsent_log_store.staged_log(), &report));
  return report;
}

metrics::LogMetadata GetPersistedLogMetadata(TestingPrefServiceSimple& prefs) {
  EXPECT_GE(GetPersistedLogCount(prefs), 1);
  metrics::UnsentLogStore result_unsent_log_store(
      std::make_unique<UnsentLogStoreMetricsImpl>(), &prefs,
      prefs::kUkmUnsentLogStore, /*metadata_pref_name=*/nullptr,
      // Set to 3 so logs are not dropped in the test.
      metrics::UnsentLogStore::UnsentLogStoreLimits{
          .min_log_count = 3,
      },
      /*signing_key=*/std::string(),
      /*logs_event_manager=*/nullptr);

  result_unsent_log_store.LoadPersistedUnsentLogs();
  result_unsent_log_store.StageNextLog();

  return result_unsent_log_store.staged_log_metadata();
}

void AddSourceToReport(Report& report,
                       int64_t other_id,
                       SourceIdType id_type,
                       std::string url) {
  Source* proto_source = report.add_sources();
  SourceId source_id = ConvertToSourceId(other_id, id_type);
  proto_source->set_id(source_id);
  proto_source->add_urls()->set_url(url);
  // Add entry for the source.
  Entry* entry = report.add_entries();
  entry->set_source_id(source_id);
}

bool WebDXFeaturesStrictlyContains(const HighLevelWebFeatures& actual_features,
                                   const std::set<int32_t>& expected_features) {
  BitSet bitset(kWebDXFeatureNumberOfFeaturesForTesting,
                actual_features.bit_vector());
  for (size_t i = 0; i < kWebDXFeatureNumberOfFeaturesForTesting; ++i) {
    if (bitset.Contains(i) != base::Contains(expected_features, i)) {
      return false;
    }
  }
  return true;
}

class ScopedUkmFeatureParams {
 public:
  explicit ScopedUkmFeatureParams(const base::FieldTrialParams& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(kUkmFeature,
                                                            params);
  }

  ScopedUkmFeatureParams(const ScopedUkmFeatureParams&) = delete;
  ScopedUkmFeatureParams& operator=(const ScopedUkmFeatureParams&) = delete;

  ~ScopedUkmFeatureParams() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class MockDemographicMetricsProvider
    : public metrics::UkmDemographicMetricsProvider {
 public:
  ~MockDemographicMetricsProvider() override = default;

  // DemographicMetricsProvider:
  MOCK_METHOD1(ProvideSyncedUserNoisedBirthYearAndGenderToReport,
               void(Report* report));
};

class MockUkmRecorderObserver : public UkmRecorder::Observer {
 public:
  MOCK_METHOD0(OnStartingShutdown, void());
};

// A simple Provider that emits a 'TestProviderEvent' on session close (i.e. a
// Report being emitted).
class UkmTestMetricsProvider : public metrics::TestMetricsProvider {
 public:
  explicit UkmTestMetricsProvider(UkmRecorder* test_recording_helper)
      : test_recording_helper_(test_recording_helper) {}

  void ProvideCurrentSessionUKMData() override {
    // An Event emitted during a Provider will frequently not not associated
    // with a URL.
    SourceId id = ukm::NoURLSourceId();
    TestProviderEvent(id).Record(test_recording_helper_);
  }

 private:
  raw_ptr<UkmRecorder> test_recording_helper_;
};

class UkmServiceTest : public testing::Test {
 public:
  UkmServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());
    ClearPrefs();
  }

  UkmServiceTest(const UkmServiceTest&) = delete;
  UkmServiceTest& operator=(const UkmServiceTest&) = delete;

  void ClearPrefs() {
    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmUnsentLogStore);
  }

  int GetPersistedLogCount() { return ukm::GetPersistedLogCount(prefs_); }

  Report GetPersistedReport() { return ukm::GetPersistedReport(prefs_); }

  metrics::LogMetadata GetPersistedLogMetadata() {
    return ukm::GetPersistedLogMetadata(prefs_);
  }

  static SourceId GetAllowlistedSourceId(int64_t id) {
    return ConvertToSourceId(id, SourceIdType::NAVIGATION_ID);
  }

  static SourceId GetAppIDSourceId(int64_t id) {
    return ConvertToSourceId(id, SourceIdType::APP_ID);
  }

  static SourceId GetNonAllowlistedSourceId(int64_t id) {
    return ConvertToSourceId(id, SourceIdType::DEFAULT);
  }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

class UkmReduceAddEntryIpcTest : public testing::Test {
 public:
  UkmReduceAddEntryIpcTest() {
    UkmService::RegisterPrefs(prefs_.registry());
    ClearPrefs();
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmReduceAddEntryIPC);
  }

  UkmReduceAddEntryIpcTest(const UkmReduceAddEntryIpcTest&) = delete;
  UkmReduceAddEntryIpcTest& operator=(const UkmReduceAddEntryIpcTest&) = delete;

  void ClearPrefs() {
    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmUnsentLogStore);
  }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::TaskEnvironment task_environment;
};
}  // namespace

TEST_F(UkmServiceTest, ClientIdMigration) {
  prefs_.SetInt64(prefs::kUkmClientId, -1);
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  service.Initialize();
  uint64_t migrated_id = prefs_.GetUint64(prefs::kUkmClientId);
  // -1 migrates to the max UInt 64 value.
  EXPECT_EQ(migrated_id, 18446744073709551615ULL);
}

TEST_F(UkmServiceTest, ClientIdClonedInstall) {
  prefs_.SetInt64(prefs::kUkmClientId, 123);
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());

  EXPECT_FALSE(client_.ShouldResetClientIdsOnClonedInstall());
  client_.set_should_reset_client_ids_on_cloned_install(true);
  EXPECT_TRUE(client_.ShouldResetClientIdsOnClonedInstall());

  uint64_t original_id = prefs_.GetUint64(prefs::kUkmClientId);
  service.Initialize();
  uint64_t new_id = prefs_.GetUint64(prefs::kUkmClientId);
  EXPECT_NE(original_id, new_id);
}

TEST_F(UkmServiceTest, EnableDisableSchedule) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  service.Initialize();
  EXPECT_FALSE(task_runner_->HasPendingTask());
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();
  EXPECT_TRUE(task_runner_->HasPendingTask());
  service.DisableReporting();
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(UkmServiceTest, PersistAndPurge) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Should init, generate a log, and start an upload for source.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Flushes the generated log to disk and generates a new entry.
  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 2);
  service.Purge();
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

TEST_F(UkmServiceTest, Purge) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Record some data
  auto id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));
  TestEvent1(id).Record(&service);

  // Purge should delete data, so there shouldn't be anything left to upload.
  service.Purge();
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(0, GetPersistedLogCount());
}

TEST_F(UkmServiceTest, PurgeExtensionDataFromUnsentLogStore) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);
  report.mutable_system_profile()->set_app_version(client_.GetVersionString());

  std::string non_extension_url = "https://www.google.ca";
  std::string extension_url =
      "chrome-extension://bmnlcjabgnpnenekpadlanbbkooimhnj/manifest.json";

  // Add both extension- and non-extension-related sources to the Report.
  Source* proto_source_1 = report.add_sources();
  SourceId source_id_1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  proto_source_1->set_id(source_id_1);
  proto_source_1->add_urls()->set_url(non_extension_url);
  Source* proto_source_2 = report.add_sources();
  SourceId source_id_2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  proto_source_2->set_id(source_id_2);
  proto_source_2->add_urls()->set_url(extension_url);

  // Add entries related to the sources.
  Entry* entry_1 = report.add_entries();
  entry_1->set_source_id(source_id_2);
  Entry* entry_2 = report.add_entries();
  entry_2->set_source_id(source_id_1);
  Entry* entry_3 = report.add_entries();
  entry_3->set_source_id(source_id_1);

  // Add web features related to the sources.
  HighLevelWebFeatures* features_1 = report.add_web_features();
  features_1->set_source_id(source_id_1);
  HighLevelWebFeatures* features_2 = report.add_web_features();
  features_2->set_source_id(source_id_2);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  // Makes sure that the serialized ukm report can be parsed.
  ASSERT_TRUE(UkmService::LogCanBeParsed(serialized_log));
  metrics::LogMetadata log_metadata;
  unsent_log_store->StoreLog(
      serialized_log, log_metadata,
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Do extension purging.
  service.PurgeExtensionsData();

  // Get the Report in the log store and verify extension-related data have been
  // filtered.
  unsent_log_store->StageNextLog();
  const std::string& compressed_log_data = unsent_log_store->staged_log();

  Report filtered_report;
  ASSERT_TRUE(
      metrics::DecodeLogDataToProto(compressed_log_data, &filtered_report));

  // The logs app version and the client_ version string should be the same,
  // so the log_written_by_app_version shouldn't be set.
  ASSERT_FALSE(
      filtered_report.system_profile().has_log_written_by_app_version());
  // Only proto_source_1 with non-extension URL is kept.
  EXPECT_EQ(1, filtered_report.sources_size());
  EXPECT_EQ(source_id_1, filtered_report.sources(0).id());
  EXPECT_EQ(non_extension_url, filtered_report.sources(0).urls(0).url());

  // Only entry_2 and entry_3 from the non-extension source is kept.
  EXPECT_EQ(2, filtered_report.entries_size());
  EXPECT_EQ(source_id_1, filtered_report.entries(0).source_id());
  EXPECT_EQ(source_id_1, filtered_report.entries(1).source_id());

  // Only features_1 from the non-extension source is kept (features_2 was
  // filtered out due to being associated with an extension source).
  EXPECT_EQ(1, filtered_report.web_features_size());
  EXPECT_EQ(source_id_1, filtered_report.web_features(0).source_id());
}

TEST_F(UkmServiceTest, PurgeExtensionDataFromUnsentLogStoreWithVersionChange) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);
  report.mutable_system_profile()->set_app_version("0.0.0.0");

  std::string non_extension_url = "https://www.google.ca";
  std::string extension_url =
      "chrome-extension://bmnlcjabgnpnenekpadlanbbkooimhnj/manifest.json";

  // Add both extension- and non-extension-related sources to the Report.
  Source* proto_source_1 = report.add_sources();
  SourceId source_id_1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  proto_source_1->set_id(source_id_1);
  proto_source_1->add_urls()->set_url(non_extension_url);
  Source* proto_source_2 = report.add_sources();
  SourceId source_id_2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  proto_source_2->set_id(source_id_2);
  proto_source_2->add_urls()->set_url(extension_url);

  // Add entries related to the sources.
  Entry* entry_1 = report.add_entries();
  entry_1->set_source_id(source_id_2);
  Entry* entry_2 = report.add_entries();
  entry_2->set_source_id(source_id_1);
  Entry* entry_3 = report.add_entries();
  entry_3->set_source_id(source_id_1);

  // Add web features related to the sources.
  HighLevelWebFeatures* features_1 = report.add_web_features();
  features_1->set_source_id(source_id_1);
  HighLevelWebFeatures* features_2 = report.add_web_features();
  features_2->set_source_id(source_id_2);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  // Makes sure that the serialized ukm report can be parsed.
  ASSERT_TRUE(UkmService::LogCanBeParsed(serialized_log));
  metrics::LogMetadata log_metadata;
  unsent_log_store->StoreLog(
      serialized_log, log_metadata,
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Do extension purging.
  service.PurgeExtensionsData();

  // Get the Report in the log store and verify extension-related data have been
  // filtered.
  unsent_log_store->StageNextLog();
  const std::string& compressed_log_data = unsent_log_store->staged_log();

  Report filtered_report;
  ASSERT_TRUE(
      metrics::DecodeLogDataToProto(compressed_log_data, &filtered_report));

  // The logs app version and the client_ version string should be different,
  // so the log_written_by_app_version should be set to the current client_
  // version.
  ASSERT_EQ(filtered_report.system_profile().log_written_by_app_version(),
            client_.GetVersionString());
  // Only proto_source_1 with non-extension URL is kept.
  EXPECT_EQ(1, filtered_report.sources_size());
  EXPECT_EQ(source_id_1, filtered_report.sources(0).id());
  EXPECT_EQ(non_extension_url, filtered_report.sources(0).urls(0).url());

  // Only entry_2 and entry_3 from the non-extension source is kept.
  EXPECT_EQ(2, filtered_report.entries_size());
  EXPECT_EQ(source_id_1, filtered_report.entries(0).source_id());
  EXPECT_EQ(source_id_1, filtered_report.entries(1).source_id());

  // Only features_1 from the non-extension source is kept (features_2 was
  // filtered out due to being associated with an extension source).
  EXPECT_EQ(1, filtered_report.web_features_size());
  EXPECT_EQ(source_id_1, filtered_report.web_features(0).source_id());
}

TEST_F(UkmServiceTest, PurgeAppDataFromUnsentLogStore) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);

  // A URL from browser navigation.
  std::string non_app_url = "https://www.google.ca";
  // A URL with app:// scheme.
  std::string app_url = "app://mgndgikekgjfcpckkfioiadnlibdjbkf";
  // OS Settings is an app on ChromeOS without the app:// scheme.
  std::string os_settings_url = "chrome://os-settings";

  // Add sources to the Report.
  // Non-app source.
  Source* proto_source_1 = report.add_sources();
  SourceId source_id_1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  proto_source_1->set_id(source_id_1);
  proto_source_1->add_urls()->set_url(non_app_url);
  // App source with app:// URL.
  Source* proto_source_2 = report.add_sources();
  SourceId source_id_2 = ConvertToSourceId(2, SourceIdType::APP_ID);
  proto_source_2->set_id(source_id_2);
  proto_source_2->add_urls()->set_url(app_url);
  // App source with non-app:// URL.
  Source* proto_source_3 = report.add_sources();
  SourceId source_id_3 = ConvertToSourceId(3, SourceIdType::APP_ID);
  proto_source_3->set_id(source_id_3);
  proto_source_3->add_urls()->set_url(os_settings_url);
  // Non-app source with app:// URL. This shouldn't happen in practice, but
  // if it does, this source should be purged when app data are purged.
  Source* proto_source_4 = report.add_sources();
  SourceId source_id_4 = ConvertToSourceId(4, SourceIdType::NAVIGATION_ID);
  proto_source_4->set_id(source_id_4);
  proto_source_4->add_urls()->set_url(app_url);

  // Add entries to each of the sources.
  Entry* entry_1 = report.add_entries();
  entry_1->set_source_id(source_id_2);
  Entry* entry_2 = report.add_entries();
  entry_2->set_source_id(source_id_1);
  Entry* entry_3 = report.add_entries();
  entry_3->set_source_id(source_id_2);
  Entry* entry_4 = report.add_entries();
  entry_4->set_source_id(source_id_3);
  Entry* entry_5 = report.add_entries();
  entry_5->set_source_id(source_id_4);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  // Make sure that the serialized ukm report can be parsed.
  ASSERT_TRUE(UkmService::LogCanBeParsed(serialized_log));
  metrics::LogMetadata log_metadata;
  unsent_log_store->StoreLog(
      serialized_log, log_metadata,
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Do app data purging.
  service.PurgeAppsData();

  // Get the Report in the log store and verify app-related data have been
  // filtered.
  unsent_log_store->StageNextLog();
  const std::string& compressed_log_data = unsent_log_store->staged_log();

  Report filtered_report;
  ASSERT_TRUE(
      metrics::DecodeLogDataToProto(compressed_log_data, &filtered_report));

  // Only proto_source_1 with non-app URL is kept.
  EXPECT_EQ(1, filtered_report.sources_size());
  EXPECT_EQ(source_id_1, filtered_report.sources(0).id());
  EXPECT_EQ(non_app_url, filtered_report.sources(0).urls(0).url());

  // Only entry_2 from the non-app source is kept.
  EXPECT_EQ(1, filtered_report.entries_size());
  EXPECT_EQ(source_id_1, filtered_report.entries(0).source_id());
}

TEST_F(UkmServiceTest, PurgeMsbbDataFromUnsentLogStore) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);

  // A URL from browser navigation.
  std::string non_app_url = "https://www.google.ca";
  std::string non_app_url2 = "https://www.google.com";
  // A URL with app:// scheme.
  std::string app_url = "app://mgndgikekgjfcpckkfioiadnlibdjbkf";
  // OS Settings is an app on ChromeOS without the app:// scheme.
  std::string os_settings_url = "chrome://os-settings";

  // Add sources to the Report.
  // Non-app source.
  Source* proto_source_1 = report.add_sources();
  SourceId source_id_1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  proto_source_1->set_id(source_id_1);
  proto_source_1->add_urls()->set_url(non_app_url);
  // Non-app source 2.
  Source* proto_source_2 = report.add_sources();
  SourceId source_id_2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);
  proto_source_2->set_id(source_id_2);
  proto_source_2->add_urls()->set_url(non_app_url);
  // App source with app:// URL.
  Source* proto_source_3 = report.add_sources();
  SourceId source_id_3 = ConvertToSourceId(3, SourceIdType::APP_ID);
  proto_source_3->set_id(source_id_3);
  proto_source_3->add_urls()->set_url(app_url);
  // App source with non-app:// URL.
  Source* proto_source_4 = report.add_sources();
  SourceId source_id_4 = ConvertToSourceId(4, SourceIdType::APP_ID);
  proto_source_4->set_id(source_id_4);
  proto_source_4->add_urls()->set_url(os_settings_url);
  // Non-app source with app:// URL. This shouldn't happen in practice, but
  // if it does, this source should be purged when app data are purged.
  Source* proto_source_5 = report.add_sources();
  SourceId source_id_5 = ConvertToSourceId(5, SourceIdType::NAVIGATION_ID);
  proto_source_5->set_id(source_id_5);
  proto_source_5->add_urls()->set_url(app_url);

  // Add entries to each of the sources.
  Entry* entry_1 = report.add_entries();
  entry_1->set_source_id(source_id_2);
  Entry* entry_2 = report.add_entries();
  entry_2->set_source_id(source_id_1);
  Entry* entry_3 = report.add_entries();
  entry_3->set_source_id(source_id_2);
  Entry* entry_4 = report.add_entries();
  entry_4->set_source_id(source_id_3);
  Entry* entry_5 = report.add_entries();
  entry_5->set_source_id(source_id_4);
  Entry* entry_6 = report.add_entries();
  entry_6->set_source_id(source_id_5);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  // Make sure that the serialized ukm report can be parsed.
  ASSERT_TRUE(UkmService::LogCanBeParsed(serialized_log));
  metrics::LogMetadata log_metadata;
  unsent_log_store->StoreLog(
      serialized_log, log_metadata,
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Purge MSBB data.
  service.PurgeMsbbData();

  // Get the Report in the log store and verify non-app-related data have been
  // filtered.
  unsent_log_store->StageNextLog();
  const std::string& compressed_log_data = unsent_log_store->staged_log();

  Report filtered_report;
  ASSERT_TRUE(
      metrics::DecodeLogDataToProto(compressed_log_data, &filtered_report));

  // Source proto_source_1 with app URL is kept.
  EXPECT_EQ(2, filtered_report.sources_size());
  EXPECT_EQ(source_id_4, filtered_report.sources(0).id());
  EXPECT_EQ(os_settings_url, filtered_report.sources(0).urls(0).url());

  EXPECT_EQ(source_id_3, filtered_report.sources(1).id());
  EXPECT_EQ(app_url, filtered_report.sources(1).urls(0).url());

  // Entry entry_2 from the app source is kept.
  EXPECT_EQ(2, filtered_report.entries_size());
  EXPECT_EQ(source_id_4, filtered_report.entries(0).source_id());
  EXPECT_EQ(source_id_3, filtered_report.entries(1).source_id());
}

TEST_F(UkmServiceTest, PurgeAppDataLogMetadataUpdate) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  auto* unsent_log_store = service.reporting_service_.ukm_log_store();

  // Initialize a Report to be saved to the log store.
  Report report;
  report.set_client_id(1);
  report.set_session_id(1);
  report.set_report_id(1);

  // A URL from browser navigation.
  std::string non_app_url = "https://www.google.ca";
  // A URL with app:// scheme.
  std::string app_url = "app://mgndgikekgjfcpckkfioiadnlibdjbkf";
  // OS Settings is an app on ChromeOS without the app:// scheme.
  std::string os_settings_url = "chrome://os-settings";

  // Add sources to the Report.
  AddSourceToReport(report, 1, SourceIdType::NAVIGATION_ID, non_app_url);
  AddSourceToReport(report, 2, SourceIdType::APP_ID, app_url);
  AddSourceToReport(report, 3, SourceIdType::APP_ID, os_settings_url);
  AddSourceToReport(report, 4, SourceIdType::NAVIGATION_ID, app_url);

  // Save the Report to the store.
  std::string serialized_log;
  report.SerializeToString(&serialized_log);

  // Make sure that the serialized ukm report can be parsed.
  ASSERT_TRUE(UkmService::LogCanBeParsed(serialized_log));

  metrics::LogMetadata log_metadata;
  log_metadata.log_source_type = metrics::UkmLogSourceType::BOTH_UKM_AND_APPKM;
  unsent_log_store->StoreLog(
      serialized_log, log_metadata,
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Do app data purging.
  service.PurgeAppsData();

  // Get the Report in the log store and verify log metadata is updated.
  unsent_log_store->StageNextLog();
  const metrics::LogMetadata updated_log_metadata =
      unsent_log_store->staged_log_metadata();
  EXPECT_EQ(updated_log_metadata.log_source_type,
            metrics::UkmLogSourceType::UKM_ONLY);
}

TEST_F(UkmServiceTest, SourceSerialization) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  UkmSource::NavigationData navigation_data;
  navigation_data.urls = {GURL("https://google.com/initial"),
                          GURL("https://google.com/final")};

  SourceId id = GetAllowlistedSourceId(0);
  recorder.RecordNavigation(id, navigation_data);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  EXPECT_TRUE(proto_report.has_session_id());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(id, proto_source.id());
  EXPECT_EQ(GURL("https://google.com/final").spec(),
            proto_source.urls(1).url());
}

TEST_F(UkmServiceTest, SourceSerializationForAllowlistedButNonNavigationType) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  const GURL kURL("https://example.com/");

  SourceId id = ConvertToSourceId(0, SourceIdType::NOTIFICATION_ID);
  recorder.UpdateSourceURL(id, kURL);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.sources_size());
  EXPECT_TRUE(proto_report.has_session_id());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(id, proto_source.id());
  EXPECT_EQ(static_cast<int>(SourceIdType::NOTIFICATION_ID), proto_source.type());
  ASSERT_EQ(1, proto_source.urls_size());
  EXPECT_EQ(kURL.spec(), proto_source.urls(0).url());
}

TEST_F(UkmServiceTest, LogMetadataOnlyAppKMSourceType) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::APPS});
  service.EnableReporting();
  const GURL kAppURL("app://google.com/foobar");

  SourceId id = GetAppIDSourceId(0);
  recorder.UpdateSourceURL(id, kAppURL);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  metrics::LogMetadata log_metadata = GetPersistedLogMetadata();
  EXPECT_TRUE(log_metadata.log_source_type.has_value());
  EXPECT_TRUE(log_metadata.log_source_type.value() ==
              metrics::UkmLogSourceType::APPKM_ONLY);
}

TEST_F(UkmServiceTest, LogMetadataOnlyUKMSourceType) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB, UkmConsentType::APPS});
  service.EnableReporting();
  const GURL kURL("https://google.com/foobar");

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, kURL);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  metrics::LogMetadata log_metadata = GetPersistedLogMetadata();
  EXPECT_TRUE(log_metadata.log_source_type.has_value());
  EXPECT_TRUE(log_metadata.log_source_type.value() ==
              metrics::UkmLogSourceType::UKM_ONLY);
}

TEST_F(UkmServiceTest, LogMetadataBothSourceType) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB, UkmConsentType::APPS});
  service.EnableReporting();

  const GURL kAppURL("app://google.com/foobar");

  SourceId app_id = GetAppIDSourceId(0);
  recorder.UpdateSourceURL(app_id, kAppURL);

  const GURL kURL("https://google.com/foobar");

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, kURL);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  metrics::LogMetadata log_metadata = GetPersistedLogMetadata();
  EXPECT_TRUE(log_metadata.log_source_type.has_value() &&
              log_metadata.log_source_type.value() ==
                  metrics::UkmLogSourceType::BOTH_UKM_AND_APPKM);
}

TEST_F(UkmServiceTest, AddEntryWithEmptyMetrics) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  ASSERT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.entries_size());
}

TEST_F(UkmServiceTest, MetricsProviderTest) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);

  auto* provider = new UkmTestMetricsProvider(&service);
  service.RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(provider));

  service.Initialize();

  // Providers have not supplied system profile information yet.
  EXPECT_FALSE(provider->provide_system_profile_metrics_called());

  task_runner_->RunUntilIdle();
  service.UpdateRecording({MSBB});
  service.EnableReporting();

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  // We should have an Event from a Provider provided metric, however, it is not
  // attached to a Source (which should be typical for a Provider metric).
  EXPECT_EQ(proto_report.sources_size(), 0);

  // Providers have now supplied system profile.
  EXPECT_TRUE(provider->provide_system_profile_metrics_called());
  // Providers has also supplied a UKM Event.
  const Entry& entry = proto_report.entries(0);
  EXPECT_EQ(base::HashMetricName(TestProviderEvent::kEntryName),
            entry.event_hash());
}

// Currently just testing brand is set, would be good to test other core
// system profile fields.
TEST_F(UkmServiceTest, SystemProfileTest) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);

  service.Initialize();

  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(metrics::TestMetricsServiceClient::kBrandForTesting,
            proto_report.system_profile().brand_code());
}

TEST_F(UkmServiceTest, AddUserDemograhicsWhenAvailableAndFeatureEnabled) {
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
                       test_birth_year](Report* report) {
        report->mutable_user_demographics()->set_birth_year(test_birth_year);
        report->mutable_user_demographics()->set_gender(test_gender);
        ++number_of_invocations;
      });

  UkmService service(&prefs_, &client_, std::move(provider));
  TestRecordingHelper recorder(&service);

  service.Initialize();

  // Verify that the provider hasn't provided the synced user's noised birth
  // year and gender yet.
  EXPECT_EQ(0, number_of_invocations);

  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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
  auto provider = std::make_unique<MockDemographicMetricsProvider>();
  EXPECT_CALL(*provider,
              ProvideSyncedUserNoisedBirthYearAndGenderToReport(testing::_))
      .Times(2)
      .WillRepeatedly([](Report* report) {});

  UkmService service(&prefs_, &client_, std::move(provider));
  TestRecordingHelper recorder(&service);
  service.Initialize();

  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  // Verify that the synced user's noised birth year and gender are not in the
  // report when they are not available.
  Report proto_report = GetPersistedReport();
  EXPECT_FALSE(proto_report.has_user_demographics());
}

TEST_F(UkmServiceTest, DontAddUserDemograhicsWhenFeatureDisabled) {
  base::test::ScopedFeatureList local_feature;
  local_feature.InitAndDisableFeature(kReportUserNoisedUserBirthYearAndGender);

  // The demographics provider should not be called.
  auto provider = std::make_unique<MockDemographicMetricsProvider>();
  EXPECT_CALL(*provider,
              ProvideSyncedUserNoisedBirthYearAndGenderToReport(testing::_))
      .Times(0);

  UkmService service(&prefs_, &client_, std::move(provider));
  TestRecordingHelper recorder(&service);

  service.Initialize();

  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  // Verify that the synced user's noised birth year and gender are not in the
  // report when they are not available.
  Report proto_report = GetPersistedReport();
  EXPECT_FALSE(proto_report.has_user_demographics());
}

TEST_F(UkmServiceTest, LogsRotation) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  EXPECT_EQ(0, service.report_count());

  // Log rotation should generate a log.
  const SourceId id = GetAllowlistedSourceId(0);
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

TEST_F(UkmServiceTest, LogsUploadedOnlyWhenHavingData) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  EXPECT_TRUE(task_runner_->HasPendingTask());
  // Neither rotation or Flush should generate logs
  task_runner_->RunPendingTasks();
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 0);

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Includes a Source, so will persist.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 1);

  TestEvent1(id).Record(&service);
  // Includes an Entry, so will persist.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 2);

  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  TestEvent1(id).Record(&service);
  // Do not keep the source in the recorder after the current log.
  recorder.MarkSourceForDeletion(id);
  // Includes a Source and an Entry, so will persist.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 3);

  // The recorder contains no data (Sources, Entries, nor web features), thus
  // will not create a new log.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 3);

  recorder.RecordWebDXFeatures(id, {kWebDXFeature1},
                               kWebDXFeatureNumberOfFeaturesForTesting);
  // Includes web features data, so will persist.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 4);

  // The recorder contains no data (Sources, Entries, nor web features), thus
  // will not create a new log.
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 4);
}

TEST_F(UkmServiceTest, GetNewSourceID) {
  SourceId id1 = UkmRecorder::GetNewSourceID();
  SourceId id2 = UkmRecorder::GetNewSourceID();
  SourceId id3 = UkmRecorder::GetNewSourceID();
  EXPECT_NE(id1, id2);
  EXPECT_NE(id1, id3);
  EXPECT_NE(id2, id3);
}

TEST_F(UkmServiceTest, RecordRedirectedUrl) {
  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  UkmSource::NavigationData navigation_data;
  navigation_data.urls = {GURL("https://google.com/initial"),
                          GURL("https://google.com/final")};
  recorder.RecordNavigation(id, navigation_data);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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

TEST_F(UkmServiceTest, RecordSessionId) {
  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  auto id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  EXPECT_TRUE(proto_report.has_session_id());
  EXPECT_EQ(1, proto_report.report_id());
}

TEST_F(UkmServiceTest, SourceSize) {
  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Add a large number of sources, more than the hardcoded max.
  for (int i = 0; i < 1000; ++i) {
    auto id = GetAllowlistedSourceId(i);
    recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  }

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  // Note, 500 instead of 1000 sources, since 500 is the maximum.
  EXPECT_EQ(500, proto_report.sources_size());
}

TEST_F(UkmServiceTest, PurgeMidUpload) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  auto id = GetAllowlistedSourceId(0);
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

TEST_F(UkmServiceTest, SourceURLLength) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  auto id = GetAllowlistedSourceId(0);

  // This URL is too long to be recorded fully.
  const std::string long_string =
      "https://example.com/" + std::string(10000, 'a');
  recorder.UpdateSourceURL(id, GURL(long_string));

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);
  EXPECT_EQ("URLTooLong", proto_source.urls(0).url());
}

TEST_F(UkmServiceTest, UnreferencedNonAllowlistedSources) {
  const GURL kURL("https://google.com/foobar");

  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Record with Allowlisted ID to allowlist the URL.
  // Use a larger ID to make it last in the proto.
  SourceId allowlisted_id = GetAllowlistedSourceId(100);
  recorder.UpdateSourceURL(allowlisted_id, kURL);

  std::vector<SourceId> ids;
  base::TimeTicks last_time = base::TimeTicks::Now();
  for (int i = 1; i < 7; ++i) {
    // Wait until base::TimeTicks::Now() no longer equals |last_time|. This
    // ensures each source has a unique timestamp to avoid flakes. Should take
    // between 1-15ms per documented resolution of base::TimeTicks.
    while (base::TimeTicks::Now() == last_time) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }

    ids.push_back(GetNonAllowlistedSourceId(i));
    recorder.UpdateSourceURL(ids.back(), kURL);
    last_time = base::TimeTicks::Now();
  }

  TestEvent1(ids[0]).Record(&service);
  TestEvent2(ids[2]).Record(&service);
  TestEvent3(ids[2]).Record(&service);
  TestEvent3(ids[3]).Record(&service);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());
  auto proto_report = GetPersistedReport();

  // 1 allowlisted source and 6 non-allowlisted source.
  EXPECT_EQ(7, proto_report.source_counts().observed());
  // The one allowlisted source is of navigation type.
  EXPECT_EQ(1, proto_report.source_counts().navigation_sources());
  EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());

  EXPECT_EQ(4, proto_report.source_counts().deferred_sources());
  EXPECT_EQ(0, proto_report.source_counts().carryover_sources());

  ASSERT_EQ(4, proto_report.sources_size());
  EXPECT_EQ(ids[0], proto_report.sources(0).id());
  EXPECT_EQ(kURL.spec(), proto_report.sources(0).urls(0).url());
  EXPECT_EQ(ids[2], proto_report.sources(1).id());
  EXPECT_EQ(kURL.spec(), proto_report.sources(1).urls(0).url());
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

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(2, GetPersistedLogCount());
  proto_report = GetPersistedReport();

  EXPECT_EQ(0, proto_report.source_counts().observed());
  EXPECT_EQ(0, proto_report.source_counts().navigation_sources());
  EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());

  EXPECT_EQ(2, proto_report.source_counts().deferred_sources());

  EXPECT_EQ(4, proto_report.source_counts().carryover_sources());
  ASSERT_EQ(3, proto_report.sources_size());
}

TEST_F(UkmServiceTest, NonAllowlistedUrls) {
  // URL to be manually allowlisted using allowlisted source type.
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

  for (const auto& test : test_cases) {
    ClearPrefs();
    UkmService service(&prefs_, &client_,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);

    ASSERT_EQ(GetPersistedLogCount(), 0);
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.UpdateRecording({UkmConsentType::MSBB});
    service.EnableReporting();

    // Record with allowlisted ID to allowlist the URL.
    SourceId allowlist_id = GetAllowlistedSourceId(1);
    recorder.UpdateSourceURL(allowlist_id, kURL);

    // Record non allowlisted ID with an entry.
    SourceId nonallowlist_id = GetNonAllowlistedSourceId(100);
    recorder.UpdateSourceURL(nonallowlist_id, test.url);
    TestEvent1(nonallowlist_id).Record(&service);

    service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
    ASSERT_EQ(1, GetPersistedLogCount());
    auto proto_report = GetPersistedReport();

    EXPECT_EQ(2, proto_report.source_counts().observed());
    EXPECT_EQ(1, proto_report.source_counts().navigation_sources());

    // If the source id is not allowlisted, don't send it unless it has
    // associated entries and the URL matches that of the allowlisted source.
    if (test.expect_in_report) {
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(2, proto_report.sources_size());
      EXPECT_EQ(allowlist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
      EXPECT_EQ(nonallowlist_id, proto_report.sources(1).id());
      EXPECT_EQ(test.url, proto_report.sources(1).urls(0).url());
    } else {
      EXPECT_EQ(1, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(1, proto_report.sources_size());
      EXPECT_EQ(allowlist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
    }

    // Do a log rotation again, with the same test URL associated to a new
    // source id. Since the previous source id of the test case is of
    // non-allowlisted type, the carryover URLs list is expected to remain
    // be unchanged, thus the the report should still contain the same numbers
    // of sources as before, that is, non-allowlisted URLs should not have
    // allowlisted themselves during the previous log rotation.
    SourceId nonallowlist_id2 = GetNonAllowlistedSourceId(101);
    recorder.UpdateSourceURL(nonallowlist_id2, test.url);
    TestEvent1(nonallowlist_id2).Record(&service);
    service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
    ASSERT_EQ(2, GetPersistedLogCount());
    proto_report = GetPersistedReport();

    if (test.expect_in_report) {
      EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(2, proto_report.sources_size());
      EXPECT_EQ(allowlist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
      EXPECT_EQ(nonallowlist_id2, proto_report.sources(1).id());
      EXPECT_EQ(test.url, proto_report.sources(1).urls(0).url());
    } else {
      EXPECT_EQ(1, proto_report.source_counts().unmatched_sources());
      ASSERT_EQ(1, proto_report.sources_size());
      EXPECT_EQ(allowlist_id, proto_report.sources(0).id());
      EXPECT_EQ(kURL, proto_report.sources(0).urls(0).url());
    }
  }
}

TEST_F(UkmServiceTest, AllowlistIdType) {
  std::map<SourceIdType, bool> source_id_type_allowlisted = {
      {SourceIdType::DEFAULT, false},  {SourceIdType::NAVIGATION_ID, true},
      {SourceIdType::APP_ID, true},    {SourceIdType::HISTORY_ID, true},
      {SourceIdType::WEBAPK_ID, true},
  };

  for (std::pair<SourceIdType, bool> type : source_id_type_allowlisted) {
    ClearPrefs();
    UkmService service(&prefs_, &client_,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);
    EXPECT_EQ(0, GetPersistedLogCount());
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.UpdateRecording({UkmConsentType::MSBB, UkmConsentType::APPS});
    service.EnableReporting();

    SourceId id = ConvertSourceIdToAllowlistedType(
        1, static_cast<SourceIdType>(type.first));
    ASSERT_EQ(GetSourceIdType(id), type.first);

    recorder.UpdateSourceURL(id, GURL("https://google.com/foobar1"));

    TestEvent1(id).Record(&service);

    service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
    EXPECT_EQ(1, GetPersistedLogCount());
    Report proto_report = GetPersistedReport();

    if (type.second) {
      // Verify we've added one source.
      EXPECT_EQ(1, proto_report.sources_size());
      EXPECT_EQ(GURL("https://google.com/foobar1").spec(),
                proto_report.sources(0).urls(0).url());
    } else {
      // No source added when id is not allowlisted type.
      EXPECT_EQ(0, proto_report.sources_size());
    }

    // We've added the entry whether source is added or not.
    ASSERT_EQ(1, proto_report.entries_size());
    const Entry& proto_entry_a = proto_report.entries(0);
    EXPECT_EQ(id, proto_entry_a.source_id());
    EXPECT_EQ(base::HashMetricName(TestEvent1::kEntryName),
              proto_entry_a.event_hash());
  }
}

TEST_F(UkmServiceTest, SupportedSchemes) {
  struct {
    const char* url;
    bool expected_kept;
  } test_cases[] = {
      {"http://google.ca/", true},
      {"https://google.ca/", true},
      {"about:blank", true},
      {"chrome://version/", true},
      {"app://play/abcdefghijklmnopqrstuvwxyzabcdef/", true},
      // chrome-extension are controlled by TestIsWebstoreExtension, above.
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/", true},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/", false},
      {"ftp://google.ca/", false},
      {"file:///tmp/", false},
      {"abc://google.ca/", false},
      {"www.google.ca/", false},
  };

  ScopedUkmFeatureParams params({});
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  service.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB, UkmConsentType::EXTENSIONS});
  service.EnableReporting();

  int64_t id_counter = 1;
  int expected_kept_count = 0;
  for (const auto& test : test_cases) {
    auto source_id = GetAllowlistedSourceId(id_counter++);
    recorder.UpdateSourceURL(source_id, GURL(test.url));
    TestEvent1(source_id).Record(&service);
    if (test.expected_kept) {
      ++expected_kept_count;
    }
  }

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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
      {"about:blank", true},
      {"chrome://version/", true},
      {"app://play/abcdefghijklmnopqrstuvwxyzabcdef/", true},
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/", false},
      {"chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/", false},
      {"ftp://google.ca/", false},
      {"file:///tmp/", false},
      {"abc://google.ca/", false},
      {"www.google.ca/", false},
  };

  ScopedUkmFeatureParams params({});
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);

  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  int64_t id_counter = 1;
  int expected_kept_count = 0;
  for (const auto& test : test_cases) {
    auto source_id = GetAllowlistedSourceId(id_counter++);
    recorder.UpdateSourceURL(source_id, GURL(test.url));
    TestEvent1(source_id).Record(&service);
    if (test.expected_kept) {
      ++expected_kept_count;
    }
  }

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  auto id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://username:password@example.com/"));

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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
      {"chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/foo.html?a=b",
       "chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj/"},
  };

  for (const auto& test : test_cases) {
    ClearPrefs();

    UkmService service(&prefs_, &client_,
                       std::make_unique<MockDemographicMetricsProvider>());
    TestRecordingHelper recorder(&service);
    service.SetIsWebstoreExtensionCallback(
        base::BindRepeating(&TestIsWebstoreExtension));

    EXPECT_EQ(0, GetPersistedLogCount());
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.UpdateRecording({UkmConsentType::MSBB, UkmConsentType::EXTENSIONS});
    service.EnableReporting();

    auto id = GetAllowlistedSourceId(0);
    recorder.UpdateSourceURL(id, GURL(test.url));

    service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
    EXPECT_EQ(1, GetPersistedLogCount());

    auto proto_report = GetPersistedReport();
    ASSERT_EQ(1, proto_report.sources_size());
    const Source& proto_source = proto_report.sources(0);
    EXPECT_EQ(test.expected_url, proto_source.urls(0).url());
  }
}

TEST_F(UkmServiceTest, MarkSourceForDeletion) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Seed some dummy sources.
  SourceId id0 = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id0, GURL("https://www.example0.com/"));
  SourceId id1 = GetAllowlistedSourceId(1);
  recorder.UpdateSourceURL(id1, GURL("https://www.example1.com/"));
  SourceId id2 = GetAllowlistedSourceId(2);
  recorder.UpdateSourceURL(id2, GURL("https://www.example2.com/"));

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
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
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  proto_report = GetPersistedReport();
  ASSERT_EQ(3, proto_report.sources_size());

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  proto_report = GetPersistedReport();
  ASSERT_EQ(2, proto_report.sources_size());
  EXPECT_EQ(id0, proto_report.sources(0).id());
  EXPECT_EQ(id2, proto_report.sources(1).id());
}

// Verifies that sources of some types are deleted at the end of reporting
// cycle.
TEST_F(UkmServiceTest, PurgeNonCarriedOverSources) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording(
      {UkmConsentType::MSBB, UkmConsentType::APPS, UkmConsentType::EXTENSIONS});
  service.EnableReporting();
  service.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  // Seed some fake sources.
  SourceId ukm_id = ConvertToSourceId(0, SourceIdType::DEFAULT);
  recorder.UpdateSourceURL(ukm_id, GURL("https://www.example0.com/"));
  SourceId navigation_id =
      ConvertSourceIdToAllowlistedType(1, SourceIdType::NAVIGATION_ID);
  recorder.UpdateSourceURL(navigation_id, GURL("https://www.example1.com/"));
  SourceId app_id = ConvertSourceIdToAllowlistedType(2, SourceIdType::APP_ID);
  recorder.UpdateSourceURL(app_id, GURL("https://www.example2.com/"));
  SourceId history_id =
      ConvertSourceIdToAllowlistedType(3, SourceIdType::HISTORY_ID);
  recorder.UpdateSourceURL(history_id, GURL("https://www.example3.com/"));
  SourceId webapk_id =
      ConvertSourceIdToAllowlistedType(4, SourceIdType::WEBAPK_ID);
  recorder.UpdateSourceURL(webapk_id, GURL("https://www.example4.com/"));
  SourceId payment_app_id =
      ConvertSourceIdToAllowlistedType(5, SourceIdType::PAYMENT_APP_ID);
  recorder.UpdateSourceURL(payment_app_id, GURL("https://www.example5.com/"));
  SourceId web_identity_id =
      ConvertSourceIdToAllowlistedType(6, SourceIdType::WEB_IDENTITY_ID);
  recorder.UpdateSourceURL(web_identity_id, GURL("https://www.example6.com/"));
  SourceId extension_id =
      ConvertSourceIdToAllowlistedType(7, SourceIdType::EXTENSION_ID);
  recorder.UpdateSourceURL(
      extension_id,
      GURL("chrome-extension://bhcnanendmgjjeghamaccjnochlnhcgj"));

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  int logs_count = 0;
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  // All sources are present except ukm_id of non-allowlisted UKM type.
  Report proto_report = GetPersistedReport();
  ASSERT_EQ(7, proto_report.sources_size());
  EXPECT_EQ(navigation_id, proto_report.sources(0).id());
  EXPECT_EQ(app_id, proto_report.sources(1).id());
  EXPECT_EQ(history_id, proto_report.sources(2).id());
  EXPECT_EQ(webapk_id, proto_report.sources(3).id());
  EXPECT_EQ(payment_app_id, proto_report.sources(4).id());
  EXPECT_EQ(web_identity_id, proto_report.sources(5).id());
  EXPECT_EQ(extension_id, proto_report.sources(6).id());

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(++logs_count, GetPersistedLogCount());

  // Sources of HISTORY_ID, WEBAPK_ID, PAYMENT_APP_ID, WEB_IDENTITY_ID,and
  // EXTENSION_ID types are not kept between reporting cycles, thus only 2
  // sources remain.
  proto_report = GetPersistedReport();
  ASSERT_EQ(2, proto_report.sources_size());
  EXPECT_EQ(navigation_id, proto_report.sources(0).id());
  EXPECT_EQ(app_id, proto_report.sources(1).id());
}

TEST_F(UkmServiceTest, IdentifiabilityMetricsDontExplode) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  ASSERT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  builders::Identifiability(id).SetStudyGeneration_626(0).Record(&service);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.entries_size());
}

TEST_F(UkmServiceTest, FilterCanRemoveMetrics) {
  class TestEntryFilter : public UkmEntryFilter {
   public:
    // This implementation removes the last metric in an event and returns it in
    // |filtered_metric_hashes|.
    bool FilterEntry(
        mojom::UkmEntry* entry,
        base::flat_set<uint64_t>* filtered_metric_hashes) override {
      EXPECT_FALSE(entry->metrics.empty());
      auto last_iter = --entry->metrics.end();
      filtered_metric_hashes->insert(last_iter->first);
      entry->metrics.erase(last_iter);
      return !entry->metrics.empty();
    }
  };

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  service.RegisterEventFilter(std::make_unique<TestEntryFilter>());
  TestRecordingHelper recorder(&service);
  ASSERT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  // This event sticks around albeit with a single metric instead of two.
  TestEvent1(id).SetCpuTime(1).SetNet_CacheBytes2(0).Record(&service);

  // This event is discarded because its only metric gets stripped out.
  TestEvent1(id).SetNet_CacheBytes2(0).Record(&service);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  ASSERT_EQ(1, proto_report.entries_size());
  EXPECT_EQ(1, proto_report.entries(0).metrics_size());
  ASSERT_EQ(1, proto_report.aggregates().size());
  EXPECT_EQ(1u, proto_report.aggregates(0).dropped_due_to_filter());
  EXPECT_EQ(2, proto_report.aggregates(0).metrics_size());
  EXPECT_EQ(0u, proto_report.aggregates(0).metrics(0).dropped_due_to_filter());
  EXPECT_EQ(2u, proto_report.aggregates(0).metrics(1).dropped_due_to_filter());
}

TEST_F(UkmServiceTest, FilterRejectsEvent) {
  static const auto kTestEvent1EntryNameHash =
      base::HashMetricName(TestEvent1::kEntryName);

  class TestEntryFilter : public UkmEntryFilter {
   public:
    // This filter rejects all events that are not TestEvent1.
    bool FilterEntry(
        mojom::UkmEntry* entry,
        base::flat_set<uint64_t>* filtered_metric_hashes) override {
      if (entry->event_hash == kTestEvent1EntryNameHash) {
        return true;
      }

      filtered_metric_hashes->replace(base::ToVector(
          entry->metrics, &decltype(entry->metrics)::value_type::first));

      // Note that the event still contains metrics.
      return false;
    }
  };

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  service.RegisterEventFilter(std::make_unique<TestEntryFilter>());
  TestRecordingHelper recorder(&service);
  ASSERT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  SourceId id = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  TestEvent1(id).SetCpuTime(0).Record(&service);
  TestEvent2(id).SetDownloadService(3).Record(&service);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.entries_size());
  EXPECT_EQ(kTestEvent1EntryNameHash, proto_report.entries(0).event_hash());
  ASSERT_EQ(2, proto_report.aggregates_size());
  EXPECT_EQ(1u, proto_report.aggregates(0).dropped_due_to_filter());
  ASSERT_EQ(1, proto_report.aggregates(0).metrics_size());

  // No dropped_due_to_filter due to the value being equal to the entry's
  // droppeddropped_due_to_filter.
  EXPECT_FALSE(
      proto_report.aggregates(0).metrics(0).has_dropped_due_to_filter());
}

TEST_F(UkmServiceTest, PruneOldSources) {
  const GURL kURL("https://google.com/foobar");

  // Set the 'MaxKeptSources' value to 3 so it is easier to test.
  ScopedUkmFeatureParams params({{"MaxKeptSources", "3"}});

  ClearPrefs();
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Create 5 allowlisted ids. Allowlisted ids (like APP_ID) will not be
  // automatically removed when they emit events. They're only removed via the
  // pruning mechanism. Note that the are added in order, so 4 is the
  // youngest/newest.
  std::vector<SourceId> ids;
  base::TimeTicks last_time = base::TimeTicks::Now();
  for (int i = 0; i < 5; ++i) {
    // Wait until base::TimeTicks::Now() no longer equals |last_time|. This
    // ensures each source has a unique timestamp to avoid flakes. Should take
    // between 1-15ms per documented resolution of base::TimeTicks.
    while (base::TimeTicks::Now() == last_time) {
      base::PlatformThread::Sleep(base::Milliseconds(1));
    }
    ids.push_back(GetAllowlistedSourceId(i));
    recorder.UpdateSourceURL(ids.back(), kURL);
    last_time = base::TimeTicks::Now();
  }

  // Events on 0 and 4. This doesn't affect the pruning.
  TestEvent1(ids[0]).Record(&service);
  TestEvent1(ids[4]).Record(&service);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());
  auto proto_report = GetPersistedReport();

  EXPECT_EQ(5, proto_report.source_counts().observed());
  // All are navigation sources.
  EXPECT_EQ(5, proto_report.source_counts().navigation_sources());
  EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());

  // In all cases, 3 will be deferred since that is our max allowed.
  EXPECT_EQ(3, proto_report.source_counts().deferred_sources());
  // This is from last time, so none there.
  EXPECT_EQ(0, proto_report.source_counts().carryover_sources());

  // All 5 sources will be included in this first report.
  ASSERT_EQ(5, proto_report.sources_size());
  EXPECT_EQ(ids[0], proto_report.sources(0).id());
  EXPECT_EQ(ids[1], proto_report.sources(1).id());
  EXPECT_EQ(ids[2], proto_report.sources(2).id());
  EXPECT_EQ(ids[3], proto_report.sources(3).id());
  EXPECT_EQ(ids[4], proto_report.sources(4).id());

  // We have MaxKeptSources=3, and we keep by age, so we should keep 2,3,4.

  // New events on 0,2,4. This actually doesn't matter with respect to what
  // sources are emitted here, as some sources are already pruned.
  TestEvent1(ids[0]).Record(&service);
  TestEvent1(ids[2]).Record(&service);
  TestEvent1(ids[4]).Record(&service);

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(2, GetPersistedLogCount());
  proto_report = GetPersistedReport();

  // No new sources observed.
  EXPECT_EQ(0, proto_report.source_counts().observed());
  // 0 again, as this is for newly observed ones.
  EXPECT_EQ(0, proto_report.source_counts().navigation_sources());
  EXPECT_EQ(0, proto_report.source_counts().unmatched_sources());

  // Since no new sources added, we still are keeping the same 3. So all 3 are
  // kept and retained, in both cases.
  EXPECT_EQ(3, proto_report.source_counts().deferred_sources());
  EXPECT_EQ(3, proto_report.source_counts().carryover_sources());
  ASSERT_EQ(3, proto_report.sources_size());

  // 2, 3, 4 as these are the 3 newest.
  EXPECT_EQ(ids[2], proto_report.sources(0).id());
  EXPECT_EQ(ids[3], proto_report.sources(1).id());
  EXPECT_EQ(ids[4], proto_report.sources(2).id());
}

TEST_F(UkmServiceTest, UseExternalClientID) {
  prefs_.SetUint64(prefs::kUkmClientId, 1234);
  uint64_t external_client_id = 5678;
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>(),
                     external_client_id);
  service.Initialize();
  EXPECT_EQ(external_client_id, service.client_id());
  EXPECT_EQ(external_client_id, prefs_.GetUint64(prefs::kUkmClientId));
}

// Verifies that when a cloned install is detected, logs are purged.
TEST_F(UkmServiceTest, PurgeLogsOnClonedInstallDetected) {
  TestMetricsServiceClientWithClonedInstallDetector client;
  UkmService service(&prefs_, &client,
                     std::make_unique<MockDemographicMetricsProvider>());
  service.Initialize();

  // Store various logs.
  metrics::UnsentLogStore* test_log_store =
      service.reporting_service_for_testing().ukm_log_store();
  test_log_store->StoreLog(
      "dummy log data", metrics::LogMetadata(),
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  test_log_store->StageNextLog();
  test_log_store->StoreLog(
      "more dummy log data", metrics::LogMetadata(),
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_TRUE(test_log_store->has_staged_log());
  EXPECT_TRUE(test_log_store->has_unsent_logs());

  metrics::ClonedInstallDetector* cloned_install_detector =
      client.cloned_install_detector();
  cloned_install_detector->RegisterPrefs(prefs_.registry());

  static constexpr char kTestRawId[] = "test";
  // Hashed machine id for |kTestRawId|.
  static constexpr int kTestHashedId = 2216819;

  // Save a machine id that will not cause a clone to be detected.
  prefs_.SetInteger(metrics::prefs::kMetricsMachineId, kTestHashedId);
  cloned_install_detector->SaveMachineIdForTesting(&prefs_, kTestRawId);
  // Verify that the logs are still present.
  EXPECT_TRUE(test_log_store->has_staged_log());
  EXPECT_TRUE(test_log_store->has_unsent_logs());

  // Save a machine id that will cause a clone to be detected.
  prefs_.SetInteger(metrics::prefs::kMetricsMachineId, kTestHashedId + 1);
  cloned_install_detector->SaveMachineIdForTesting(&prefs_, kTestRawId);
  // Verify that the logs were purged.
  EXPECT_FALSE(test_log_store->has_staged_log());
  EXPECT_FALSE(test_log_store->has_unsent_logs());
}

TEST_F(UkmServiceTest, WebDXFeatures) {
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({UkmConsentType::MSBB});
  service.EnableReporting();

  // Record some web features data, create a report, and verify that the data in
  // it matches what was recorded.
  auto id0 = GetAllowlistedSourceId(0);
  recorder.UpdateSourceURL(id0, GURL("https://google.com/foobar0"));
  recorder.RecordWebDXFeatures(id0, {kWebDXFeature1},
                               kWebDXFeatureNumberOfFeaturesForTesting);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(GetPersistedLogCount(), 1);
  Report proto_report = GetPersistedReport();
  ASSERT_EQ(proto_report.web_features_size(), 1);
  EXPECT_EQ(proto_report.web_features(0).source_id(), id0);
  EXPECT_TRUE(WebDXFeaturesStrictlyContains(proto_report.web_features(0),
                                            {kWebDXFeature1}));

  // Record some more web features data, create a report, and verify that the
  // data in it matches what was recorded. The web features data from the
  // previous report should not appear.
  auto id1 = GetAllowlistedSourceId(1);
  recorder.UpdateSourceURL(id1, GURL("https://google.com/foobar1"));
  recorder.RecordWebDXFeatures(id1, {kWebDXFeature1, kWebDXFeature3},
                               kWebDXFeatureNumberOfFeaturesForTesting);
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(GetPersistedLogCount(), 2);
  proto_report = GetPersistedReport();
  ASSERT_EQ(proto_report.web_features_size(), 1);
  EXPECT_EQ(proto_report.web_features(0).source_id(), id1);
  EXPECT_TRUE(WebDXFeaturesStrictlyContains(proto_report.web_features(0),
                                            {kWebDXFeature1, kWebDXFeature3}));

  // Create a report without recording any web features data. Verify that it
  // contains no web features data, as the data from the previous reports should
  // not appear.
  auto id2 = GetAllowlistedSourceId(2);
  recorder.UpdateSourceURL(id2, GURL("https://google.com/foobar2"));
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  ASSERT_EQ(GetPersistedLogCount(), 3);
  proto_report = GetPersistedReport();
  EXPECT_EQ(proto_report.web_features_size(), 0);

  // Record some more web features data, purge the data, and then try to create
  // a report. Nothing should be created, and all the previous logs should also
  // be gone.
  auto id3 = GetAllowlistedSourceId(3);
  recorder.UpdateSourceURL(id3, GURL("https://google.com/foobar3"));
  recorder.RecordWebDXFeatures(id3, {kWebDXFeature3},
                               kWebDXFeatureNumberOfFeaturesForTesting);
  service.Purge();
  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(UkmServiceTest, NotifyObserverOnShutdown) {
  MockUkmRecorderObserver observer;
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  ukm::UkmRecorder::Get()->AddObserver(&observer);
  EXPECT_CALL(observer, OnStartingShutdown()).Times(1);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

class UkmServiceTestWithIndependentAppKM
    : public testing::TestWithParam<UkmConsentType> {
 public:
  UkmServiceTestWithIndependentAppKM()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());

    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmUnsentLogStore);
  }

  int GetPersistedLogCount() { return ukm::GetPersistedLogCount(prefs_); }

  Report GetPersistedReport() { return ukm::GetPersistedReport(prefs_); }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

}  // namespace

TEST_P(UkmServiceTestWithIndependentAppKM, RejectWhenNotConsented) {
  const GURL kURL("https://google.com/foobar");
  const GURL kAppURL("app://google.com/foobar");

  // Setup test constants from param.
  const auto consent = GetParam();
  const std::vector<int> app_indices = {1, 4};
  const std::vector<int> url_indices = {0, 2, 3};
  const std::vector<int>& expected_source_indices =
      (consent == UkmConsentType::APPS ? app_indices : url_indices);

  const int expected_result = expected_source_indices.size();

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording({consent});
  service.EnableReporting();

  std::vector<SourceId> source_ids;
  for (int i = 0; i < 5; ++i) {
    if (base::Contains(app_indices, i)) {
      source_ids.push_back(UkmServiceTest::GetAppIDSourceId(i));
      recorder.UpdateSourceURL(source_ids.back(), kAppURL);
    } else {
      source_ids.push_back(UkmServiceTest::GetAllowlistedSourceId(i));
      recorder.UpdateSourceURL(source_ids.back(), kURL);
    }

    TestEvent1(source_ids.back()).Record(&service);
  }

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
  EXPECT_EQ(1, GetPersistedLogCount());

  // Has the sources and entries associated with AppIDs.
  Report report = GetPersistedReport();
  EXPECT_EQ(expected_result, report.sources_size());
  EXPECT_EQ(expected_result, report.entries_size());

  for (int i = 0; i < expected_result; ++i) {
    EXPECT_EQ(source_ids[expected_source_indices[i]], report.sources(i).id());
    EXPECT_EQ(source_ids[expected_source_indices[i]],
              report.entries(i).source_id());
  }
}

INSTANTIATE_TEST_SUITE_P(
    UkmServiceTestWithIndependentAppKMGroup,
    UkmServiceTestWithIndependentAppKM,
    testing::Values(UkmConsentType::APPS, UkmConsentType::MSBB),
    [](const testing::TestParamInfo<
        UkmServiceTestWithIndependentAppKM::ParamType>& info) {
      if (info.param == UkmConsentType::APPS) {
        return "TestApps";
      } else {
        return "TestMSBB";
      }
    });

namespace {

class UkmServiceTestWithIndependentAppKMFullConsent
    : public testing::TestWithParam<bool> {
 public:
  UkmServiceTestWithIndependentAppKMFullConsent()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_current_default_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());

    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmUnsentLogStore);
  }

  int GetPersistedLogCount() { return ukm::GetPersistedLogCount(prefs_); }

  Report GetPersistedReport() { return ukm::GetPersistedReport(prefs_); }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

}  // namespace

TEST_P(UkmServiceTestWithIndependentAppKMFullConsent, VerifyAllAndNoneConsent) {
  const GURL kURL("https://google.com/foobar");
  const GURL kAppURL("app://google.com/foobar");
  const int kExpectedResultWithConsent = 5;

  // Setup test constants from param.
  const auto has_consent = GetParam();
  const auto consent_state =
      (has_consent ? UkmConsentState::All() : UkmConsentState());

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  TestRecordingHelper recorder(&service);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.UpdateRecording(consent_state);
  service.EnableReporting();

  const std::vector<int> app_indices = {1, 4};
  const std::vector<int> url_indices = {0, 2, 3};

  std::vector<SourceId> source_ids;
  for (int i = 0; i < 5; ++i) {
    if (base::Contains(app_indices, i)) {
      source_ids.push_back(UkmServiceTest::GetAppIDSourceId(i));
      recorder.UpdateSourceURL(source_ids.back(), kAppURL);
    } else {
      source_ids.push_back(UkmServiceTest::GetAllowlistedSourceId(i));
      recorder.UpdateSourceURL(source_ids.back(), kURL);
    }

    TestEvent1(source_ids.back()).Record(&service);
  }

  service.Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  EXPECT_EQ(GetPersistedLogCount(), static_cast<int>(has_consent));

  if (has_consent) {
    const auto report = GetPersistedReport();

    EXPECT_EQ(report.sources_size(), kExpectedResultWithConsent);
    EXPECT_EQ(report.entries_size(), kExpectedResultWithConsent);

    for (int i = 0; i < kExpectedResultWithConsent; ++i) {
      EXPECT_EQ(source_ids[i], report.sources(i).id());
      EXPECT_EQ(source_ids[i], report.entries(i).source_id());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    UkmServiceTestWithIndependentAppKMFullConsentGroup,
    UkmServiceTestWithIndependentAppKMFullConsent,
    testing::Values(true, false),
    [](const testing::TestParamInfo<
        UkmServiceTestWithIndependentAppKMFullConsent::ParamType>& info) {
      if (info.param) {
        return "TestAllConsent";
      } else {
        return "TestNoConsent";
      }
    });

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class MockUkmRecorder : public ukm::UkmRecorder {
 public:
  MockUkmRecorder() = default;
  ~MockUkmRecorder() override = default;

  MOCK_METHOD(void, AddEntry, (mojom::UkmEntryPtr entry), (override));
  MOCK_METHOD(void,
              RecordWebDXFeatures,
              (SourceId source_id,
               const std::set<int32_t>& features,
               const size_t max_feature_value),
              (override));
  MOCK_METHOD(void,
              UpdateSourceURL,
              (SourceId source_id, const GURL& url),
              (override));

  MOCK_METHOD(void,
              UpdateAppURL,
              (SourceId source_id, const GURL& url, const AppType app_type),
              (override));

  MOCK_METHOD(void,
              RecordNavigation,
              (SourceId source_id,
               const UkmSource::NavigationData& navigation_data),
              (override));

  MOCK_METHOD(void,
              MarkSourceForDeletion,
              (ukm::SourceId source_id),
              (override));
};

TEST_F(UkmReduceAddEntryIpcTest, RecordingEnabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(ukm::kUkmReduceAddEntryIPC));

  base::RunLoop run_loop;
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  // Initialize UkmService.
  service.Initialize();

  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(),
                               "Event.ScrollUpdate.Touch");
  builder.SetMetric("TimeToScrollUpdateSwapBegin", 17);

  // Custom UkmRecorder to intercept messages to and from UkmService to clients.
  MockUkmRecorder mock_recorder;
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  metrics::UkmRecorderFactoryImpl::Create(&mock_recorder,
                                          factory.BindNewPipeAndPassReceiver());

  // MojoUkmRecorder (client).
  auto mojo_recorder = MojoUkmRecorder::Create(*factory);

  service.EnableRecording();
  run_loop.RunUntilIdle();

  // Since UkmObservers list is empty, the final decision regarding sending the
  // AddEntry IPC from clients to UkmService depends on recording being
  // enabled/disabled.
  EXPECT_CALL(mock_recorder, AddEntry).Times(1);

  builder.Record(mojo_recorder.get());
  run_loop.RunUntilIdle();
}

TEST_F(UkmReduceAddEntryIpcTest, RecordingDisabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(ukm::kUkmReduceAddEntryIPC));

  base::RunLoop run_loop;
  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(),
                               "Event.ScrollUpdate.Touch");
  builder.SetMetric("TimeToScrollUpdateSwapBegin", 17);

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  // Initialize UkmService.
  service.Initialize();

  // Custom UkmRecorder to intercept messages to and from UkmService to clients.
  MockUkmRecorder mock_recorder;
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  metrics::UkmRecorderFactoryImpl::Create(&mock_recorder,
                                          factory.BindNewPipeAndPassReceiver());

  // MojoUkmRecorder (client).
  auto mojo_recorder = MojoUkmRecorder::Create(*factory);

  service.DisableRecording();
  run_loop.RunUntilIdle();

  // Since UkmObservers list is empty, the final decision regarding sending the
  // AddEntry IPC from clients to UkmService depends on recording being
  // enabled/disabled.
  EXPECT_CALL(mock_recorder, AddEntry).Times(0);

  builder.Record(mojo_recorder.get());
  run_loop.RunUntilIdle();
}

TEST_F(UkmReduceAddEntryIpcTest, AddRemoveUkmObserver) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(ukm::kUkmReduceAddEntryIPC));

  base::RunLoop run_loop;
  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  // Initialize UkmService.
  service.Initialize();

  // Custom UkmRecorder to intercept messages to and from UkmService to clients.
  MockUkmRecorder mock_recorder;
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  metrics::UkmRecorderFactoryImpl::Create(&mock_recorder,
                                          factory.BindNewPipeAndPassReceiver());

  // MojoUkmRecorder (client).
  auto mojo_recorder = MojoUkmRecorder::Create(*factory);

  // Recording Disabled.
  service.DisableRecording();
  run_loop.RunUntilIdle();

  mojom::UkmEntryPtr observed_ukm_entry;

  // UkmRecorderObservers with different event hashes. If an entry is seen at
  // client where event_hash matches with one from observers, we need to send
  // AddEntry IPC even if recording is disabled.
  UkmRecorderObserver obs1, obs2;
  base::flat_set<uint64_t> events1 = {
      base::HashMetricName("Event.ScrollUpdate.Touch")};
  service.AddUkmRecorderObserver(events1, &obs1);
  base::flat_set<uint64_t> events2 = {
      base::HashMetricName("Event.ScrollBegin.Wheel")};
  service.AddUkmRecorderObserver(events2, &obs2);
  run_loop.RunUntilIdle();

  {
    // This event is being observed by UkmRecorderObserver obs1. Hence, even
    // when UKM recording has been disabled, the event needs to be sent to
    // browser process.
    ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(),
                                 "Event.ScrollUpdate.Touch");
    builder.SetMetric("TimeToScrollUpdateSwapBegin", 17);
    builder.SetMetric("IsMainThread", 21);
    auto expected_ukm_entry = builder.GetEntryForTesting();
    // We expect this event will not be filtered at client,i.e., MojoUkmRecorder
    // and the mock method AddEntry would be called once.
    EXPECT_CALL(mock_recorder, AddEntry)
        .Times(1)
        .WillOnce(testing::Invoke([&](mojom::UkmEntryPtr entry) {
          observed_ukm_entry = std::move(entry);
        }));

    builder.Record(mojo_recorder.get());
    run_loop.RunUntilIdle();
    // Expects the UkmEntry seen at both sent(MojoUkmRecorder) and
    // receive(MockUkmRecorder) to be same.
    EXPECT_EQ(expected_ukm_entry, observed_ukm_entry);
  }
  {
    // This event is not being observed by any of UkmRecorderObservers and since
    // UKM recording has been disabled, the event will not be sent to browser
    // process.
    ukm::UkmEntryBuilder builder2(ukm::NoURLSourceId(), "Download.Interrupted");
    builder2.SetMetric("BytesWasted", 10);
    // Expect 0 calls to MockUkmRecorer::AddEntry since the UKM event will be
    // filtered out at the client.
    EXPECT_CALL(mock_recorder, AddEntry).Times(0);
    builder2.Record(mojo_recorder.get());
    run_loop.RunUntilIdle();
  }
  {
    // This event is being observed by UkmRecorderObserver obs2. Hence, even
    // when UKM recording has been disabled, the event needs to be sent to
    // browser process.
    ukm::UkmEntryBuilder builder3(ukm::NoURLSourceId(),
                                  "Event.ScrollBegin.Wheel");
    builder3.SetMetric("TimeToScrollUpdateSwapBegin", 25);
    auto expected_ukm_entry = builder3.GetEntryForTesting();

    EXPECT_CALL(mock_recorder, AddEntry)
        .Times(1)
        .WillOnce(testing::Invoke([&](mojom::UkmEntryPtr entry) {
          observed_ukm_entry = std::move(entry);
        }));
    builder3.Record(mojo_recorder.get());
    run_loop.RunUntilIdle();
    EXPECT_EQ(expected_ukm_entry, observed_ukm_entry);
  }
  // Remove UkmRecorderObserver obs1.
  service.RemoveUkmRecorderObserver(&obs1);
  run_loop.RunUntilIdle();
  {
    // This event is not being observed by any of the UkmRecorderObservers now,
    // and since UKM recording is disabled, it will be filtered out.
    ukm::UkmEntryBuilder builder4(ukm::NoURLSourceId(),
                                  "Event.ScrollUpdate.Touch");
    builder4.SetMetric("TimeToScrollUpdateSwapBegin", 17);
    // Expect 0 calls to MockUkmRecorer::AddEntry since the UKM event will be
    // filtered out at the client, because of UKM recording being disabled.
    EXPECT_CALL(mock_recorder, AddEntry).Times(0);
    builder4.Record(mojo_recorder.get());
    run_loop.RunUntilIdle();
  }
}

TEST_F(UkmReduceAddEntryIpcTest, MultipleDelegates) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(ukm::kUkmReduceAddEntryIPC));

  base::RunLoop run_loop;
  ukm::UkmEntryBuilder builder(ukm::NoURLSourceId(),
                               "Event.ScrollUpdate.Touch");
  builder.SetMetric("TimeToScrollUpdateSwapBegin", 17);

  UkmService service(&prefs_, &client_,
                     std::make_unique<MockDemographicMetricsProvider>());
  // Initialize UkmService.
  service.Initialize();
  MockUkmRecorder mock_recorder;
  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  metrics::UkmRecorderFactoryImpl::Create(&mock_recorder,
                                          factory.BindNewPipeAndPassReceiver());

  // MojoUkmRecorder (client).
  auto mojo_recorder = MojoUkmRecorder::Create(*factory);

  // Disabled recording but having multiple delegates will default clients
  // sending all AddEntry IPCs to the browser.
  service.DisableRecording();
  run_loop.RunUntilIdle();

  ukm::TestAutoSetUkmRecorder test_recorder;
  run_loop.RunUntilIdle();

  EXPECT_CALL(mock_recorder, AddEntry).Times(1);

  builder.Record(mojo_recorder.get());
  run_loop.RunUntilIdle();
}
}  // namespace ukm
