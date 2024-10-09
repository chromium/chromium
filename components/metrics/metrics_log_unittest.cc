// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/sample_vector.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_metrics_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/active_field_trials.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/current_module.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#include "base/scoped_environment_variable_override.h"
#endif

namespace metrics {
namespace {

const char kClientId[] = "0a94430b-18e5-43c8-a657-580f7e855ce1";
const int kSessionId = 127;

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 LogType log_type,
                 MetricsServiceClient* client)
      : MetricsLog(client_id, session_id, log_type, client) {}

  TestMetricsLog(const TestMetricsLog&) = delete;
  TestMetricsLog& operator=(const TestMetricsLog&) = delete;

  ~TestMetricsLog() override = default;

  const ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  ChromeUserMetricsExtension* mutable_uma_proto() {
    return MetricsLog::uma_proto();
  }

  const SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }
};

// Returns the expected hardware class for a metrics log.
std::string GetExpectedHardwareClass() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Currently, we are relying on base/ implementation for functionality on our
  // side which can be fragile if in the future someone decides to change that.
  // This replicates the logic to get the hardware class for ChromeOS and this
  // result should match with the result by calling
  // base::SysInfo::HardwareModelName().
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  if (board == "unknown") {
    return "";
  }
  const size_t index = board.find("-signed-");
  if (index != std::string::npos)
    board.resize(index);
  return base::ToUpperASCII(board);
#else
  return base::SysInfo::HardwareModelName();
#endif
}

