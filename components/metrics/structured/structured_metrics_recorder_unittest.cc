// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_recorder.h"
#include <cstdint>

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
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/storage.pb.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics::structured {

namespace {

// These project, event, and metric names are used for testing.

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = UINT64_C(5876808001962504629);
// The name hash of "TestProjectThree".
constexpr uint64_t kProjectThreeHash = UINT64_C(10860358748803291132);
// The name hash of "TestProjectFour".
constexpr uint64_t kProjectFourHash = UINT64_C(6801665881746546626);
// The name hash of "TestProjectFive"
constexpr uint64_t kProjectFiveHash = UINT64_C(3960582687892677139);
// The name hash of "TestProjectSix"
constexpr uint64_t kProjectSixHash = UINT64_C(6972396123792667134);
// The name hash of "CrOSEvents"
constexpr uint64_t kCrOSEventsProjectHash = UINT64_C(12657197978410187837);

// The name hash of "chrome::TestProjectOne::TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);
// The name hash of "chrome::TestProjectTwo::TestEventTwo".
constexpr uint64_t kEventTwoHash = UINT64_C(8995967733561999410);
// The name hash of "chrome::TestProjectTwo::TestEventThree".
constexpr uint64_t kEventThreeHash = UINT64_C(5848687377041124372);
// The name hash of "chrome::TestProjectThree::TestEventFour".
constexpr uint64_t kEventFourHash = UINT64_C(1718797808092246258);
// The name hash of "chrome::TestProjectFour::TestEventFive".
constexpr uint64_t kEventFiveHash = UINT64_C(7045523601811399253);
// The name hash of "chrome::TestProjectFour::TestEventSix".
constexpr uint64_t kEventSixHash = UINT64_C(2873337042686447043);
// The name hash of "chrome::TestProjectSix::TestEventSeven".
constexpr uint64_t kEventSevenHash = UINT64_C(16749091071228286247);
// The name hash of "chrome::CrOSEvents::NoMetricsEvent".
constexpr uint64_t kNoMetricsEventHash = UINT64_C(5106854608989380457);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);
// The name hash of "TestMetricThree".
constexpr uint64_t kMetricThreeHash = UINT64_C(13469300759843809564);
// The name hash of "TestMetricFour".
constexpr uint64_t kMetricFourHash = UINT64_C(2917855408523247722);
// The name hash of "TestMetricFive".
constexpr uint64_t kMetricFiveHash = UINT64_C(8665976921794972190);
// The name hash of "TestMetricSix".
constexpr uint64_t kMetricSixHash = UINT64_C(3431522567539822144);
// The name hash of "TestMetricSeven".
constexpr uint64_t kMetricSevenHash = UINT64_C(8395865158198697574);

// The hex-encoded first 8 bytes of SHA256("aaa...a")
constexpr char kProjectOneId[] = "3BA3F5F43B926026";
// The hex-encoded first 8 bytes of SHA256("bbb...b")
constexpr char kProjectTwoId[] = "BDB339768BC5E4FE";
// The hex-encoded first 8 bytes of SHA256("ddd...d")
constexpr char kProjectFourId[] = "FBBBB6DE2AA74C3C";

// Test values.
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";

constexpr char kHwid[] = "hwid";
constexpr size_t kUserCount = 3;

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

// Make a simple testing proto with one |uma_events| message for each id in
// |ids|.
EventsProto MakeExternalEventProto(const std::vector<uint64_t>& ids) {
  EventsProto proto;

  for (const auto id : ids) {
    auto* event = proto.add_uma_events();
    event->set_profile_event_id(id);
  }

  return proto;
}

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

class TestStructuredMetricsRecorder : public StructuredMetricsRecorder {
 public:
  TestStructuredMetricsRecorder(
      const base::FilePath& device_key_path,
      base::raw_ptr<metrics::MetricsProvider> system_profile_provider)
      : StructuredMetricsRecorder(device_key_path,
                                  /*write_delay=*/base::Seconds(0),
                                  system_profile_provider) {}

  using StructuredMetricsRecorder::StructuredMetricsRecorder;
};

}  // namespace

class StructuredMetricsRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    StructuredMetricsClient::Get()->SetDelegate(&test_recorder_);
    // Move the mock date forward from day 0, because KeyData assumes that day 0
    // is a bug.
    task_environment_.AdvanceClock(base::Days(1000));
  }

  base::FilePath TempDirPath() { return temp_dir_.GetPath(); }

  base::FilePath ProfileKeyFilePath() {
    return temp_dir_.GetPath().Append("structured_metrics").Append("keys");
  }

  base::FilePath DeviceKeyFilePath() {
    return temp_dir_.GetPath()
        .Append("structured_metrics")
        .Append("device_keys");
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void WriteTestingProfileKeys() {
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
    const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();

    KeyDataProto proto;
    KeyProto& key = (*proto.mutable_keys())[kProjectFourHash];
    key.set_key("dddddddddddddddddddddddddddddddd");
    key.set_last_rotation(today);
    key.set_rotation_period(90);

    base::CreateDirectory(DeviceKeyFilePath().DirName());
    ASSERT_TRUE(
        base::WriteFile(DeviceKeyFilePath(), proto.SerializeAsString()));
    Wait();
  }

  KeyDataProto ReadKeys(const base::FilePath& filepath) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    Wait();
    CHECK(base::PathExists(filepath));

    std::string proto_str;
    CHECK(base::ReadFileToString(filepath, &proto_str));

    KeyDataProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  // Adds a project to the disallowed projects list.
  void AddDisallowedProject(uint64_t project_name_hash) {
    recorder_->AddDisallowedProjectForTest(project_name_hash);
  }

  // Simulates the three external events that the structure metrics system cares
  // about: the metrics service initializing and enabling its providers, and a
  // user logging in.
  void Init() {
    // Create a system profile, normally done by ChromeMetricsServiceClient.
    system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
    // Create the provider, normally done by the ChromeMetricsServiceClient.
    recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
        DeviceKeyFilePath(), system_profile_provider_.get());
    // Enable recording, normally done after the metrics service has checked
    // consent allows recording.
    recorder_->EnableRecording();
    // Add a profile, normally done by the ChromeMetricsServiceClient after a
    // user logs in.
    recorder_->OnProfileAdded(TempDirPath());
    Wait();
  }

  // Enables recording without adding a profile.
  void InitWithoutLogin() {
    // Create a system profile, normally done by ChromeMetricsServiceClient.
    system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
    // Create the provider, normally done by the ChromeMetricsServiceClient.
    recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
        DeviceKeyFilePath(), system_profile_provider_.get());
    // Enable recording, normally done after the metrics service has checked
    // consent allows recording.
    recorder_->EnableRecording();
  }

  bool is_initialized() {
    return recorder_->init_state_ ==
           StructuredMetricsRecorder::InitState::kInitialized;
  }

  bool is_recording_enabled() { return recorder_->recording_enabled_; }

  void OnRecordingEnabled() { recorder_->EnableRecording(); }

  void OnRecordingDisabled() { recorder_->DisableRecording(); }

  void OnReportingStateChanged(bool enabled) {
    recorder_->OnReportingStateChanged(enabled);
  }

  void OnProfileAdded(const base::FilePath& path) {
    recorder_->OnProfileAdded(path);
  }

  void WriteNow() {
    recorder_->WriteNowForTest();
    Wait();
  }

  StructuredDataProto GetUMAEventMetrics() {
    ChromeUserMetricsExtension uma_proto;
    recorder_->ProvideUmaEventMetrics(uma_proto);
    Wait();
    return uma_proto.structured_data();
  }

  StructuredDataProto GetEventMetrics() {
    ChromeUserMetricsExtension uma_proto;
    recorder_->ProvideEventMetrics(uma_proto);
    Wait();
    return uma_proto.structured_data();
  }

  ChromeUserMetricsExtension GetUmaProto() {
    ChromeUserMetricsExtension uma_proto;
    recorder_->ProvideEventMetrics(uma_proto);
    Wait();
    return uma_proto;
  }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  void SetExternalMetricsDirForTest(const base::FilePath dir) {
    recorder_->SetExternalMetricsDirForTest(dir);
  }

  void InitializeSystemProfile() { recorder_->OnSystemProfileInitialized(); }

 protected:
  std::unique_ptr<MetricsProvider> system_profile_provider_;
  std::unique_ptr<TestStructuredMetricsRecorder> recorder_;
  // Feature list should be constructed before task environment.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;

 private:
  TestRecorder test_recorder_;
};

