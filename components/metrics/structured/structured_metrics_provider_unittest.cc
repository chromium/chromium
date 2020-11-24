// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
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

// To test that the right values are calculated for hashed metrics, we need to
// set up some fake keys that we know the output hashes for. kKeyData contains
// the JSON for a simple structured_metrics.json file with keys for the test
// projects, "TestProjectOne" and "TestProjectTwo".
// TODO(crbug.com/1016655): Once custom rotation periods have been implemented,
// change the large constants to 0.
constexpr char kKeyData[] = R"({
  "keys":{
    "16881314472396226433":{
      "rotation_period":1000000,
      "last_rotation":1000000,
      "key":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    },
    "5876808001962504629":{
      "rotation_period":1000000,
      "last_rotation":1000000,
      "key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    }
  }
})";

// The name hash of "TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(15619026293081468407);
// The name hash of "TestEventTwo".
constexpr uint64_t kEventTwoHash = UINT64_C(15791833939776536363);
// The name hash of "TestEventThree".
constexpr uint64_t kEventThreeHash = UINT64_C(16464330721839207086);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);
// The name hash of "TestMetricThree".
constexpr uint64_t kMetricThreeHash = UINT64_C(13469300759843809564);
// The name hash of "TestMetricFour".
// constexpr uint64_t kMetricFourHash = UINT64_C(13469300759843809564);

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
    storage_ = new JsonPrefStore(temp_dir_.GetPath().Append("storage.json"));
    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
  }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

  void Wait() { task_environment_.RunUntilIdle(); }

  void WriteTestingKeys() {
    CHECK(base::ImportantFileWriter::WriteFileAtomically(
        TempDirPath().Append("structured_metrics.json"), kKeyData,
        "StructuredMetricsProviderTest"));
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

  bool is_initialized() { return provider_->initialized_; }
  bool is_recording_enabled() { return provider_->recording_enabled_; }

  void OnRecordingEnabled() { provider_->OnRecordingEnabled(); }

  void OnRecordingDisabled() { provider_->OnRecordingDisabled(); }

  void OnProfileAdded(const base::FilePath& path) {
    provider_->OnProfileAdded(path);
  }

  void CommitPendingWrite() {
    provider_->CommitPendingWriteForTest();
    Wait();
  }

  ChromeUserMetricsExtension GetProvidedEvents() {
    ChromeUserMetricsExtension uma_proto;
    provider_->ProvideCurrentSessionData(&uma_proto);
    return uma_proto;
  }

  // Most tests start without an existing structured_metrics.json storage file
  // on-disk, and so will trigger a single PREF_READ_ERROR_NO_FILE metric.
  // Expect that, and no other errors.
  void ExpectOnlyFileReadError() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
    histogram_tester_.ExpectUniqueSample(
        "UMA.StructuredMetrics.PrefReadError",
        PersistentPrefStore::PREF_READ_ERROR_NO_FILE, 1);
  }

 protected:
  std::unique_ptr<StructuredMetricsProvider> provider_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<JsonPrefStore> storage_;
};

// Simple test to ensure initialization works correctly in the case of a
// first-time run.
TEST_F(StructuredMetricsProviderTest, ProviderInitializesFromBlankSlate) {
  Init();
  EXPECT_TRUE(is_initialized());
  EXPECT_TRUE(is_recording_enabled());
  ExpectOnlyFileReadError();
}

// Ensure a call to OnRecordingDisabled prevents reporting.
TEST_F(StructuredMetricsProviderTest, EventsNotReportedWhenRecordingDisabled) {
  Init();
  OnRecordingDisabled();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);
  ExpectOnlyFileReadError();
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

  ExpectOnlyFileReadError();
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

  ExpectOnlyFileReadError();
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

  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 3);
  ExpectOnlyFileReadError();
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

  const auto uma = GetProvidedEvents();
  ASSERT_EQ(uma.structured_event_size(), 2);

  {  // First event
    const auto& event = uma.structured_event(0);
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
    const auto& event = uma.structured_event(1);
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

  const auto uma = GetProvidedEvents();
  ASSERT_EQ(uma.structured_event_size(), 3);

  const auto& event_one = uma.structured_event(0);
  const auto& event_two = uma.structured_event(1);
  const auto& event_three = uma.structured_event(2);

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
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 2);

  // But the previous events shouldn't appear in the second report.
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);

  events::test_project_one::TestEventOne().SetTestMetricTwo(3).Record();
  // The third request should only contain the third event.
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 1);

  ExpectOnlyFileReadError();
}

// Test that events recorded in one session are correctly persisted and are
// uploaded in the first report from a subsequent session.
TEST_F(StructuredMetricsProviderTest, EventsFromPreviousSessionAreReported) {
  // Start first session and record one event.
  Init();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1234).Record();

  // Write events to disk, then destroy the provider.
  CommitPendingWrite();
  provider_.reset();

  // Start a second session and ensure the event is reported.
  Init();
  const auto uma = GetProvidedEvents();
  ASSERT_EQ(uma.structured_event_size(), 1);
  ASSERT_EQ(uma.structured_event(0).metrics_size(), 1);
  EXPECT_EQ(uma.structured_event(0).metrics(0).value_int64(), 1234);

  ExpectOnlyFileReadError();
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
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);

  ExpectOnlyFileReadError();
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
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);

  ExpectOnlyFileReadError();
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
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 2);

  ExpectOnlyFileReadError();
}

// Ensure that a call to ProvideCurrentSessionData before initialization
// completes returns no events.
TEST_F(StructuredMetricsProviderTest,
       ReportsNothingBeforeInitializationComplete) {
  provider_ = std::make_unique<StructuredMetricsProvider>();
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);
  OnRecordingEnabled();
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);
  OnProfileAdded(TempDirPath());
  EXPECT_EQ(GetProvidedEvents().structured_event_size(), 0);
}

// Ensure an old structured_metrics.json file correctly migrates to the new
// format
TEST_F(StructuredMetricsProviderTest, MigrateEventsKey) {
  const auto json_path = TempDirPath().Append("structured_metrics.json");

  // Write a json file with the old format.
  const std::string old_json = R"({
    "events":[
      {"id":"some_id",
       "metrics":[{
          "name":"some_name",
          "value":"some_value"}]}]
  })";
  CHECK(base::ImportantFileWriter::WriteFileAtomically(
      TempDirPath().Append("structured_metrics.json"), old_json,
      "StructuredMetricsProviderTest"));

  // Initialize and trigger a migration by recording an event.
  Init();
  events::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  CommitPendingWrite();

  // Check that the new format has the structure:
  // {"events": {"associated": [{...}, {...}]}}
  std::string new_json;
  ASSERT_TRUE(base::ReadFileToString(json_path, &new_json));
  const auto value = base::JSONReader::Read(new_json);
  ASSERT_TRUE(value.has_value());

  const auto* events = value.value().FindKey("events");
  ASSERT_TRUE(events != nullptr);
  ASSERT_TRUE(events->is_dict());
  ASSERT_EQ(events->DictSize(), 1U);

  const auto* associated = events->FindKey("associated");
  ASSERT_TRUE(associated != nullptr);
  ASSERT_TRUE(associated->is_list());
  ASSERT_EQ(associated->GetList().size(), 2U);
}

}  // namespace structured
}  // namespace metrics