// Sets the time in |network_time| to |time|.
void UpdateNetworkTime(network_time::NetworkTimeTracker* network_time_tracker,
                       base::TickClock* tick_clock,
                       base::Time time) {
  network_time_tracker->UpdateNetworkTime(
      time,
      base::Seconds(1),         // resolution
      base::Milliseconds(250),  // latency
      tick_clock->NowTicks());  // posting time
}

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() { MetricsLog::RegisterPrefs(prefs_.registry()); }

  MetricsLogTest(const MetricsLogTest&) = delete;
  MetricsLogTest& operator=(const MetricsLogTest&) = delete;

  ~MetricsLogTest() override = default;

 protected:
  // Check that the values in |system_values| are filled in and expected ones
  // correspond to the test data defined at the top of this file.
  void CheckSystemProfile(const SystemProfileProto& system_profile) {
    // Check for presence of core system profile fields.
    EXPECT_TRUE(system_profile.has_build_timestamp());
    EXPECT_TRUE(system_profile.has_app_version());
    EXPECT_TRUE(system_profile.has_channel());
    EXPECT_FALSE(system_profile.has_is_extended_stable_channel());
    EXPECT_TRUE(system_profile.has_application_locale());
    EXPECT_TRUE(system_profile.has_client_uuid());

    const SystemProfileProto::OS& os = system_profile.os();
    EXPECT_TRUE(os.has_name());
    EXPECT_TRUE(os.has_version());

    // Check matching test brand code.
    EXPECT_EQ(TestMetricsServiceClient::kBrandForTesting,
              system_profile.brand_code());

    // Check for presence of fields set by a metrics provider.
    const SystemProfileProto::Hardware& hardware = system_profile.hardware();
    EXPECT_EQ(hardware.hardware_class(), GetExpectedHardwareClass());
    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());
    EXPECT_TRUE(hardware.cpu().has_num_cores());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(MetricsLogTest, FinalizedRecordId) {
  MetricsLog log1(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  MetricsLog log2(kClientId, kSessionId, MetricsLog::INDEPENDENT_LOG, &client_);
  MetricsLog log3(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                  &client_);

  ASSERT_FALSE(log1.uma_proto()->has_finalized_record_id());
  ASSERT_FALSE(log2.uma_proto()->has_finalized_record_id());
  ASSERT_FALSE(log3.uma_proto()->has_finalized_record_id());

  // Set an initial finalized record-id value in prefs, so test values are
  // predictable.
  prefs_.SetInteger(prefs::kMetricsLogFinalizedRecordId, 500);

  log1.AssignFinalizedRecordId(&prefs_);
  log2.AssignFinalizedRecordId(&prefs_);
  log3.AssignFinalizedRecordId(&prefs_);

  EXPECT_EQ(501, log1.uma_proto()->finalized_record_id());
  EXPECT_EQ(502, log2.uma_proto()->finalized_record_id());
  EXPECT_EQ(503, log3.uma_proto()->finalized_record_id());
}

TEST_F(MetricsLogTest, RecordId) {
  MetricsLog log1(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  MetricsLog log2(kClientId, kSessionId, MetricsLog::INDEPENDENT_LOG, &client_);
  MetricsLog log3(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                  &client_);

  ASSERT_FALSE(log1.uma_proto()->has_record_id());
  ASSERT_FALSE(log2.uma_proto()->has_record_id());
  ASSERT_FALSE(log3.uma_proto()->has_record_id());

  // Set an initial record-id value in prefs, so test values are predictable.
  prefs_.SetInteger(prefs::kMetricsLogRecordId, 500);

  log1.AssignRecordId(&prefs_);
  log2.AssignRecordId(&prefs_);
  log3.AssignRecordId(&prefs_);

  EXPECT_EQ(501, log1.uma_proto()->record_id());
  EXPECT_EQ(502, log2.uma_proto()->record_id());
  EXPECT_EQ(503, log3.uma_proto()->record_id());
}

TEST_F(MetricsLogTest, SessionHash) {
  MetricsLog log1(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  MetricsLog log2(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);

  // Verify that created logs are tagged with the same session hash.
  EXPECT_TRUE(log1.uma_proto()->system_profile().has_session_hash());
  EXPECT_TRUE(log2.uma_proto()->system_profile().has_session_hash());
  EXPECT_EQ(log1.uma_proto()->system_profile().session_hash(),
            log2.uma_proto()->system_profile().session_hash());
}

TEST_F(MetricsLogTest, LogType) {
  MetricsLog log1(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  EXPECT_EQ(MetricsLog::ONGOING_LOG, log1.log_type());

  MetricsLog log2(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                  &client_);
  EXPECT_EQ(MetricsLog::INITIAL_STABILITY_LOG, log2.log_type());
}

TEST_F(MetricsLogTest, BasicRecord) {
  client_.set_version_string("bogus version");
  const std::string kOtherClientId = "0a94430b-18e5-43c8-a657-580f7e855ce2";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Clears existing command line flags and sets mock flags:
  // "--mock-flag-1 --mock-flag-2=unused_value"
  // Hashes of these flags should be populated on the system_profile field.
  command_line->InitFromArgv(0, nullptr);
  command_line->AppendSwitch("mock-flag-1");
  command_line->AppendSwitchASCII("mock-flag-2", "unused_value");

#if BUILDFLAG(IS_LINUX)
  base::ScopedEnvironmentVariableOverride scoped_desktop_override(
      base::nix::kXdgCurrentDesktopEnvVar, "GNOME");
  metrics::SystemProfileProto::OS::XdgCurrentDesktop expected_current_desktop =
      metrics::SystemProfileProto::OS::GNOME;

  base::ScopedEnvironmentVariableOverride scoped_session_override(
      base::nix::kXdgSessionTypeEnvVar, "wayland");
  metrics::SystemProfileProto::OS::XdgSessionType expected_session_type =
      metrics::SystemProfileProto::OS::WAYLAND;
#endif

  MetricsLog log(kOtherClientId, 137, MetricsLog::ONGOING_LOG, &client_);

  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);

  // A couple of fields are hard to mock, so these will be copied over directly
  // for the expected output.
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));

  ChromeUserMetricsExtension expected;
  expected.set_client_id(13917849739535108017ull);  // Hashed kOtherClientId
  expected.set_session_id(137);

  SystemProfileProto* system_profile = expected.mutable_system_profile();
  system_profile->set_app_version("bogus version");
  // Make sure |client_uuid| in the system profile is the unhashed client id
  // and is the same as the client id in |local_prefs|.
  system_profile->set_client_uuid(kOtherClientId);
  system_profile->set_channel(client_.GetChannel());
  system_profile->set_application_locale(client_.GetApplicationLocale());
  system_profile->set_brand_code(TestMetricsServiceClient::kBrandForTesting);
  // Hashes of "mock-flag-1" and "mock-flag-2" from SetUpCommandLine.
  system_profile->add_command_line_key_hash(2578836236);
  system_profile->add_command_line_key_hash(2867288449);
  // The session hash.
  system_profile->set_session_hash(
      log.uma_proto()->system_profile().session_hash());

#if defined(ADDRESS_SANITIZER) || DCHECK_IS_ON()
  system_profile->set_is_instrumented_build(true);
#endif
  metrics::SystemProfileProto::Hardware* hardware =
      system_profile->mutable_hardware();
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  auto app_os_arch = base::SysInfo::ProcessCPUArchitecture();
  if (!app_os_arch.empty())
    hardware->set_app_cpu_architecture(app_os_arch);
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
  hardware->set_hardware_class(GetExpectedHardwareClass());
#if BUILDFLAG(IS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64_t>(CURRENT_MODULE()));
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  system_profile->mutable_os()->set_name("Lacros");
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  system_profile->mutable_os()->set_name("CrOS");
#else
  system_profile->mutable_os()->set_name(base::SysInfo::OperatingSystemName());
#endif
  system_profile->mutable_os()->set_version(
      base::SysInfo::OperatingSystemVersion());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  system_profile->mutable_os()->set_kernel_version(
      base::SysInfo::KernelVersion());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  system_profile->mutable_os()->set_kernel_version(
      base::SysInfo::OperatingSystemVersion());
#elif BUILDFLAG(IS_ANDROID)
  system_profile->mutable_os()->set_build_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
  system_profile->set_app_package_name("test app");
#elif BUILDFLAG(IS_IOS)
  system_profile->mutable_os()->set_build_number(
      base::SysInfo::GetIOSBuildNumber());
#endif

#if BUILDFLAG(IS_LINUX)
  system_profile->mutable_os()->set_xdg_session_type(expected_session_type);
  system_profile->mutable_os()->set_xdg_current_desktop(
      expected_current_desktop);
#endif

  // Hard to mock.
  system_profile->set_build_timestamp(
      parsed.system_profile().build_timestamp());
#if BUILDFLAG(IS_ANDROID)
  system_profile->set_installer_package(
      parsed.system_profile().installer_package());
#endif

  // Not tested here; instead tested in Timestamps_* tests below.
  expected.mutable_time_log_created()->CopyFrom(parsed.time_log_created());
  expected.mutable_time_log_closed()->CopyFrom(parsed.time_log_closed());

  EXPECT_EQ(expected.SerializeAsString(), encoded);
}

TEST_F(MetricsLogTest, FinalizeLog) {
  static const char kVersionString[] = "1";
  static const char kNewVersionString[] = "2";
  client_.set_version_string(kVersionString);

  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  TestMetricsLog log2(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);

  // Fill logs with user actions and omnibox events. We put more than the limit
  // to verify that when calling FinalizeLog(), we may optionally truncate
  // those events.
  const int kUserActionCount = internal::kUserActionEventLimit * 2;
  for (int i = 0; i < kUserActionCount; ++i) {
    log.RecordUserAction("BasicAction", base::TimeTicks::Now());
    log2.RecordUserAction("BasicAction", base::TimeTicks::Now());
  }
  const int kOmniboxEventCount = internal::kOmniboxEventLimit * 2;
  for (int i = 0; i < kOmniboxEventCount; ++i) {
    // Add an empty omnibox event. Not fully realistic since these are normally
    // supplied by a metrics provider.
    log.mutable_uma_proto()->add_omnibox_event();
    log2.mutable_uma_proto()->add_omnibox_event();
  }

  // Finalize |log|. We truncate events, and we pass the same version string as
  // the one that was used when the log was created.
  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/true, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);

  // Finalize |log2|. We do not truncate events, and we pass a different version
  // string than the one that was used when the log was created.
  client_.set_version_string(kNewVersionString);
  std::string encoded2;
  log2.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                   log.GetCurrentClockTime(/*record_time_zone=*/true),
                   &encoded2);

  ChromeUserMetricsExtension parsed;
  parsed.ParseFromString(encoded);
  ChromeUserMetricsExtension parsed2;
  parsed2.ParseFromString(encoded2);

  // The user actions and omnibox events in |parsed| should have been truncated
  // to the limits, while |parsed2| should be untouched.
  EXPECT_EQ(parsed.user_action_event_size(), internal::kUserActionEventLimit);
  EXPECT_EQ(parsed.omnibox_event_size(), internal::kOmniboxEventLimit);
  EXPECT_EQ(parsed2.user_action_event_size(), kUserActionCount);
  EXPECT_EQ(parsed2.omnibox_event_size(), kOmniboxEventCount);

  // |kNewVersionString| (the version string when |log2| was closed) should
  // have been written to |parsed2| since it differs from |kVersionString|
  // (the version string when |log2| was created). |parsed| should not have it
  // since the version strings were the same.
  EXPECT_EQ(parsed2.system_profile().app_version(), kVersionString);
  EXPECT_EQ(parsed2.system_profile().log_written_by_app_version(),
            kNewVersionString);
  EXPECT_EQ(parsed.system_profile().app_version(), kVersionString);
  EXPECT_FALSE(parsed.system_profile().has_log_written_by_app_version());
}