// Simple test to ensure initialization works correctly in the case of a
// first-time run.
TEST_F(StructuredMetricsRecorderTest, RecorderInitializesFromBlankSlate) {
  Init();
  EXPECT_TRUE(is_initialized());
  EXPECT_TRUE(is_recording_enabled());
  ExpectNoErrors();
}

// Ensure a call to OnRecordingDisabled prevents reporting.
TEST_F(StructuredMetricsRecorderTest, EventsNotReportedWhenRecordingDisabled) {
  Init();
  OnRecordingDisabled();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  ExpectNoErrors();
}

// Ensure that disabling the structured metrics feature flag prevents all
// structured metrics reporting.
TEST_F(StructuredMetricsRecorderTest, EventsNotReportedWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kStructuredMetrics);

  Init();
  // OnRecordingEnabled should not actually enable recording because the flag is
  // disabled.
  OnRecordingEnabled();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  ExpectNoErrors();
}

// Ensure that keys and unsent logs are deleted when reporting is disabled, and
// that reporting resumes when re-enabled.
TEST_F(StructuredMetricsRecorderTest, ReportingStateChangesHandledCorrectly) {
  Init();

  // Record an event and read the keys, there should be one.
  events::v2::test_project_one::TestEventOne().Record();
  EXPECT_EQ(GetEventMetrics().events_size(), 1);

  const KeyDataProto enabled_proto = ReadKeys(ProfileKeyFilePath());
  EXPECT_EQ(enabled_proto.keys_size(), 1);

  // Record an event, disable reporting, then record another event. Both of
  // these events should have been ignored.
  events::v2::test_project_one::TestEventOne().Record();
  OnReportingStateChanged(false);
  events::v2::test_project_one::TestEventOne().Record();
  EXPECT_EQ(GetEventMetrics().events_size(), 0);

  // Read the keys again, it should be empty.
  const KeyDataProto disabled_proto = ReadKeys(ProfileKeyFilePath());
  EXPECT_EQ(disabled_proto.keys_size(), 0);

  // Enable reporting again, and record an event.
  OnReportingStateChanged(true);
  OnRecordingEnabled();
  events::v2::test_project_one::TestEventOne().Record();
  EXPECT_EQ(GetEventMetrics().events_size(), 1);
  const KeyDataProto reenabled_proto = ReadKeys(ProfileKeyFilePath());
  EXPECT_EQ(reenabled_proto.keys_size(), 1);

  ExpectNoErrors();
}

// Ensure that, if recording is disabled part-way through initialization, the
// initialization still completes correctly, but recording is correctly set to
// disabled.
TEST_F(StructuredMetricsRecorderTest, RecordingDisabledDuringInitialization) {
  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());

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
TEST_F(StructuredMetricsRecorderTest, RecordingDisabledByDefault) {
  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());

  OnProfileAdded(TempDirPath());
  Wait();
  EXPECT_TRUE(is_initialized());
  EXPECT_FALSE(is_recording_enabled());

  OnRecordingEnabled();
  EXPECT_TRUE(is_recording_enabled());

  ExpectNoErrors();
}

