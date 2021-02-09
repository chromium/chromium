// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_events.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {
namespace structured {

namespace {

// These project, event, and metric names are used for testing.
// - project: TestProjectOne
//   - event: TestEventOne
//     - metric: TestMetricOne
//     - metric: TestMetricTwo
// - project: TestProjectTwo
//   - event: TestEventTwo
//     - metric: TestMetricThree
//   - event: TestEventThree
//     - metric: TestMetricFour

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = UINT64_C(5876808001962504629);

// The name hash of "chrome::TestProjectOne::TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);
// The name hash of "chrome::TestProjectTwo::TestEventTwo".
constexpr uint64_t kEventTwoHash = UINT64_C(8995967733561999410);
// The name hash of "chrome::TestProjectTwo::TestEventThree".
constexpr uint64_t kEventThreeHash = UINT64_C(5848687377041124372);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);
// The name hash of "TestMetricThree".
constexpr uint64_t kMetricThreeHash = UINT64_C(13469300759843809564);

// The hex-encoded first 8 bytes of SHA256("aaa...a")
constexpr char kProjectOneId[] = "3BA3F5F43B926026";
// The hex-encoded first 8 bytes of SHA256("bbb...b")
constexpr char kProjectTwoId[] = "BDB339768BC5E4FE";

// Test values.
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

}  // namespace

class StructuredMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
  }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

  base::FilePath KeyFilePath() {
    return temp_dir_.GetPath().Append("structured_metrics").Append("keys");
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void WriteTestingKeys() {
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyDataProto proto;
    KeyProto& key_one = (*proto.mutable_keys())[kProjectOneHash];
    key_one.set_key("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    key_one.set_last_rotation(today);
    key_one.set_rotation_period(90);

    KeyProto& key_two = (*proto.mutable_keys())[kProjectTwoHash];
    key_two.set_key("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    key_two.set_last_rotation(today);
    key_two.set_rotation_period(90);

    base::CreateDirectory(KeyFilePath().DirName());
    ASSERT_TRUE(base::WriteFile(KeyFilePath(), proto.SerializeAsString()));
  }

  // Simulates the three external events that the structure metrics system cares
  // about: the metrics service initializing and enabling its providers, and a
  // user logging in.
  void Init() {
    // Create the provider, normally done by the ChromeMetricsServiceClient.
    provider_ = std::make_unique<StructuredMetricsProvider>();
    // Enable recording, normally done after the metrics service has checked
    // consent allows recording.
    provider_->OnRecordingEnabled();
    // Add a profile, normally done by the ChromeMetricsServiceClient after a
    // user logs in.
    provider_->OnProfileAdded(TempDirPath());
    Wait();
  }

  bool is_initialized() {
    return provider_->init_state_ ==
           StructuredMetricsProvider::InitState::kInitialized;
  }
  bool is_recording_enabled() { return provider_->recording_enabled_; }

  void OnRecordingEnabled() { provider_->OnRecordingEnabled(); }

  void OnRecordingDisabled() { provider_->OnRecordingDisabled(); }

  void OnProfileAdded(const base::FilePath& path) {
    provider_->OnProfileAdded(path);
  }

  void WriteNow() {
    provider_->WriteNowForTest();
    Wait();
  }

  StructuredDataProto GetSessionData() {
    ChromeUserMetricsExtension uma_proto;
    provider_->ProvideCurrentSessionData(&uma_proto);
    return uma_proto.structured_data();
  }

  StructuredDataProto GetIndependentMetrics() {
    CHECK(provider_->HasIndependentMetrics());
    ChromeUserMetricsExtension uma_proto;
    provider_->ProvideIndependentMetrics(
        base::BindOnce([](bool success) { CHECK(success); }), &uma_proto,
        nullptr);
    return uma_proto.structured_data();
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

 protected:
  std::unique_ptr<StructuredMetricsProvider> provider_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
};

// Simple test to ensure initialization works correctly in the case of a
// first-time run.
TEST_F(StructuredMetricsProviderTest, ProviderInitializesFromBlankSlate) {
  Init();
  EXPECT_TRUE(is_initialized());
  EXPECT_TRUE(is_recording_enabled());
  ExpectNoErrors();
}

// Ensure a call to OnRecordingDisabled prevents reporting.
TEST_F(StructuredMetricsProviderTest, EventsNotReportedWhenRecordingDisabled) {
  Init();
  OnRecordingDisabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  EXPECT_EQ(GetSessionData().events_size(), 0);
  ExpectNoErrors();
}

// Ensure that, if recording is disabled part-way through initialization, the
// initialization still completes correctly, but recording is correctly set to
// disabled.
TEST_F(StructuredMetricsProviderTest, RecordingDisabledDuringInitialization) {
  provider_ = std::make_unique<StructuredMetricsProvider>();

  OnProfileAdded(TempDirPath());
  OnRecordingDisabled();
  EXPECT_FALSE(is_initialized());
  EXPECT_FALSE(is_recording_enabled());

  Wait();
  EXPECT_TRUE(is_initialized());
  EXPECT_FALSE(is_recording_enabled());

  ExpectNoErrors();
}

// Ensure that recording is disabled until explicitly enabled with a call to
// OnRecordingEnabled.
TEST_F(StructuredMetricsProviderTest, RecordingDisabledByDefault) {
  provider_ = std::make_unique<StructuredMetricsProvider>();

  OnProfileAdded(TempDirPath());
  Wait();
  EXPECT_TRUE(is_initialized());
  EXPECT_FALSE(is_recording_enabled());

  OnRecordingEnabled();
  EXPECT_TRUE(is_recording_enabled());

  ExpectNoErrors();
}

TEST_F(StructuredMetricsProviderTest, RecordedEventAppearsInReport) {
  Init();

  events::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();
  events::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();
  events::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();

  EXPECT_EQ(GetSessionData().events_size(), 3);
  ExpectNoErrors();
}

TEST_F(StructuredMetricsProviderTest, EventsReportedCorrectly) {
  WriteTestingKeys();
  Init();

  events::test_project_one::TestEventOne()
      .SetTestMetricOne(kValueOne)
      .SetTestMetricTwo(12345)
      .Record();
  events::test_project_two::TestEventTwo()
      .SetTestMetricThree(kValueTwo)
      .Record();

  const auto data = GetSessionData();
  ASSERT_EQ(data.events_size(), 2);

  {  // First event
    const auto& event = data.events(0);
    EXPECT_EQ(event.event_name_hash(), kEventOneHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectOneId);
    ASSERT_EQ(event.metrics_size(), 2);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricOneHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("aaa...a", concat(hex(kMetricOneHash),
                // "value one"))
                "8C2469269D142715");
    }

    {  // Second metric
      const auto& metric = event.metrics(1);
      EXPECT_EQ(metric.name_hash(), kMetricTwoHash);
      EXPECT_EQ(metric.value_int64(), 12345);
    }
  }

  {  // Second event
    const auto& event = data.events(1);
    EXPECT_EQ(event.event_name_hash(), kEventTwoHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectTwoId);
    ASSERT_EQ(event.metrics_size(), 1);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricThreeHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("bbb...b", concat(hex(kProjectHash),
                // "value three"))
                "86F0169868588DC7");
    }
  }

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.PrefReadError", 0);
}

TEST_F(StructuredMetricsProviderTest, EventsWithinProjectReportedWithSameID) {
  WriteTestingKeys();
  Init();

  events::test_project_one::TestEventOne().Record();
  events::test_project_two::TestEventTwo().Record();
  events::test_project_two::TestEventThree().Record();

  const auto data = GetSessionData();
  ASSERT_EQ(data.events_size(), 3);

  const auto& event_one = data.events(0);
  const auto& event_two = data.events(1);
  const auto& event_three = data.events(2);

  // Check events are in the right order.
  EXPECT_EQ(event_one.event_name_hash(), kEventOneHash);
  EXPECT_EQ(event_two.event_name_hash(), kEventTwoHash);
  EXPECT_EQ(event_three.event_name_hash(), kEventThreeHash);

  // Events two and three share a project, so should have the same ID. Event
  // one should have its own ID.
  EXPECT_EQ(HashToHex(event_one.profile_event_id()), kProjectOneId);
  EXPECT_EQ(HashToHex(event_two.profile_event_id()), kProjectTwoId);
  EXPECT_EQ(HashToHex(event_three.profile_event_id()), kProjectTwoId);

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.PrefReadError", 0);
}

// Test that a call to ProvideCurrentSessionData clears the provided events from
// the cache, and a subsequent call does not return those events again.
TEST_F(StructuredMetricsProviderTest, EventsClearedAfterReport) {
  Init();

  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::test_project_one::TestEventOne().SetTestMetricTwo(2).Record();
  // Should provide both the previous events.
  EXPECT_EQ(GetSessionData().events_size(), 2);

  // But the previous events shouldn't appear in the second report.
  EXPECT_EQ(GetSessionData().events_size(), 0);

  events::test_project_one::TestEventOne().SetTestMetricTwo(3).Record();
  // The third request should only contain the third event.
  EXPECT_EQ(GetSessionData().events_size(), 1);

  ExpectNoErrors();
}

// Test that events recorded in one session are correctly persisted and are
// uploaded in the first report from a subsequent session.
TEST_F(StructuredMetricsProviderTest, EventsFromPreviousSessionAreReported) {
  // Start first session and record one event.
  Init();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1234).Record();

  // Write events to disk, then destroy the provider.
  WriteNow();
  provider_.reset();

  // Start a second session and ensure the event is reported.
  Init();
  const auto data = GetSessionData();
  ASSERT_EQ(data.events_size(), 1);
  ASSERT_EQ(data.events(0).metrics_size(), 1);
  EXPECT_EQ(data.events(0).metrics(0).value_int64(), 1234);

  ExpectNoErrors();
}

// Test that events reported at various stages before and during initialization
// are ignored (and don't cause a crash).
TEST_F(StructuredMetricsProviderTest, EventsNotRecordedBeforeInitialization) {
  // Manually create and initialize the provider, adding recording calls between
  // each step. All of these events should be ignored.
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  provider_ = std::make_unique<StructuredMetricsProvider>();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  OnRecordingEnabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  OnProfileAdded(TempDirPath());
  // This one should still fail even though all of the initialization calls are
  // done, because the provider hasn't finished loading the keys from disk.
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  Wait();
  EXPECT_EQ(GetSessionData().events_size(), 0);

  ExpectNoErrors();
}

// Ensure a call to OnRecordingDisabled not only prevents the reporting of new
// events, but also clears the cache of any existing events that haven't yet
// been reported.
TEST_F(StructuredMetricsProviderTest,
       ExistingEventsClearedWhenRecordingDisabled) {
  Init();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  OnRecordingDisabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  EXPECT_EQ(GetSessionData().events_size(), 0);

  ExpectNoErrors();
}

// Ensure that recording and reporting is re-enabled after recording is disabled
// and then enabled again.
TEST_F(StructuredMetricsProviderTest, ReportingResumesWhenEnabled) {
  Init();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  OnRecordingDisabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();

  OnRecordingEnabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  EXPECT_EQ(GetSessionData().events_size(), 2);

  ExpectNoErrors();
}

// Ensure that a call to ProvideCurrentSessionData before initialization
// completes returns no events.
TEST_F(StructuredMetricsProviderTest,
       ReportsNothingBeforeInitializationComplete) {
  provider_ = std::make_unique<StructuredMetricsProvider>();
  EXPECT_EQ(GetSessionData().events_size(), 0);
  OnRecordingEnabled();
  EXPECT_EQ(GetSessionData().events_size(), 0);
  OnProfileAdded(TempDirPath());
  EXPECT_EQ(GetSessionData().events_size(), 0);
}

}  // namespace structured
}  // namespace metrics