TEST_F(MetricsLogTest, Timestamps_InitialStabilityLog) {
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();

  // Should not have times from initial stability logs.
  clock->SetNow(base::Time::FromTimeT(1));
  MetricsLog log(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                 clock.get(), nullptr, &client_);
  clock->SetNow(base::Time::FromTimeT(2));
  std::string encoded;
  // Don't set the close_time param since this is an initial stability log.
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  /*close_time=*/std::nullopt, &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_FALSE(parsed.has_time_log_created());
  EXPECT_FALSE(parsed.has_time_log_closed());
}

TEST_F(MetricsLogTest, Timestamps_IndependentLog) {
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();

  // Should not have times from independent logs.
  clock->SetNow(base::Time::FromTimeT(1));
  MetricsLog log(kClientId, kSessionId, MetricsLog::INDEPENDENT_LOG,
                 clock.get(), nullptr, &client_);
  clock->SetNow(base::Time::FromTimeT(2));
  std::string encoded;
  // Don't set the close_time param since this is an independent log.
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  /*close_time=*/std::nullopt, &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_FALSE(parsed.has_time_log_created());
  EXPECT_FALSE(parsed.has_time_log_closed());
}

TEST_F(MetricsLogTest, Timestamps_OngoingLog) {
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();

  // Should have times from regular (ongoing) logs.
  clock->SetNow(base::Time::FromTimeT(1));
  MetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, clock.get(),
                 nullptr, &client_);
  clock->SetNow(base::Time::FromTimeT(2));
  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_TRUE(parsed.has_time_log_created());
  EXPECT_EQ(parsed.time_log_created().time_sec(), 1);
  EXPECT_EQ(parsed.time_log_created().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  // The timezone should not be set in the time_log_created field.
  EXPECT_FALSE(parsed.time_log_created().has_time_zone_offset_from_gmt_sec());
  EXPECT_TRUE(parsed.has_time_log_closed());
  EXPECT_EQ(parsed.time_log_closed().time_sec(), 2);
  EXPECT_EQ(parsed.time_log_closed().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  // The timezone should be set, but we don't check what it is.
  EXPECT_TRUE(parsed.time_log_closed().has_time_zone_offset_from_gmt_sec());
}

TEST_F(MetricsLogTest,
       Timestamps_OngoingLogLog_WithNetworkClockExists_AlwaysUnavailable) {
  // Setup a network clock that doesn't provide a timestamp (time unavailable).
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test(
      new network_time::FieldTrialTest());
  field_trial_test->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  network_time::NetworkTimeTracker network_time_tracker(
      std::make_unique<base::DefaultClock>(),
      std::make_unique<base::DefaultTickClock>(), &pref_service,
      shared_url_loader_factory, /*fetch_behavior=*/std::nullopt);

  // Set up the backup client clock.
  TestMetricsServiceClient client;
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();

  // Should have times from regular (ongoing) logs.  These times should come
  // from the backup client clock, not the (unavailable) network clock.
  clock->SetNow(base::Time::FromTimeT(1));
  MetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, clock.get(),
                 &network_time_tracker, &client);
  clock->SetNow(base::Time::FromTimeT(2));

  // Check the output.
  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_TRUE(parsed.has_time_log_created());
  EXPECT_EQ(parsed.time_log_created().time_sec(), 1);
  EXPECT_EQ(parsed.time_log_created().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  // The timezone should not be set in the time_log_created field.
  EXPECT_FALSE(parsed.time_log_created().has_time_zone_offset_from_gmt_sec());
  EXPECT_TRUE(parsed.has_time_log_closed());
  EXPECT_EQ(parsed.time_log_closed().time_sec(), 2);
  EXPECT_EQ(parsed.time_log_closed().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  // The timezone should be set, but we don't check what it is.
  EXPECT_TRUE(parsed.time_log_closed().has_time_zone_offset_from_gmt_sec());
}

TEST_F(
    MetricsLogTest,
    Timestamps_OngoingLogLog_WithNetworkClockExists_UnavailableThenAvailable) {
  // Setup a network clock that initially doesn't provide a timestamp (time
  // unavailable).
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test(
      new network_time::FieldTrialTest());
  field_trial_test->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  base::SimpleTestClock* clock = new base::SimpleTestClock;
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
  // Do this to be sure that |is_null| returns false.
  clock->Advance(base::Days(111));
  tick_clock->Advance(base::Days(222));
  network_time::NetworkTimeTracker network_time_tracker(
      std::unique_ptr<base::Clock>(clock),
      std::unique_ptr<const base::TickClock>(tick_clock), &pref_service,
      shared_url_loader_factory, /*fetch_behavior=*/std::nullopt);

  // Should have times from regular (ongoing) logs.  The creation time should
  // come from the backup client clock; the closure time should come from the
  // network clock.
  clock->SetNow(base::Time::FromTimeT(1));
  TestMetricsServiceClient client;
  MetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, clock,
                 &network_time_tracker, &client);
  // Advance the backup client clock.  (Value should not be used; merely
  // advanced to make sure the new value doesn't show up anywhere.)
  clock->SetNow(base::Time::FromTimeT(2));
  // Set the network time tracker.
  UpdateNetworkTime(&network_time_tracker, tick_clock,
                    base::Time::FromTimeT(3));

  // Check the output.
  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_TRUE(parsed.has_time_log_created());
  EXPECT_EQ(parsed.time_log_created().time_sec(), 1);
  EXPECT_EQ(parsed.time_log_created().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  // The timezone should not be set in the time_log_created field.
  EXPECT_FALSE(parsed.time_log_created().has_time_zone_offset_from_gmt_sec());
  EXPECT_TRUE(parsed.has_time_log_closed());
  EXPECT_EQ(parsed.time_log_closed().time_sec(), 3);
  EXPECT_EQ(parsed.time_log_closed().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::NETWORK_TIME_CLOCK);
  // The timezone should be set, but we don't check what it is.
  EXPECT_TRUE(parsed.time_log_closed().has_time_zone_offset_from_gmt_sec());
}

TEST_F(MetricsLogTest,
       Timestamps_OngoingLogLog_WithNetworkClockExists_AlwaysAvailable) {
  // Setup a network clock that provides a timestamp.
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test(
      new network_time::FieldTrialTest());
  field_trial_test->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  base::SimpleTestClock* clock = new base::SimpleTestClock;
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
  // Do this to be sure that |is_null| returns false.
  clock->Advance(base::Days(111));
  tick_clock->Advance(base::Days(222));
  network_time::NetworkTimeTracker network_time_tracker(
      std::unique_ptr<base::Clock>(clock),
      std::unique_ptr<const base::TickClock>(tick_clock), &pref_service,
      shared_url_loader_factory, /*fetch_behavior=*/std::nullopt);

  // Should have times from regular (ongoing) logs.  These times should come
  // from the network clock.
  // Set the backup client clock time.  (Value should not be used; merely set
  // to make sure the value doesn't show up anywhere.)
  clock->SetNow(base::Time::FromTimeT(1));
  // Set the network time tracker.
  UpdateNetworkTime(&network_time_tracker, tick_clock,
                    base::Time::FromTimeT(2));
  TestMetricsServiceClient client;
  MetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, clock,
                 &network_time_tracker, &client);
  // Advance the backup client clock.  (Value should not be used; merely
  // advanced to make sure the new value doesn't show up anywhere.)
  clock->SetNow(base::Time::FromTimeT(3));
  // Advance and set the network time clock.
  UpdateNetworkTime(&network_time_tracker, tick_clock,
                    base::Time::FromTimeT(4));

  // Check the output.
  std::string encoded;
  log.FinalizeLog(/*truncate_events=*/false, client_.GetVersionString(),
                  log.GetCurrentClockTime(/*record_time_zone=*/true), &encoded);
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));
  EXPECT_TRUE(parsed.has_time_log_created());
  // Time should be the first time returned by the network time tracker.
  EXPECT_EQ(parsed.time_log_created().time_sec(), 2);
  EXPECT_EQ(parsed.time_log_created().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::NETWORK_TIME_CLOCK);
  // The timezone should not be set in the time_log_created field.
  EXPECT_FALSE(parsed.time_log_created().has_time_zone_offset_from_gmt_sec());
  EXPECT_TRUE(parsed.has_time_log_closed());
  // Time should be the second time returned by the network time tracker.
  EXPECT_EQ(parsed.time_log_closed().time_sec(), 4);
  EXPECT_EQ(parsed.time_log_closed().time_source(),
            ChromeUserMetricsExtension::RealLocalTime::NETWORK_TIME_CLOCK);
  // The timezone should be set, but we don't check what it is.
  EXPECT_TRUE(parsed.time_log_closed().has_time_zone_offset_from_gmt_sec());
}

