// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_prefs.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/test/test_event_storage.h"
#include "components/metrics/structured/test/test_key_data_provider.h"
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

}  // namespace

class StructuredMetricsServiceTest : public testing::Test {
 public:
  StructuredMetricsServiceTest() {
    reporting::StructuredMetricsReportingService::RegisterPrefs(
        prefs_.registry());
  }

  ~StructuredMetricsServiceTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({kEnabledStructuredMetricsService}, {});

    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&test_recorder_);

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    WriteTestingDeviceKeys();

    WriteTestingProfileKeys();
  }

  void TearDown() override {
    StructuredMetricsClient::Get()->UnsetDelegate();
    service_.reset();
    Wait();
  }

  void Init() {
    auto key_data_provider =
        std::make_unique<TestKeyDataProvider>(DeviceKeyFilePath());
    TestKeyDataProvider* test_key_data_provider = key_data_provider.get();
    auto recorder = base::MakeRefCounted<StructuredMetricsRecorder>(
        std::move(key_data_provider), std::make_unique<TestEventStorage>());

    service_ = std::make_unique<StructuredMetricsService>(&client_, &prefs_,
                                                          std::move(recorder));
    // Register the profile with the key data provider.
    test_key_data_provider->OnProfileAdded(temp_dir_.GetPath());
    Wait();
  }

  void EnableRecording() { service_->EnableRecording(); }
  void EnableReporting() { service_->EnableReporting(); }

  void DisableRecording() { service_->DisableRecording(); }
  void DisableReporting() { service_->DisableReporting(); }

  base::FilePath ProfileKeyFilePath() {
    return temp_dir_.GetPath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("keys"));
  }

  base::FilePath DeviceKeyFilePath() {
    return temp_dir_.GetPath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("device_keys"));
  }

  base::FilePath DeviceEventsFilePath() {
    return temp_dir_.GetPath()
        .Append(FILE_PATH_LITERAL("structured_metrics"))
        .Append(FILE_PATH_LITERAL("events"));
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
        // Set to 3 so logs are not dropped in the test.
        UnsentLogStore::UnsentLogStoreLimits{
            .min_log_count = 3,
        },
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
  metrics::TestMetricsServiceClient client_;

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;

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

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1.0)));
  Wait();

  service_->Purge();
  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Nothing should be stored.
  EXPECT_THAT(GetPersistedLogCount(), 0);
}

TEST_F(StructuredMetricsServiceTest, PurgePersisted) {
  Init();

  EnableRecording();
  EnableReporting();

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1.0)));
  Wait();

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

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1)));
  Wait();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
  service_.reset();
}

TEST_F(StructuredMetricsServiceTest, SystemProfileFilled) {
  Init();

  EnableRecording();
  EnableReporting();

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1)));
  Wait();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
  EXPECT_TRUE(uma_proto.has_system_profile());

  const SystemProfileProto& system_profile = uma_proto.system_profile();
  EXPECT_EQ(system_profile.channel(), client_.GetChannel());
  EXPECT_EQ(system_profile.app_version(), client_.GetVersionString());
}

TEST_F(StructuredMetricsServiceTest, DoesNotRecordWhenRecordingDisabled) {
  Init();
  EnableRecording();
  EnableReporting();

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1)));
  Wait();

  DisableRecording();

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1)));
  Wait();

  EnableRecording();

  service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
}

TEST_F(StructuredMetricsServiceTest, FlushOnShutdown) {
  Init();
  EnableRecording();
  EnableReporting();

  StructuredMetricsClient::Record(
      std::move(TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(
      std::move(TestEventSeven().SetTestMetricSeven(1)));
  Wait();

  // Will flush the log.
  service_.reset();

  const auto uma_proto = GetPersistedLog();
  EXPECT_THAT(uma_proto.structured_data().events().size(), 2);
}

}  // namespace metrics::structured