TEST_F(StructuredMetricsRecorderTest, RecordedEventAppearsInReport) {
  Init();

  events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();
  events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();
  events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("a string")
      .SetTestMetricTwo(12345)
      .Record();

  EXPECT_EQ(GetEventMetrics().events_size(), 3);
  // TestProjectOne is not UMA ID'd, so GetUMAEventMetrics should be empty.
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  ExpectNoErrors();
}

TEST_F(StructuredMetricsRecorderTest, UmaEventsReportedCorrectly) {
  WriteTestingProfileKeys();
  Init();

  events::v2::test_project_three::TestEventFour()
      .SetTestMetricFour(12345)
      .Record();
  events::v2::test_project_three::TestEventFour()
      .SetTestMetricFour(67890)
      .Record();

  const auto data = GetUMAEventMetrics();
  ASSERT_EQ(data.events_size(), 2);

  {  // First event
    const auto& event = data.events(0);
    EXPECT_EQ(event.event_name_hash(), kEventFourHash);
    EXPECT_EQ(event.project_name_hash(), kProjectThreeHash);
    // TODO(crbug.com/1148168): The UMA ID currently isn't attached to UMA
    // events, so just check it isn't set.
    EXPECT_FALSE(event.has_profile_event_id());
    ASSERT_EQ(event.metrics_size(), 1);
    const auto& metric = event.metrics(0);
    EXPECT_EQ(metric.name_hash(), kMetricFourHash);
    EXPECT_EQ(metric.value_int64(), 12345);
  }

  {  // Second event
    const auto& event = data.events(1);
    EXPECT_EQ(event.event_name_hash(), kEventFourHash);
    EXPECT_EQ(event.project_name_hash(), kProjectThreeHash);
    // TODO(crbug.com/1148168): The UMA ID currently isn't attached to UMA
    // events, so just check it isn't set.
    EXPECT_FALSE(event.has_profile_event_id());
    ASSERT_EQ(event.metrics_size(), 1);
    const auto& metric = event.metrics(0);
    EXPECT_EQ(metric.name_hash(), kMetricFourHash);
    EXPECT_EQ(metric.value_int64(), 67890);
  }

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

TEST_F(StructuredMetricsRecorderTest, EventMetricsReportedCorrectly) {
  WriteTestingProfileKeys();
  Init();

  events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne(kValueOne)
      .SetTestMetricTwo(12345)
      .Record();
  events::v2::test_project_two::TestEventTwo()
      .SetTestMetricThree(kValueTwo)
      .Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 2);

  {  // First event
    const auto& event = data.events(0);
    EXPECT_EQ(event.event_name_hash(), kEventOneHash);
    EXPECT_EQ(event.project_name_hash(), kProjectOneHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectOneId);
    ASSERT_EQ(event.metrics_size(), 2);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricOneHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("aaa...a", concat(hex(kMetricOneHash),
                // kValueOne))
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
    EXPECT_EQ(event.project_name_hash(), kProjectTwoHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectTwoId);
    ASSERT_EQ(event.metrics_size(), 1);

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricThreeHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("bbb...b", concat(hex(kProjectTwoHash),
                // kValueTwo))
                "86F0169868588DC7");
    }
  }

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

TEST_F(StructuredMetricsRecorderTest, EventMetricsProvideSystemProfile) {
  WriteTestingProfileKeys();
  Init();
  InitializeSystemProfile();

  events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne(kValueOne)
      .SetTestMetricTwo(12345)
      .Record();
  events::v2::test_project_two::TestEventTwo()
      .SetTestMetricThree(kValueTwo)
      .Record();

  const auto uma_proto = GetUmaProto();
  CHECK(uma_proto.has_system_profile());

  {
    const auto structured_profile = uma_proto.system_profile();
    EXPECT_EQ(structured_profile.multi_profile_user_count(), kUserCount);
    EXPECT_EQ(structured_profile.hardware().full_hardware_class(), kHwid);
  }

  const auto data = uma_proto.structured_data();
  ASSERT_EQ(data.events_size(), 2);

  {  // First event
    const auto& event = data.events(0);
    EXPECT_EQ(event.event_name_hash(), kEventOneHash);
    EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectOneId);
    ASSERT_EQ(event.metrics_size(), 2);
    {}

    {  // First metric
      const auto& metric = event.metrics(0);
      EXPECT_EQ(metric.name_hash(), kMetricOneHash);
      EXPECT_EQ(HashToHex(metric.value_hmac()),
                // Value of HMAC_256("aaa...a", concat(hex(kMetricOneHash),
                // kValueOne))
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
                // Value of HMAC_256("bbb...b", concat(hex(kProjectTwoHash),
                // kValueTwo))
                "86F0169868588DC7");
    }
  }

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

// Ensure that events containing raw string metrics are reported correctly.
TEST_F(StructuredMetricsRecorderTest, RawStringMetricsReportedCorrectly) {
  Init();

  const std::string test_string = "a raw string value";
  events::v2::test_project_five::TestEventSix()
      .SetTestMetricSix(test_string)
      .Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);

  const auto& event = data.events(0);
  EXPECT_EQ(event.event_name_hash(), kEventSixHash);
  EXPECT_EQ(event.project_name_hash(), kProjectFiveHash);
  EXPECT_FALSE(event.has_profile_event_id());
  EXPECT_EQ(event.event_type(), StructuredEventProto_EventType_RAW_STRING);

  ASSERT_EQ(event.metrics_size(), 1);
  const auto& metric = event.metrics(0);

  EXPECT_EQ(metric.name_hash(), kMetricSixHash);
  EXPECT_EQ(metric.value_string(), test_string);
}

TEST_F(StructuredMetricsRecorderTest, FloatMetricsReportedCorrectly) {
  Init();

  const float test_float = 3.4;
  const float test_float2 = 3.14e-8;

  events::v2::test_project_six::TestEventSeven()
      .SetTestMetricSeven(test_float)
      .Record();

  events::v2::test_project_six::TestEventSeven()
      .SetTestMetricSeven(test_float2)
      .Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 2);

  const auto& event = data.events(0);
  EXPECT_EQ(event.event_name_hash(), kEventSevenHash);
  EXPECT_EQ(event.project_name_hash(), kProjectSixHash);
  EXPECT_FALSE(event.has_profile_event_id());

  ASSERT_EQ(event.metrics_size(), 1);
  const auto& metric = event.metrics(0);

  EXPECT_EQ(metric.name_hash(), kMetricSevenHash);
  EXPECT_EQ(metric.value_double(), test_float);

  const auto& event2 = data.events(1);
  EXPECT_EQ(event2.event_name_hash(), kEventSevenHash);
  EXPECT_EQ(event2.project_name_hash(), kProjectSixHash);
  EXPECT_FALSE(event2.has_profile_event_id());

  ASSERT_EQ(event2.metrics_size(), 1);
  const auto& metric2 = event2.metrics(0);

  EXPECT_EQ(metric2.name_hash(), kMetricSevenHash);
  EXPECT_EQ(metric2.value_double(), test_float2);
}

TEST_F(StructuredMetricsRecorderTest, DeviceKeysUsedForDeviceScopedProjects) {
  WriteTestingProfileKeys();
  WriteTestingDeviceKeys();
  Init();

  // This event's project has device scope set, so should use the per-device
  // keys set by WriteTestingDeviceKeys. In this case the expected key is
  // "ddd...d", which we observe by checking the ID and HMAC have the correct
  // value given that key.
  events::v2::test_project_four::TestEventFive()
      .SetTestMetricFive("value")
      .Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);

  const auto& event = data.events(0);
  EXPECT_EQ(event.event_name_hash(), kEventFiveHash);
  EXPECT_EQ(event.project_name_hash(), kProjectFourHash);
  // The hex-encoded first 8 bytes of SHA256("ddd...d").
  EXPECT_EQ(HashToHex(event.profile_event_id()), kProjectFourId);
  ASSERT_EQ(event.metrics_size(), 1);

  const auto& metric = event.metrics(0);
  EXPECT_EQ(metric.name_hash(), kMetricFiveHash);
  EXPECT_EQ(HashToHex(metric.value_hmac()),
            // Value of HMAC_256("ddd...d", concat(hex(kMetricFiveHash),
            // "value"))
            "4CC202FAA78FDC7A");

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

// Check that a full int64 can be recorded, and is not truncated to an int32.
TEST_F(StructuredMetricsRecorderTest, Int64MetricsNotTruncated) {
  Init();
  const int64_t big = 1ll << 60;
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(big).Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);
  const auto& event = data.events(0);
  ASSERT_EQ(event.metrics_size(), 1);
  const auto& metric = event.metrics(0);
  EXPECT_EQ(metric.value_int64(), big);
}