TEST_F(MetricsLogTest, HistogramBucketFields) {
  // Create buckets: 1-5, 5-7, 7-8, 8-9, 9-10, 10-11, 11-12.
  base::BucketRanges ranges(8);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 7);
  ranges.set_range(3, 8);
  ranges.set_range(4, 9);
  ranges.set_range(5, 10);
  ranges.set_range(6, 11);
  ranges.set_range(7, 12);

  base::SampleVector samples(1, &ranges);
  samples.Accumulate(3, 1);   // Bucket 1-5.
  samples.Accumulate(6, 1);   // Bucket 5-7.
  samples.Accumulate(8, 1);   // Bucket 8-9. (7-8 skipped)
  samples.Accumulate(10, 1);  // Bucket 10-11. (9-10 skipped)
  samples.Accumulate(11, 1);  // Bucket 11-12.
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  log.RecordHistogramDelta("Test", samples);

  const ChromeUserMetricsExtension& uma_proto = log.uma_proto();
  const HistogramEventProto& histogram_proto =
      uma_proto.histogram_event(uma_proto.histogram_event_size() - 1);

  // Buckets with samples: 1-5, 5-7, 8-9, 10-11, 11-12.
  // Should become: 1-/, 5-7, /-9, 10-/, /-12.
  ASSERT_EQ(5, histogram_proto.bucket_size());

  // 1-5 becomes 1-/ (max is same as next min).
  EXPECT_TRUE(histogram_proto.bucket(0).has_min());
  EXPECT_FALSE(histogram_proto.bucket(0).has_max());
  EXPECT_EQ(1, histogram_proto.bucket(0).min());

  // 5-7 stays 5-7 (no optimization possible).
  EXPECT_TRUE(histogram_proto.bucket(1).has_min());
  EXPECT_TRUE(histogram_proto.bucket(1).has_max());
  EXPECT_EQ(5, histogram_proto.bucket(1).min());
  EXPECT_EQ(7, histogram_proto.bucket(1).max());

  // 8-9 becomes /-9 (min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(2).has_min());
  EXPECT_TRUE(histogram_proto.bucket(2).has_max());
  EXPECT_EQ(9, histogram_proto.bucket(2).max());

  // 10-11 becomes 10-/ (both optimizations apply, omit max is prioritized).
  EXPECT_TRUE(histogram_proto.bucket(3).has_min());
  EXPECT_FALSE(histogram_proto.bucket(3).has_max());
  EXPECT_EQ(10, histogram_proto.bucket(3).min());

  // 11-12 becomes /-12 (last record must keep max, min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(4).has_min());
  EXPECT_TRUE(histogram_proto.bucket(4).has_max());
  EXPECT_EQ(12, histogram_proto.bucket(4).max());
}

