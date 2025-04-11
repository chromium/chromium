// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_observer.h"

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_scheduler.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace {

class MetricsServiceObserverTest : public testing::Test {
 public:
  MetricsServiceObserverTest()
      : enabled_state_provider_(/*consent=*/true, /*enabled=*/true) {}
  ~MetricsServiceObserverTest() override = default;

  void SetUp() override {
    // The following call is needed for calling MetricsService::Start(), which
    // sets up callbacks for user actions (which in turn verifies that a task
    // runner is provided).
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    // The following call is needed for instantiating an instance of
    // MetricsStateManager, which reads various prefs in its constructor.
    MetricsService::RegisterPrefs(local_state_.registry());
  }

  MetricsStateManager* GetMetricsStateManager() {
    // Lazy-initialize |metrics_state_manager_| so that it correctly reads the
    // stability state from prefs after tests have a chance to initialize it.
    if (!metrics_state_manager_) {
      metrics_state_manager_ = MetricsStateManager::Create(
          local_state(), enabled_state_provider(),
          /*backup_registry_key=*/std::wstring(),
          /*user_data_dir=*/base::FilePath(), StartupVisibility::kUnknown);
      metrics_state_manager_->InstantiateFieldTrialList();
    }
    return metrics_state_manager_.get();
  }

  EnabledStateProvider* enabled_state_provider() {
    return &enabled_state_provider_;
  }

  PrefService* local_state() { return &local_state_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestEnabledStateProvider enabled_state_provider_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<MetricsStateManager> metrics_state_manager_;
};

class MetricsServiceObserverExportTest
    : public MetricsServiceObserverTest,
      public testing::WithParamInterface<bool> {
 public:
  MetricsServiceObserverExportTest() = default;
  ~MetricsServiceObserverExportTest() override = default;

  bool ShouldIncludeLogProtoData() { return GetParam(); }
};

}  // namespace

// Verifies that MetricsServiceObserver is notified when a log is created,
// staged, uploading, uploaded, and finally discarded.
TEST_F(MetricsServiceObserverTest, SuccessfulLogUpload) {
  TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, local_state());

  // Create a MetricsServiceObserver that will observe logs from |service|.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);
  service.AddLogsObserver(&logs_observer);

  // Start the metrics service.
  service.InitializeMetricsRecordingState();
  service.Start();

  // Fast forward the time until the the first ongoing log is completed.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));

  // Verify that |logs_observer| is aware of the log.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 1U);
  MetricsServiceObserver::Log* log_info = observed_logs->front().get();

  // Stage the log.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  test_log_store->StageNextLog();
  ASSERT_TRUE(test_log_store->has_staged_log());

  // Verify that |logs_observer| is aware that the log was staged.
  ASSERT_EQ(log_info->events.size(), 2U);
  EXPECT_EQ(log_info->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogCreated);
  EXPECT_EQ(log_info->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Fast forward the time to trigger the uploading of the log.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());

  // Verify that |logs_observer| observed the log being sent.
  ASSERT_EQ(log_info->events.size(), 3U);
  EXPECT_EQ(log_info->events[2].event,
            MetricsLogsEventManager::LogEvent::kLogUploading);

  // Simulate the upload being completed successfully.
  client.uploader()->CompleteUpload(200);

  // Verify that |logs_observer| observed the log being uploaded and then
  // finally discarded.
  ASSERT_EQ(log_info->events.size(), 5U);
  EXPECT_EQ(log_info->events[3].event,
            MetricsLogsEventManager::LogEvent::kLogUploaded);
  EXPECT_EQ(log_info->events[4].event,
            MetricsLogsEventManager::LogEvent::kLogDiscarded);

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that MetricsServiceObserver is notified when 1) a log is re-staged
// to be re-transmitted after failing to be uploaded, and 2) a log is discarded
// after failing to be uploaded because of a bad request.
TEST_F(MetricsServiceObserverTest, UnsuccessfulLogUpload) {
  TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, local_state());

  // Create a MetricsServiceObserver that will observe logs from |service|.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);
  service.AddLogsObserver(&logs_observer);

  // Start the metrics service.
  service.InitializeMetricsRecordingState();
  service.Start();

  // Fast forward the time until the the first ongoing log is completed.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));

  // Verify that |logs_observer| is aware of the log.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 1U);
  MetricsServiceObserver::Log* log_info = observed_logs->front().get();

  // Stage the log, and verify that |logs_observer| observed this event.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  test_log_store->StageNextLog();
  ASSERT_TRUE(test_log_store->has_staged_log());
  ASSERT_EQ(log_info->events.size(), 2U);
  EXPECT_EQ(log_info->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogCreated);
  EXPECT_EQ(log_info->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Fast forward the time to trigger the uploading of the log, and verify that
  // |logs_observer| observed this event.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());
  EXPECT_EQ(log_info->events.size(), 3U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogUploading);

  // Simulate the upload failing due to a timeout.
  client.uploader()->CompleteUpload(408);

  // Verify that |logs_observer| observed the log being re-staged for
  // re-transmission.
  EXPECT_EQ(log_info->events.size(), 4U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Fast forward the time to trigger the re-upload of the log, and verify that
  // |logs_observer| observed this event. Since the last upload failed, the time
  // before the next upload is triggered is different (longer).
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetInitialBackoffInterval());
  EXPECT_EQ(log_info->events.size(), 5U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogUploading);

  // Simulate the upload failing due to bad syntax.
  client.uploader()->CompleteUpload(400);

  // Verify that |logs_observer| observed the log being discarded because it was
  // failed to be uploaded due to a bad request, and should not be
  // re-transmitted.
  EXPECT_EQ(log_info->events.size(), 6U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogDiscarded);

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that MetricsServiceObserver is notified when a log that is too
// large gets trimmed.
TEST_F(MetricsServiceObserverTest, TrimLargeLog) {
  TestMetricsServiceClient client;

  // Set the max log size to be 1 byte so that pretty much all logs will be
  // trimmed. We don't set it to 0 bytes because that is a special value that
  // represents no max size.
  client.set_max_ongoing_log_size_bytes(1);

  MetricsService service(GetMetricsStateManager(), &client, local_state());

  // Create a MetricsServiceObserver that will observe logs from |service|.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);
  service.AddLogsObserver(&logs_observer);

  // Initialize the metrics service.
  service.InitializeMetricsRecordingState();

  // Store some arbitrary log.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  LogMetadata metadata;
  test_log_store->StoreLog(
      /*log_data=*/".", MetricsLog::LogType::ONGOING_LOG, metadata,
      MetricsLogsEventManager::CreateReason::kUnknown);

  // Verify that |logs_observer| is aware of the log.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 1U);
  MetricsServiceObserver::Log* log_info = observed_logs->front().get();
  ASSERT_EQ(log_info->events.size(), 1U);
  EXPECT_EQ(log_info->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogCreated);

  // Trim logs and verify that |logs_observer| is aware that the log was
  // trimmed.
  test_log_store->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
  ASSERT_EQ(log_info->events.size(), 2U);
  EXPECT_EQ(log_info->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that MetricsServiceObserver is notified when logs are trimmed due to
// enough logs being stored.
TEST_F(MetricsServiceObserverTest, TrimLongLogList) {
  TestMetricsServiceClient client;

  // Set the minimum log count to 1 and minimum log size to 1 byte. This
  // essentially means that the log store, when trimming logs, will only keep
  // the most recent one. I.e., after storing one log, it will trim the rest
  // due to having stored enough logs.
  client.set_min_ongoing_log_queue_size_bytes(1);
  client.set_min_ongoing_log_queue_count(1);

  MetricsService service(GetMetricsStateManager(), &client, local_state());

  // Create a MetricsServiceObserver that will observe logs from |service|.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);
  service.AddLogsObserver(&logs_observer);

  // Initialize the metrics service.
  service.InitializeMetricsRecordingState();

  // Store 3 arbitrary logs.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  LogMetadata metadata;
  for (int i = 0; i < 3; i++) {
    test_log_store->StoreLog(
        /*log_data=*/base::NumberToString(i), MetricsLog::LogType::ONGOING_LOG,
        metadata, MetricsLogsEventManager::CreateReason::kUnknown);
  }

  // Verify that |logs_observer| is aware of the 3 logs.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 3U);

  // Trim logs.
  test_log_store->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

  // Verify that all logs except the last one (the most recent one) have been
  // trimmed.
  ASSERT_EQ(observed_logs->size(), 3U);
  ASSERT_EQ(observed_logs->at(0)->events.size(), 2U);
  EXPECT_EQ(observed_logs->at(0)->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  ASSERT_EQ(observed_logs->at(1)->events.size(), 2U);
  EXPECT_EQ(observed_logs->at(1)->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  ASSERT_EQ(observed_logs->at(2)->events.size(), 1U);

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that logs created through MetricsLogStore, which is used by UMA, are
// annotated with a type (ongoing, independent, or stability).
TEST_F(MetricsServiceObserverTest, UmaLogType) {
  // Verify that logs created through MetricsLogStore::StoreLog() will be
  // annotated with a type.
  {
    MetricsLogsEventManager logs_event_manager;
    TestMetricsServiceClient client;
    MetricsLogStore test_log_store(local_state(), client.GetStorageLimits(),
                                   client.GetUploadSigningKey(),
                                   &logs_event_manager);

    // Create a MetricsServiceObserver that will observe UMA logs from
    // |test_log_store|, which notifies through |logs_event_manager|.
    MetricsServiceObserver logs_observer(
        MetricsServiceObserver::MetricsServiceType::UMA);
    logs_event_manager.AddObserver(&logs_observer);

    // Load logs from persistent storage, which is needed to internally
    // initialize |test_log_store|. There should be no logs loaded.
    test_log_store.LoadPersistedUnsentLogs();
    std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* logs =
        logs_observer.logs_for_testing();
    EXPECT_EQ(logs->size(), 0U);

    test_log_store.StoreLog("Ongoing Log", MetricsLog::LogType::ONGOING_LOG,
                            LogMetadata(),
                            MetricsLogsEventManager::CreateReason::kUnknown);
    ASSERT_EQ(logs->size(), 1U);
    ASSERT_TRUE(logs->back()->type.has_value());
    EXPECT_EQ(logs->back()->type.value(), MetricsLog::LogType::ONGOING_LOG);
    test_log_store.StoreLog("Independent Log",
                            MetricsLog::LogType::INDEPENDENT_LOG, LogMetadata(),
                            MetricsLogsEventManager::CreateReason::kUnknown);
    ASSERT_EQ(logs->size(), 2U);
    ASSERT_TRUE(logs->back()->type.has_value());
    EXPECT_EQ(logs->back()->type.value(), MetricsLog::LogType::INDEPENDENT_LOG);
    test_log_store.StoreLog(
        "Stability Log", MetricsLog::LogType::INITIAL_STABILITY_LOG,
        LogMetadata(), MetricsLogsEventManager::CreateReason::kUnknown);
    ASSERT_EQ(logs->size(), 3U);
    ASSERT_TRUE(logs->back()->type.has_value());
    EXPECT_EQ(logs->back()->type.value(),
              MetricsLog::LogType::INITIAL_STABILITY_LOG);

    // Store logs in persistent storage, in preparation for the next assertions.
    test_log_store.TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);

    logs_event_manager.RemoveObserver(&logs_observer);
  }

  // Verify that logs loaded from persistent storage are annotated with a type.
  {
    MetricsLogsEventManager logs_event_manager;
    TestMetricsServiceClient client;
    MetricsLogStore test_log_store(local_state(), client.GetStorageLimits(),
                                   client.GetUploadSigningKey(),
                                   &logs_event_manager);

    // Create a MetricsServiceObserver that will observe UMA logs from
    // |test_log_store|, which notifies through |logs_event_manager|.
    MetricsServiceObserver logs_observer(
        MetricsServiceObserver::MetricsServiceType::UMA);
    logs_event_manager.AddObserver(&logs_observer);

    // Load logs from persistent storage, which were created previously.
    std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* logs =
        logs_observer.logs_for_testing();
    EXPECT_EQ(logs->size(), 0U);
    test_log_store.LoadPersistedUnsentLogs();

    // Verify that logs were observed by |logs_observer|, and that they were
    // annotated with a type. There should be 3 logs. Note that the order in
    // which the logs are loaded (stability first, then ongoing) is hardcoded in
    // MetricsLogStore. Further, the "independent" log should have become an
    // "ongoing" log (due to limitations on how logs are stored in persistent
    // storage).
    ASSERT_EQ(logs->size(), 3U);
    ASSERT_TRUE(logs->at(0)->type.has_value());
    EXPECT_EQ(logs->at(0)->type.value(),
              MetricsLog::LogType::INITIAL_STABILITY_LOG);
    ASSERT_TRUE(logs->at(1)->type.has_value());
    EXPECT_EQ(logs->at(1)->type.value(), MetricsLog::LogType::ONGOING_LOG);
    ASSERT_TRUE(logs->at(2)->type.has_value());
    EXPECT_EQ(logs->at(2)->type.value(), MetricsLog::LogType::ONGOING_LOG);

    logs_event_manager.RemoveObserver(&logs_observer);
  }

  // Verify that when loading logs from persistent storage after setting an
  // "alternate ongoing log store", the logs are annotated with a type.
  {
    MetricsLogsEventManager logs_event_manager;
    TestMetricsServiceClient client;
    MetricsLogStore::StorageLimits storage_limits = client.GetStorageLimits();
    MetricsLogStore test_log_store(local_state(), storage_limits,
                                   client.GetUploadSigningKey(),
                                   &logs_event_manager);

    // Load logs from persistent storage, which is needed to internally
    // initialize |test_log_store|. There should be 3 logs loaded (however, we
    // do not care about them).
    test_log_store.LoadPersistedUnsentLogs();

    // Create a MetricsServiceObserver that will observe UMA logs from
    // |test_log_store|, which notifies through |logs_event_manager|.
    MetricsServiceObserver logs_observer(
        MetricsServiceObserver::MetricsServiceType::UMA);
    logs_event_manager.AddObserver(&logs_observer);

    // Verify that |logs_observer| is not aware of any logs (since we started
    // observing *after* logs from persistent storage were loaded).
    std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* logs =
        logs_observer.logs_for_testing();
    EXPECT_EQ(logs->size(), 0U);

    // Create an UnsentLogStore, which will be used as the "alternate ongoing
    // log store". This log store will read from the same persistent storage as
    // a "normal" ongoing log queue.
    auto alternate_ongoing_log_store = std::make_unique<UnsentLogStore>(
        std::make_unique<UnsentLogStoreMetricsImpl>(), local_state(),
        prefs::kMetricsOngoingLogs, prefs::kMetricsOngoingLogsMetadata,
        storage_limits.ongoing_log_queue_limits, client.GetUploadSigningKey(),
        // |logs_event_manager| will be set by |test_log_store| directly in
        // MetricsLogStore::SetAlternateOngoingLogStore().
        /*logs_event_manager=*/nullptr);

    // Set the alternate ongoing log store of |test_log_store|. This should load
    // logs from persistent storage.
    test_log_store.SetAlternateOngoingLogStore(
        std::move(alternate_ongoing_log_store));

    // Verify that logs were observed by |logs_observer|, and that they were
    // annotated with a type. There should be 2 logs. The "independent" log
    // should have become an "ongoing" log (due to limitations on how logs are
    // stored in persistent storage).
    ASSERT_EQ(logs->size(), 2U);
    ASSERT_TRUE(logs->at(0)->type.has_value());
    EXPECT_EQ(logs->at(0)->type.value(), MetricsLog::LogType::ONGOING_LOG);
    ASSERT_TRUE(logs->at(1)->type.has_value());
    EXPECT_EQ(logs->at(1)->type.value(), MetricsLog::LogType::ONGOING_LOG);

    logs_event_manager.RemoveObserver(&logs_observer);
  }
}

INSTANTIATE_TEST_SUITE_P(MetricsServiceObserverExportTests,
                         MetricsServiceObserverExportTest,
                         testing::Bool());

// Verifies that MetricsServiceObserver::ExportLogsAsJson() returns a valid
// JSON string that represents the logs that the observer is aware of.
// This test is parameterized (bool parameter). When the parameter is true,
// we should include log proto data when exporting. When false, log proto data
// should not be included.
TEST_P(MetricsServiceObserverExportTest, ExportLogsAsJson) {
  TestMetricsServiceClient client;
  MetricsService service(GetMetricsStateManager(), &client, local_state());

  // Create a MetricsServiceObserver that will observe logs from |service|.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);
  service.AddLogsObserver(&logs_observer);

  // Start the metrics service.
  service.InitializeMetricsRecordingState();
  service.Start();

  // Fast forward the time until the the first ongoing log is completed.
  task_environment_.FastForwardBy(
      base::Seconds(MetricsScheduler::GetInitialIntervalSeconds()));

  // Stage the log.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  test_log_store->StageNextLog();
  ASSERT_TRUE(test_log_store->has_staged_log());

  // Fast forward the time to trigger the uploading of the log.
  task_environment_.FastForwardBy(
      MetricsUploadScheduler::GetUnsentLogsInterval());

  // Export logs as a JSON string.
  std::string json;
  bool include_log_proto_data = ShouldIncludeLogProtoData();
  ASSERT_TRUE(logs_observer.ExportLogsAsJson(include_log_proto_data, &json));

  // Parse the JSON string and convert it to a base::Value.
  JSONStringValueDeserializer deserializer(json);
  std::unique_ptr<base::Value> logs_value = deserializer.Deserialize(
      /*error_code=*/nullptr, /*error_message=*/nullptr);
  ASSERT_TRUE(logs_value);

  // Verify that the base::Value created from the JSON reflects what we expect:
  // |logs_observer| should be aware of a single log, which has been staged and
  // is being uploaded.
  ASSERT_TRUE(logs_value->is_dict());
  base::Value::Dict& logs_dict = logs_value->GetDict();

  base::Value* log_type = logs_dict.Find("logType");
  ASSERT_TRUE(log_type);
  ASSERT_TRUE(log_type->is_string());
  EXPECT_EQ(log_type->GetString(), "UMA");

  base::Value* logs_list_value = logs_dict.Find("logs");
  ASSERT_TRUE(logs_list_value);
  ASSERT_TRUE(logs_list_value->is_list());
  base::Value::List& logs_list = logs_list_value->GetList();
  ASSERT_EQ(logs_list.size(), 1U);

  base::Value& log_value = logs_list.front();
  ASSERT_TRUE(log_value.is_dict());
  base::Value::Dict& log_dict = log_value.GetDict();

  base::Value* uma_log_type = log_dict.Find("type");
  ASSERT_TRUE(uma_log_type);
  ASSERT_TRUE(uma_log_type->is_string());
  EXPECT_EQ(uma_log_type->GetString(), "Ongoing");

  base::Value* log_hash = log_dict.Find("hash");
  ASSERT_TRUE(log_hash);
  ASSERT_TRUE(log_hash->is_string());
  EXPECT_FALSE(log_hash->GetString().empty());

  base::Value* log_timestamp = log_dict.Find("timestamp");
  ASSERT_TRUE(log_timestamp);
  ASSERT_TRUE(log_timestamp->is_string());
  EXPECT_FALSE(log_timestamp->GetString().empty());

  base::Value* log_data = log_dict.Find("data");
  if (include_log_proto_data) {
    ASSERT_TRUE(log_data);
    ASSERT_TRUE(log_data->is_string());
    EXPECT_FALSE(log_data->GetString().empty());

    // Verify that the proto data can be parsed.
    std::string gzipped_log_data;
    ASSERT_TRUE(base::Base64Decode(log_data->GetString(), &gzipped_log_data));
    ChromeUserMetricsExtension uma_proto;
    ASSERT_TRUE(DecodeLogDataToProto(gzipped_log_data, &uma_proto));
    EXPECT_TRUE(uma_proto.has_client_id());
    EXPECT_TRUE(uma_proto.has_session_id());
    EXPECT_TRUE(uma_proto.has_system_profile());
  } else {
    EXPECT_FALSE(log_data);
  }

  base::Value* log_size = log_dict.Find("size");
  ASSERT_TRUE(log_size);
  ASSERT_TRUE(log_size->is_int());

  base::Value* log_events = log_dict.Find("events");
  ASSERT_TRUE(log_events);
  ASSERT_TRUE(log_events->is_list());
  base::Value::List& log_events_list = log_events->GetList();
  ASSERT_EQ(log_events_list.size(), 3U);

  base::Value& first_log_event = log_events_list[0];
  ASSERT_TRUE(first_log_event.is_dict());
  base::Value::Dict& first_log_event_dict = first_log_event.GetDict();
  base::Value* first_log_event_string = first_log_event_dict.Find("event");
  ASSERT_TRUE(first_log_event_string);
  ASSERT_TRUE(first_log_event_string->is_string());
  EXPECT_EQ(first_log_event_string->GetString(), "Created");
  base::Value* first_log_event_timestamp =
      first_log_event_dict.Find("timestampMs");
  ASSERT_TRUE(first_log_event_timestamp);
  ASSERT_TRUE(first_log_event_timestamp->is_double());

  base::Value& second_log_event = log_events_list[1];
  ASSERT_TRUE(second_log_event.is_dict());
  base::Value::Dict& second_log_event_dict = second_log_event.GetDict();
  base::Value* second_log_event_string = second_log_event_dict.Find("event");
  ASSERT_TRUE(second_log_event_string);
  ASSERT_TRUE(second_log_event_string->is_string());
  EXPECT_EQ(second_log_event_string->GetString(), "Staged");
  base::Value* second_log_event_timestamp =
      second_log_event_dict.Find("timestampMs");
  ASSERT_TRUE(second_log_event_timestamp);
  ASSERT_TRUE(second_log_event_timestamp->is_double());

  base::Value& third_log_event = log_events_list[2];
  ASSERT_TRUE(third_log_event.is_dict());
  base::Value::Dict& third_log_event_dict = third_log_event.GetDict();
  base::Value* third_log_event_string = third_log_event_dict.Find("event");
  ASSERT_TRUE(third_log_event_string);
  ASSERT_TRUE(third_log_event_string->is_string());
  EXPECT_EQ(third_log_event_string->GetString(), "Uploading");
  base::Value* third_log_event_timestamp =
      third_log_event_dict.Find("timestampMs");
  ASSERT_TRUE(third_log_event_timestamp);
  ASSERT_TRUE(third_log_event_timestamp->is_double());

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that callbacks registered to a MetricsServiceObserver instance are
// run every time it is notified.
TEST_F(MetricsServiceObserverTest, NotifiedCallbacks) {
  // Create a MetricsServiceObserver.
  MetricsServiceObserver logs_observer(
      MetricsServiceObserver::MetricsServiceType::UMA);

  int num_callback_executed = 0;

  {
    // Add a callback to |logs_observer| that increments |num_callback_executed|
    // every time it is run.
    base::CallbackListSubscription callback_subscription =
        logs_observer.AddNotifiedCallback(base::BindLambdaForTesting(
            [&num_callback_executed]() { num_callback_executed++; }));
    EXPECT_EQ(num_callback_executed, 0);

    // Verify that OnLogCreated() triggers the callback.
    const std::string kLogHash = "test";
    logs_observer.OnLogCreated(kLogHash, /*log_data=*/"", /*log_timestamp=*/"",
                               MetricsLogsEventManager::CreateReason::kUnknown);
    EXPECT_EQ(num_callback_executed, 1);

    // Verify that OnLogEvent() triggers the callback.
    logs_observer.OnLogEvent(MetricsLogsEventManager::LogEvent::kLogStaged,
                             kLogHash, /*message=*/"");
    EXPECT_EQ(num_callback_executed, 2);
  }

  // Verify that after the callback list subscription |callback_subscription| is
  // destroyed, it is automatically de-registered from |logs_observer|.
  const std::string kLogHash2 = "test2";
  num_callback_executed = 0;
  logs_observer.OnLogCreated(kLogHash2, /*log_data=*/"", /*log_timestamp=*/"",
                             MetricsLogsEventManager::CreateReason::kUnknown);
  logs_observer.OnLogEvent(MetricsLogsEventManager::LogEvent::kLogStaged,
                           kLogHash2, /*message=*/"");
  EXPECT_EQ(num_callback_executed, 0);
}

}  // namespace metrics