TEST_F(StructuredMetricsRecorderTest, EventsWithinProjectReportedWithSameID) {
  WriteTestingProfileKeys();
  Init();

  events::v2::test_project_one::TestEventOne().Record();
  events::v2::test_project_two::TestEventTwo().Record();
  events::v2::test_project_two::TestEventThree().Record();

  const auto data = GetEventMetrics();
  // TestProjectOne is not UMA ID'd, so GetUMAEventMetrics should be empty.
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  ASSERT_EQ(data.events_size(), 3);

  const auto& event_one = data.events(0);
  const auto& event_two = data.events(1);
  const auto& event_three = data.events(2);

  // Check events are in the right order.
  EXPECT_EQ(event_one.event_name_hash(), kEventOneHash);
  EXPECT_EQ(event_two.event_name_hash(), kEventTwoHash);
  EXPECT_EQ(event_three.event_name_hash(), kEventThreeHash);

  // Events two and three share a project, so should have the same project
  // name hash. Event one should have its own project name hash.
  EXPECT_EQ(event_one.project_name_hash(), kProjectOneHash);
  EXPECT_EQ(event_two.project_name_hash(), kProjectTwoHash);
  EXPECT_EQ(event_three.project_name_hash(), kProjectTwoHash);

  // Events two and three share a project, so should have the same ID. Event
  // one should have its own ID.
  EXPECT_EQ(HashToHex(event_one.profile_event_id()), kProjectOneId);
  EXPECT_EQ(HashToHex(event_two.profile_event_id()), kProjectTwoId);
  EXPECT_EQ(HashToHex(event_three.profile_event_id()), kProjectTwoId);

  histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError", 0);
}

