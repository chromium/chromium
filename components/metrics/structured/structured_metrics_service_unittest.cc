// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics/unsent_log_store_metrics_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {
namespace {

using events::v2::test_project_one::TestEventOne;
using events::v2::test_project_six::TestEventSeven;

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectThree".
constexpr uint64_t kProjectThreeHash = UINT64_C(10860358748803291132);

class TestRecorder : public StructuredMetricsClient::RecordingDelegate {
 public:
  TestRecorder() = default;
  TestRecorder(const TestRecorder& recorder) = delete;
  TestRecorder& operator=(const TestRecorder& recorder) = delete;
  ~TestRecorder() override = default;

  void RecordEvent(Event&& event) override {
    Recorder::GetInstance()->RecordEvent(std::move(event));
  }

  bool IsReadyToRecord() const override { return true; }
};

class TestSystemProfileProvider : public metrics::MetricsProvider {
 public:
  TestSystemProfileProvider() = default;
  TestSystemProfileProvider(const TestSystemProfileProvider& recorder) = delete;
  TestSystemProfileProvider& operator=(
      const TestSystemProfileProvider& recorder) = delete;
  ~TestSystemProfileProvider() override = default;

  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* proto) override {}
};

}  // namespace

class StructuredMetricsServiceTest : public testing::Test {
 public:
  StructuredMetricsServiceTest() {
    reporting::StructuredMetricsReportingService::RegisterPrefs(
        prefs_.registry());

    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&test_recorder_);
  }

  void SetUp() override {
    feature_list_.InitWithFeatures({kEnabledStructuredMetricsService}, {});

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    WriteTestingDeviceKeys();

    system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();

    WriteTestingProfileKeys();
  }

  void Init() {
    auto recorder = std::unique_ptr<StructuredMetricsRecorder>(
        new StructuredMetricsRecorder(DeviceKeyFilePath(), base::Seconds(0),
                                      system_profile_provider_.get()));
    recorder->OnProfileAdded(temp_dir_.GetPath());
    service_ = std::unique_ptr<StructuredMetricsService>(
        new StructuredMetricsService(&client_, &prefs_, std::move(recorder)));
    Wait();
  }

  void EnableRecording() { service_->EnableRecording(); }
  void EnableReporting() { service_->EnableReporting(); }

  void DisableRecording() { service_->DisableRecording(); }
  void DisableReporting() { service_->DisableReporting(); }

  base::FilePath ProfileKeyFilePath() {
    return temp_dir_.GetPath().Append("structured_metrics").Append("keys");
  }

  base::FilePath DeviceKeyFilePath() {
    return temp_dir_.GetPath()
        .Append("structured_metrics")
        .Append("device_keys");
  }

  void WriteTestingProfileKeys() {
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyDataProto proto;
    KeyProto& key_one = (*proto.mutable_keys())[kProjectOneHash];
    key_one.set_key("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    key_one.set_last_rotation(today);
    key_one.set_rotation_period(90);

    KeyProto& key_three = (*proto.mutable_keys())[kProjectThreeHash];
    key_three.set_key("cccccccccccccccccccccccccccccccc");
    key_three.set_last_rotation(today);
    key_three.set_rotation_period(90);

    base::CreateDirectory(ProfileKeyFilePath().DirName());
    ASSERT_TRUE(
        base::WriteFile(ProfileKeyFilePath(), proto.SerializeAsString()));
    Wait();
  }

  void WriteTestingDeviceKeys() {
    base::CreateDirectory(DeviceKeyFilePath().DirName());
    ASSERT_TRUE(base::WriteFile(DeviceKeyFilePath(),
                                KeyDataProto().SerializeAsString()));
    Wait();
  }

  int GetPersistedLogCount() {
    return prefs_.GetList(prefs::kLogStoreName).size();
  }

  ChromeUserMetricsExtension GetPersistedLog() {
    EXPECT_THAT(GetPersistedLogCount(), 1);
    metrics::UnsentLogStore result_unsent_log_store(
        std::make_unique<UnsentLogStoreMetricsImpl>(), &prefs_,
        prefs::kLogStoreName, /*metadata_pref_name=*/nullptr,
        /*min_log_count=*/3, /*min_log_bytes=*/1000,
        /*max_log_size=*/0,
        /*signing_key=*/std::string(),
        /*logs_event_manager=*/nullptr);

    result_unsent_log_store.LoadPersistedUnsentLogs();
    result_unsent_log_store.StageNextLog();

    ChromeUserMetricsExtension uma_proto;
    EXPECT_TRUE(metrics::DecodeLogDataToProto(
        result_unsent_log_store.staged_log(), &uma_proto));
    return uma_proto;
  }

  StructuredMetricsService& service() { return *service_.get(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  void AdvanceClock(int hours) {
    task_environment_.AdvanceClock(base::Hours(hours));
  }

 protected:
  std::unique_ptr<StructuredMetricsService> service_;

 private:
  base::test::ScopedFeatureList feature_list_;
  metrics::TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;

  std::unique_ptr<TestSystemProfileProvider> system_profile_provider_;
  TestRecorder test_recorder_;
  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(StructuredMetricsServiceTest, PurgeInMemory) {
  Init();

  EnableRecording();
  EnableReporting();

  TestEventOne().SetTestMetricTwo(1).Record();
  TestEventSeven().SetTestMetricSeven(1.0).Record();

  service_->Purge();
  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Nothing should be stored.
  EXPECT_THAT(GetPersistedLogCount(), 0);
}

TEST_F(StructuredMetricsServiceTest, PurgePersisted) {
  Init();

  EnableRecording();
  EnableReporting();

  TestEventOne().SetTestMetricTwo(1).Record();
  TestEventSeven().SetTestMetricSeven(1.0).Record();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  service_->Purge();

  // Need to make sure there is a log to read.
  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Nothing should be stored.
  EXPECT_THAT(GetPersistedLogCount(), 0);
}

TEST_F(StructuredMetricsServiceTest, RotateLogs) {
  Init();

  EnableRecording();
  EnableReporting();

  TestEventOne().SetTestMetricTwo(1).Record();
  TestEventSeven().SetTestMetricSeven(1).Record();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
}

TEST_F(StructuredMetricsServiceTest, DoesNotRecordWhenRecordingDisabled) {
  Init();
  EnableRecording();
  EnableReporting();

  TestEventOne().SetTestMetricTwo(1).Record();
  TestEventSeven().SetTestMetricSeven(1).Record();

  DisableRecording();

  TestEventOne().SetTestMetricTwo(1).Record();
  TestEventSeven().SetTestMetricSeven(1).Record();

  EnableRecording();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
}

}  // namespace metrics::structured
