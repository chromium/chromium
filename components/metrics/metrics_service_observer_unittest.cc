// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_observer.h"

#include "base/json/json_string_value_serializer.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

class MetricsServiceObserverTest : public testing::Test {
 public:
  MetricsServiceObserverTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        enabled_state_provider_(/*consent=*/true, /*enabled=*/true) {}
  ~MetricsServiceObserverTest() override = default;

  void SetUp() override {
    // The following call is needed for calling MetricsService::Start(), which
    // sets up callbacks for user actions (which in turn verifies that a task
    // runner is provided).
    base::SetRecordActionTaskRunner(task_runner_);
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
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

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

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();

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
  ASSERT_EQ(log_info->events.size(), 1U);
  EXPECT_EQ(log_info->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Run pending tasks to trigger the uploading of the log.
  task_runner_->RunPendingTasks();

  // Verify that |logs_observer| observed the log being sent.
  ASSERT_EQ(log_info->events.size(), 2U);
  EXPECT_EQ(log_info->events[1].event,
            MetricsLogsEventManager::LogEvent::kLogUploading);

  // Simulate the upload being completed successfully.
  client.uploader()->CompleteUpload(200);

  // Verify that |logs_observer| observed the log being uploaded and then
  // finally discarded.
  ASSERT_EQ(log_info->events.size(), 4U);
  EXPECT_EQ(log_info->events[2].event,
            MetricsLogsEventManager::LogEvent::kLogUploaded);
  EXPECT_EQ(log_info->events[3].event,
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

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();

  // Verify that |logs_observer| is aware of the log.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 1U);
  MetricsServiceObserver::Log* log_info = observed_logs->front().get();

  // Stage the log, and verify that |logs_observer| observed this event.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  test_log_store->StageNextLog();
  ASSERT_TRUE(test_log_store->has_staged_log());
  ASSERT_EQ(log_info->events.size(), 1U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Run pending tasks to trigger the uploading of the log, and verify that
  // |logs_observer| observed this event.
  task_runner_->RunPendingTasks();
  EXPECT_EQ(log_info->events.size(), 2U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogUploading);

  // Simulate the upload failing due to a timeout.
  client.uploader()->CompleteUpload(408);

  // Verify that |logs_observer| observed the log being re-staged for
  // re-transmission.
  EXPECT_EQ(log_info->events.size(), 3U);
  EXPECT_EQ(log_info->events.back().event,
            MetricsLogsEventManager::LogEvent::kLogStaged);

  // Run pending tasks to trigger the uploading of the log, and verify that
  // |logs_observer| observed this event.
  task_runner_->RunPendingTasks();
  EXPECT_EQ(log_info->events.size(), 4U);

  // Simulate the upload failing due to bad syntax.
  client.uploader()->CompleteUpload(400);

  // Verify that |logs_observer| observed the log being discarded because it was
  // failed to be uploaded due to a bad request, and should not be
  // re-transmitted.
  EXPECT_EQ(log_info->events.size(), 5U);
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
  client.set_max_ongoing_log_size(1);

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
  test_log_store->StoreLog(/*log_data=*/".", MetricsLog::LogType::ONGOING_LOG,
                           metadata);

  // Verify that |logs_observer| is aware of the log.
  std::vector<std::unique_ptr<MetricsServiceObserver::Log>>* observed_logs =
      logs_observer.logs_for_testing();
  ASSERT_EQ(observed_logs->size(), 1U);
  MetricsServiceObserver::Log* log_info = observed_logs->front().get();

  // Trim logs and verify that |logs_observer| is aware that the log was
  // trimmed.
  test_log_store->TrimAndPersistUnsentLogs(/*overwrite_in_memory_store=*/true);
  ASSERT_EQ(log_info->events.size(), 1U);
  EXPECT_EQ(log_info->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  service.RemoveLogsObserver(&logs_observer);
}

// Verifies that MetricsServiceObserver is notified when logs are trimmed due to
// enough logs being stored.
TEST_F(MetricsServiceObserverTest, TrimLongLogList) {
  TestMetricsServiceClient client;

  // Set the mininimum log count to 1 and minimum log size to 1 byte. This
  // essentially means that the log store, when trimming logs, will only keep
  // the most recent one. I.e., after storing one log, it will trim the rest
  // due to having stored enough logs.
  client.set_min_ongoing_log_queue_size(1);
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
    test_log_store->StoreLog(/*log_data=*/base::NumberToString(i),
                             MetricsLog::LogType::ONGOING_LOG, metadata);
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
  ASSERT_EQ(observed_logs->at(0)->events.size(), 1U);
  EXPECT_EQ(observed_logs->at(0)->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  ASSERT_EQ(observed_logs->at(1)->events.size(), 1U);
  EXPECT_EQ(observed_logs->at(1)->events[0].event,
            MetricsLogsEventManager::LogEvent::kLogTrimmed);

  ASSERT_EQ(observed_logs->at(2)->events.size(), 0U);

  service.RemoveLogsObserver(&logs_observer);
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

  // Run pending tasks to finish init task and complete the first ongoing log.
  task_runner_->RunPendingTasks();

  // Stage the log.
  MetricsLogStore* test_log_store = service.LogStoreForTest();
  test_log_store->StageNextLog();
  ASSERT_TRUE(test_log_store->has_staged_log());

  // Run pending tasks to trigger the uploading of the log.
  task_runner_->RunPendingTasks();

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

  base::Value* log_type = logs_dict.Find("log_type");
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
  ASSERT_EQ(log_events_list.size(), 2U);

  base::Value& first_log_event = log_events_list[0];
  ASSERT_TRUE(first_log_event.is_dict());
  base::Value::Dict& first_log_event_dict = first_log_event.GetDict();
  base::Value* first_log_event_string = first_log_event_dict.Find("event");
  ASSERT_TRUE(first_log_event_string);
  ASSERT_TRUE(first_log_event_string->is_string());
  EXPECT_EQ(first_log_event_string->GetString(), "Staged");
  base::Value* first_log_event_timestamp =
      first_log_event_dict.Find("timestamp");
  ASSERT_TRUE(first_log_event_timestamp);
  ASSERT_TRUE(first_log_event_timestamp->is_string());
  EXPECT_FALSE(first_log_event_timestamp->GetString().empty());

  base::Value& second_log_event = log_events_list[1];
  ASSERT_TRUE(second_log_event.is_dict());
  base::Value::Dict& second_log_event_dict = second_log_event.GetDict();
  base::Value* second_log_event_string = second_log_event_dict.Find("event");
  ASSERT_TRUE(second_log_event_string);
  ASSERT_TRUE(second_log_event_string->is_string());
  EXPECT_EQ(second_log_event_string->GetString(), "Uploading");
  base::Value* second_log_event_timestamp =
      second_log_event_dict.Find("timestamp");
  ASSERT_TRUE(second_log_event_timestamp);
  ASSERT_TRUE(second_log_event_timestamp->is_string());
  EXPECT_FALSE(second_log_event_timestamp->GetString().empty());

  service.RemoveLogsObserver(&logs_observer);
}

}  // namespace metrics