TEST_F(StructuredMetricsRecorderTest, EventWithoutMetricsReportCorrectly) {
  Init();

  const int test_time = 50;

  events::v2::cr_os_events::NoMetricsEvent test_event;
  EXPECT_TRUE(test_event.IsEventSequenceType());
  test_event.SetEventSequenceMetadata(Event::EventSequenceMetadata(1));
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(test_time));
  test_event.Record();

  const auto data = GetEventMetrics();

  EXPECT_EQ(data.events_size(), 1);

  const auto& event = data.events(0);

  EXPECT_EQ(event.project_name_hash(), kCrOSEventsProjectHash);
  EXPECT_EQ(event.event_name_hash(), kNoMetricsEventHash);
}

// Test that a call to ProvideCurrentSessionData clears the provided events from
// the cache, and a subsequent call does not return those events again.
TEST_F(StructuredMetricsRecorderTest, EventsClearedAfterReport) {
  Init();

  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(2).Record();
  // TestProjectOne is not UMA ID'd, so GetUMAEventMetrics should be empty.
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  // Should provide both the previous events.
  EXPECT_EQ(GetEventMetrics().events_size(), 2);

  // But the previous events shouldn't appear in the second report.
  EXPECT_EQ(GetEventMetrics().events_size(), 0);

  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(3).Record();
  // The third request should only contain the third event.
  EXPECT_EQ(GetEventMetrics().events_size(), 1);

  ExpectNoErrors();
}