TEST_F(MetricsLogTest, HistogramSamplesCount) {
  const std::string histogram_name = "test";
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);

  // Create buckets: 1-5.
  base::BucketRanges ranges(2);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);

  // Add two samples.
  base::SampleVector samples(1, &ranges);
  samples.Accumulate(3, 2);
  log.RecordHistogramDelta(histogram_name, samples);

  EXPECT_EQ(2, log.log_metadata().samples_count.value());

  // Add two more samples.
  log.RecordHistogramDelta(histogram_name, samples);
  EXPECT_EQ(4, log.log_metadata().samples_count.value());
}

TEST_F(MetricsLogTest, RecordEnvironment) {
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);

  DelegatingProvider delegating_provider;
  auto cpu_provider = std::make_unique<metrics::CPUMetricsProvider>();
  delegating_provider.RegisterMetricsProvider(std::move(cpu_provider));
  log.RecordEnvironment(&delegating_provider);

  // Check non-system profile values.
  EXPECT_EQ(MetricsLog::Hash(kClientId), log.uma_proto().client_id());
  EXPECT_EQ(kSessionId, log.uma_proto().session_id());
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());

  // Call RecordEnvironment() again and verify things are are still filled in.
  log.RecordEnvironment(&delegating_provider);

  // Check non-system profile values.
  EXPECT_EQ(MetricsLog::Hash(kClientId), log.uma_proto().client_id());
  EXPECT_EQ(kSessionId, log.uma_proto().session_id());
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());
}

