// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_recorder.h"
#include "components/metrics/structured/test/test_event_storage.h"
#include "components/metrics/structured/test/test_key_data_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {

namespace {

constexpr char kHwid[] = "hwid";
constexpr size_t kUserCount = 3;

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
      metrics::SystemProfileProto* proto) override {
    proto->set_multi_profile_user_count(kUserCount);
    proto->mutable_hardware()->set_full_hardware_class(kHwid);
  }
};

}  // namespace

class StructuredMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    // Provider is being deprecated and new features are breaking the provider.
    GTEST_SKIP();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&recorder_);
    // Move the mock date forward from day 0, because KeyData assumes that day 0
    // is a bug.
    task_environment_.AdvanceClock(base::Days(1000));

    scoped_feature_list_.InitAndDisableFeature(
        kEnabledStructuredMetricsService);
  }

  void TearDown() override { StructuredMetricsClient::Get()->UnsetDelegate(); }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

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

  void Wait() { task_environment_.RunUntilIdle(); }

  // Simulates the three external events that the structure metrics system cares
  // about: the metrics service initializing and enabling its providers, and a
  // user logging in.
  void Init() {
    auto key_data_provider = std::make_unique<TestKeyDataProvider>(
        DeviceKeyFilePath(), ProfileKeyFilePath());
    test_key_data_provider_ = key_data_provider.get();
    // Create a system profile, normally done by ChromeMetricsServiceClient.
    structured_metrics_recorder_ = std::make_unique<StructuredMetricsRecorder>(
        std::move(key_data_provider), std::make_unique<TestEventStorage>());
    // Create the provider, normally done by the ChromeMetricsServiceClient.
    provider_ = std::unique_ptr<StructuredMetricsProvider>(
        new StructuredMetricsProvider(
            /*min_independent_metrics_interval=*/
            base::Seconds(0), structured_metrics_recorder_.get()));
    // Enable recording, normally done after the metrics service has checked
    // consent allows recording.
    provider_->OnRecordingEnabled();
  }

  void OnRecordingEnabled() { provider_->OnRecordingEnabled(); }

  void OnProfileAdded(const base::FilePath& path) {
    test_key_data_provider_->OnProfileAdded(path);
  }

  StructuredDataProto GetSessionData() {
    ChromeUserMetricsExtension uma_proto;
    provider_->ProvideCurrentSessionData(&uma_proto);
    Wait();
    return uma_proto.structured_data();
  }

  StructuredDataProto GetIndependentMetrics() {
    ChromeUserMetricsExtension uma_proto;
    if (provider_->HasIndependentMetrics()) {
      provider_->ProvideIndependentMetrics(
          base::DoNothing(),
          base::BindOnce([](bool success) { CHECK(success); }), &uma_proto,
          nullptr);
      Wait();
      return uma_proto.structured_data();
    }

    auto p = StructuredDataProto();
    return p;
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  bool RecorderInitialized() { return provider_->recorder().IsInitialized(); }

 protected:
  std::unique_ptr<TestSystemProfileProvider> system_profile_provider_;
  std::unique_ptr<StructuredMetricsRecorder> structured_metrics_recorder_;
  std::unique_ptr<StructuredMetricsProvider> provider_;
  raw_ptr<TestKeyDataProvider> test_key_data_provider_;
  // Feature list should be constructed before task environment.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;

 private:
  TestRecorder recorder_;
};

// Ensure that disabling independent upload of non-client_id metrics via feature
// flag instead uploads them in the main UMA upload.
TEST_F(StructuredMetricsProviderTest, DisableIndependentUploads) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kStructuredMetrics,
      {{"enable_independent_metrics_upload", "false"}});

  Init();

  // Add a profile, normally done by the ChromeMetricsServiceClient after a
  // user logs in.
  OnProfileAdded(TempDirPath());
  Wait();

  OnRecordingEnabled();
  StructuredMetricsClient::Record(std::move(
      events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(std::move(
      events::v2::test_project_three::TestEventFour().SetTestMetricFour(1)));
  EXPECT_EQ(GetIndependentMetrics().events_size(), 0);
  EXPECT_EQ(GetSessionData().events_size(), 2);
  ExpectNoErrors();
}

TEST_F(StructuredMetricsProviderTest, NoIndependentUploadsBeforeInitialized) {
  Init();
  // Verify the recorder is not initialized.
  EXPECT_FALSE(RecorderInitialized());

  StructuredMetricsClient::Record(std::move(
      events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1)));
  StructuredMetricsClient::Record(std::move(
      events::v2::test_project_three::TestEventFour().SetTestMetricFour(1)));
  EXPECT_EQ(GetIndependentMetrics().events_size(), 0);
  EXPECT_EQ(GetSessionData().events_size(), 0);
  ExpectNoErrors();
}

}  // namespace metrics::structured