// Test that events recorded in one session are correctly persisted and are
// uploaded in the first report from a subsequent session.
TEST_F(StructuredMetricsRecorderTest, EventsFromPreviousSessionAreReported) {
  // Start first session and record one event.
  Init();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1234).Record();

  // Write events to disk, then destroy the provider.
  WriteNow();
  recorder_.reset();

  // Start a second session and ensure the event is reported.
  Init();
  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);
  ASSERT_EQ(data.events(0).metrics_size(), 1);
  EXPECT_EQ(data.events(0).metrics(0).value_int64(), 1234);
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);

  ExpectNoErrors();
}

TEST_F(StructuredMetricsRecorderTest, ExternalMetricsAreReported) {
  const base::FilePath events_dir(TempDirPath().Append("events"));
  base::CreateDirectory(events_dir);

  const auto proto = MakeExternalEventProto({111, 222, 333});
  ASSERT_TRUE(
      base::WriteFile(events_dir.Append("event"), proto.SerializeAsString()));

  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());
  OnProfileAdded(TempDirPath());
  SetExternalMetricsDirForTest(events_dir);
  OnRecordingEnabled();
  task_environment_.AdvanceClock(base::Hours(10));
  Wait();
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 3);
}

TEST_F(StructuredMetricsRecorderTest,
       ExternalMetricsDroppedWhenRecordingDisabled) {
  const base::FilePath events_dir(TempDirPath().Append("events"));
  base::CreateDirectory(events_dir);

  const auto proto = MakeExternalEventProto({111, 222, 333});
  ASSERT_TRUE(
      base::WriteFile(events_dir.Append("event"), proto.SerializeAsString()));

  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());
  OnProfileAdded(TempDirPath());
  SetExternalMetricsDirForTest(events_dir);
  OnRecordingDisabled();
  task_environment_.AdvanceClock(base::Hours(10));
  Wait();
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
}

// Test that events reported before recording is enabled are ignored.
TEST_F(StructuredMetricsRecorderTest, EventsNotRecordedBeforeRecordingEnabled) {
  // Manually create and initialize the provider, adding recording calls between
  // each step. All of these events should be ignored.
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  OnRecordingEnabled();
  Wait();

  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 0);

  ExpectNoErrors();
}

// Test that events reported after recording is enabled but before the keys are
// loaded are hashed and stored after keys are loaded.
TEST_F(StructuredMetricsRecorderTest, EventsRecordedBeforeKeysInitialized) {
  InitWithoutLogin();
  // Emulate metric before login.
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();

  OnProfileAdded(TempDirPath());
  // Called before user key is loaded.
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  Wait();

  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 2);

  ExpectNoErrors();
}

// Ensure a call to OnRecordingDisabled not only prevents the reporting of new
// events, but also clears the cache of any existing events that haven't yet
// been reported.
TEST_F(StructuredMetricsRecorderTest,
       ExistingEventsClearedWhenRecordingDisabled) {
  Init();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();
  OnRecordingDisabled();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 0);

  ExpectNoErrors();
}

// Ensure that recording and reporting is re-enabled after recording is disabled
// and then enabled again.
TEST_F(StructuredMetricsRecorderTest, ReportingResumesWhenEnabled) {
  Init();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();

  OnRecordingDisabled();
  OnRecordingEnabled();

  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_three::TestEventFour().SetTestMetricFour(1).Record();

  EXPECT_EQ(GetUMAEventMetrics().events_size(), 2);
  EXPECT_EQ(GetEventMetrics().events_size(), 4);

  ExpectNoErrors();
}

// Ensure that a call to ProvideCurrentSessionData before initialization
// completes returns no events.
TEST_F(StructuredMetricsRecorderTest,
       ReportsNothingBeforeInitializationComplete) {
  system_profile_provider_ = std::make_unique<TestSystemProfileProvider>();
  recorder_ = std::make_unique<TestStructuredMetricsRecorder>(
      system_profile_provider_.get());
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
  OnRecordingEnabled();
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
  OnProfileAdded(TempDirPath());
  EXPECT_EQ(GetUMAEventMetrics().events_size(), 0);
  EXPECT_EQ(GetEventMetrics().events_size(), 0);
}