TEST_F(MetricsLogTest, RecordEnvironmentExtendedStable) {
  client_.set_is_extended_stable_channel(true);
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);

  DelegatingProvider delegating_provider;
  auto cpu_provider = std::make_unique<metrics::CPUMetricsProvider>();
  delegating_provider.RegisterMetricsProvider(std::move(cpu_provider));
  log.RecordEnvironment(&delegating_provider);

  EXPECT_TRUE(log.system_profile().has_is_extended_stable_channel());
  EXPECT_TRUE(log.system_profile().is_extended_stable_channel());
}

TEST_F(MetricsLogTest, RecordEnvironmentEnableDefault) {
  TestMetricsLog log_unknown(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client_);

  DelegatingProvider delegating_provider;
  log_unknown.RecordEnvironment(&delegating_provider);
  EXPECT_FALSE(log_unknown.system_profile().has_uma_default_state());

  client_.set_enable_default(EnableMetricsDefault::OPT_IN);
  TestMetricsLog log_opt_in(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                            &client_);
  log_opt_in.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_opt_in.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_IN,
            log_opt_in.system_profile().uma_default_state());

  client_.set_enable_default(EnableMetricsDefault::OPT_OUT);
  TestMetricsLog log_opt_out(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client_);
  log_opt_out.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_opt_out.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_OUT,
            log_opt_out.system_profile().uma_default_state());

  client_.set_reporting_is_managed(true);
  TestMetricsLog log_managed(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client_);
  log_managed.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_managed.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_POLICY_FORCED_ENABLED,
            log_managed.system_profile().uma_default_state());
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                     &client_);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  DelegatingProvider delegating_provider;
  delegating_provider.RegisterMetricsProvider(
      base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(&delegating_provider);
  log.RecordPreviousSessionData(&delegating_provider, &prefs_);

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  DelegatingProvider delegating_provider;
  delegating_provider.RegisterMetricsProvider(
      base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(&delegating_provider);
  log.RecordCurrentSessionData(base::TimeDelta(), base::TimeDelta(),
                               &delegating_provider, &prefs_);

  // The test provider should have been called upon to provide regular but not
  // initial stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, ChromeChannelWrittenToProtobuf) {
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  EXPECT_TRUE(log.uma_proto().system_profile().has_channel());
}

TEST_F(MetricsLogTest, ProductNotSetIfDefault) {
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, client_.GetProduct());
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  // Check that the product isn't set, since it's default and also verify the
  // default value is indeed equal to Chrome.
  EXPECT_FALSE(log.uma_proto().has_product());
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, log.uma_proto().product());
}

TEST_F(MetricsLogTest, ProductSetIfNotDefault) {
  const int32_t kTestProduct = 100;
  EXPECT_NE(ChromeUserMetricsExtension::CHROME, kTestProduct);
  client_.set_product(kTestProduct);
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client_);
  // Check that the product is set to |kTestProduct|.
  EXPECT_TRUE(log.uma_proto().has_product());
  EXPECT_EQ(kTestProduct, log.uma_proto().product());
}

TEST_F(MetricsLogTest, ToInstallerPackage) {
  using internal::ToInstallerPackage;
  EXPECT_EQ(SystemProfileProto::INSTALLER_PACKAGE_NONE, ToInstallerPackage(""));
  EXPECT_EQ(SystemProfileProto::INSTALLER_PACKAGE_GOOGLE_PLAY_STORE,
            ToInstallerPackage("com.android.vending"));
  EXPECT_EQ(SystemProfileProto::INSTALLER_PACKAGE_OTHER,
            ToInstallerPackage("foo"));
}

}  // namespace metrics