// Check that LastKeyRotation returns a value in the correct range of possible
// last rotations for a newly generated key.
TEST_F(StructuredMetricsRecorderTest, LastKeyRotation) {
  Init();

  events::v2::test_project_one::TestEventOne event;

  // Record a metric so that the key is created.
  event.Record();

  const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  const absl::optional<int> last_rotation =
      Recorder::GetInstance()->LastKeyRotation(event);

  // The last rotation should be a random day between today and 90 days in the
  // past, ie. the rotation period for this project.
  ASSERT_TRUE(last_rotation.has_value());
  EXPECT_GE(last_rotation, today - 90);
}

// Ensures that events part of event sequence are recorded properly.
TEST_F(StructuredMetricsRecorderTest, EventSequenceLogging) {
  Init();

  scoped_feature_list_.InitAndEnableFeature(
      metrics::structured::kEventSequenceLogging);

  const int test_time = 50;
  const double test_metric = 1.0;

  events::v2::cr_os_events::Test1 test_event;
  EXPECT_TRUE(test_event.IsEventSequenceType());
  test_event.SetEventSequenceMetadata(Event::EventSequenceMetadata(1));
  test_event.SetRecordedTimeSinceBoot(base::Milliseconds(test_time));
  test_event.SetMetric1(test_metric).Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);

  const auto& event = data.events(0);
  EXPECT_EQ(event.project_name_hash(), kCrOSEventsProjectHash);

  // Sequence events should have both a device and user project id.
  EXPECT_TRUE(event.has_device_project_id());
  EXPECT_TRUE(event.has_user_project_id());

  // Verify that event sequence metadata has been serialized correctly.
  const auto& event_metadata = event.event_sequence_metadata();
  EXPECT_EQ(event_metadata.reset_counter(), 1);
  EXPECT_TRUE(event_metadata.has_event_unique_id());
  EXPECT_EQ(event_metadata.system_uptime(), test_time);

  ASSERT_EQ(event.metrics_size(), 1);
  const auto& metric = event.metrics(0);
  EXPECT_EQ(metric.value_double(), 1.0);

  ExpectNoErrors();
}

TEST_F(StructuredMetricsRecorderTest, EventsClone) {
  Init();

  events::v2::cr_os_events::Test1 event;

  const int test_time = 50;
  const double test_metric = 1.0;

  event.SetEventSequenceMetadata(Event::EventSequenceMetadata(1));
  event.SetRecordedTimeSinceBoot(base::Milliseconds(test_time));
  event.SetMetric1(test_metric);

  auto cloned_event = event.Clone();

  EXPECT_EQ(event.event_sequence_metadata().reset_counter,
            cloned_event.event_sequence_metadata().reset_counter);
  EXPECT_EQ(event.project_name(), cloned_event.project_name());
  EXPECT_EQ(event.event_name(), cloned_event.event_name());
  EXPECT_EQ(event.is_event_sequence(), cloned_event.is_event_sequence());
  EXPECT_EQ(event.recorded_time_since_boot(),
            cloned_event.recorded_time_since_boot());
  EXPECT_EQ(event.metric_values(), cloned_event.metric_values());
}

TEST_F(StructuredMetricsRecorderTest, DisallowedProjectAreDropped) {
  Init();

  AddDisallowedProject(kProjectOneHash);

  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_one::TestEventOne().SetTestMetricTwo(1).Record();
  events::v2::test_project_two::TestEventThree()
      .SetTestMetricFour("value")
      .Record();

  const auto data = GetEventMetrics();
  ASSERT_EQ(data.events_size(), 1);
  ASSERT_EQ(data.events(0).project_name_hash(), kProjectTwoHash);
}

}  // namespace metrics::structured
