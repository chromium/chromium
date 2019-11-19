// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/sample_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/active_field_trials.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/current_module.h"
#endif

namespace metrics {

namespace {

const char kClientId[] = "bogus client ID";
const int kSessionId = 127;

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 LogType log_type,
                 MetricsServiceClient* client)
      : MetricsLog(client_id, session_id, log_type, client) {}

  ~TestMetricsLog() override {}

  const ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  ChromeUserMetricsExtension* mutable_uma_proto() {
    return MetricsLog::uma_proto();
  }

  const SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() {}
  ~MetricsLogTest() override {}

 protected:
  // Check that the values in |system_values| are filled in and expected ones
  // correspond to the test data defined at the top of this file.
  void CheckSystemProfile(const SystemProfileProto& system_profile) {
    // Check for presence of core system profile fields.
    EXPECT_TRUE(system_profile.has_build_timestamp());
    EXPECT_TRUE(system_profile.has_app_version());
    EXPECT_TRUE(system_profile.has_channel());
    EXPECT_TRUE(system_profile.has_application_locale());

    const SystemProfileProto::OS& os = system_profile.os();
    EXPECT_TRUE(os.has_name());
    EXPECT_TRUE(os.has_version());

    // Check matching test brand code.
    EXPECT_EQ(TestMetricsServiceClient::kBrandForTesting,
              system_profile.brand_code());

    // Check for presence of fields set by a metrics provider.
    const SystemProfileProto::Hardware& hardware = system_profile.hardware();
    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());
    EXPECT_TRUE(hardware.cpu().has_num_cores());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsLogTest);
};

TEST_F(MetricsLogTest, LogType) {
  TestMetricsServiceClient client;
  TestingPrefServiceSimple prefs;

  MetricsLog log1("id", 0, MetricsLog::ONGOING_LOG, &client);
  EXPECT_EQ(MetricsLog::ONGOING_LOG, log1.log_type());

  MetricsLog log2("id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client);
  EXPECT_EQ(MetricsLog::INITIAL_STABILITY_LOG, log2.log_type());
}

TEST_F(MetricsLogTest, BasicRecord) {
  TestMetricsServiceClient client;
  client.set_version_string("bogus version");
  TestingPrefServiceSimple prefs;
  MetricsLog log("totally bogus client ID", 137, MetricsLog::ONGOING_LOG,
                 &client);
  log.CloseLog();

  std::string encoded;
  log.GetEncodedLog(&encoded);

  // A couple of fields are hard to mock, so these will be copied over directly
  // for the expected output.
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));

  ChromeUserMetricsExtension expected;
  expected.set_client_id(5217101509553811875);  // Hashed bogus client ID
  expected.set_session_id(137);

  SystemProfileProto* system_profile = expected.mutable_system_profile();
  system_profile->set_app_version("bogus version");
  system_profile->set_channel(client.GetChannel());
  system_profile->set_application_locale(client.GetApplicationLocale());

#if defined(ADDRESS_SANITIZER) || DCHECK_IS_ON()
  system_profile->set_is_instrumented_build(true);
#endif
  metrics::SystemProfileProto::Hardware* hardware =
      system_profile->mutable_hardware();
#if !defined(OS_IOS)
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
#endif
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
  hardware->set_hardware_class(base::SysInfo::HardwareModelName());
#if defined(OS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64_t>(CURRENT_MODULE()));
#endif

  system_profile->mutable_os()->set_name(base::SysInfo::OperatingSystemName());
  system_profile->mutable_os()->set_version(
      base::SysInfo::OperatingSystemVersion());
#if defined(OS_CHROMEOS)
  system_profile->mutable_os()->set_kernel_version(
      base::SysInfo::KernelVersion());
#elif defined(OS_LINUX)
  system_profile->mutable_os()->set_kernel_version(
      base::SysInfo::OperatingSystemVersion());
#elif defined(OS_ANDROID)
  system_profile->mutable_os()->set_build_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
  system_profile->set_app_package_name("test app");
#elif defined(OS_IOS)
  system_profile->mutable_os()->set_build_number(
      base::SysInfo::GetIOSBuildNumber());
#endif

  // Hard to mock.
  system_profile->set_build_timestamp(
      parsed.system_profile().build_timestamp());

  EXPECT_EQ(expected.SerializeAsString(), encoded);
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

  TestMetricsServiceClient client;
  TestingPrefServiceSimple prefs;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
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

TEST_F(MetricsLogTest, RecordEnvironment) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);

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

TEST_F(MetricsLogTest, RecordEnvironmentEnableDefault) {
  TestMetricsServiceClient client;
  TestMetricsLog log_unknown(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client);

  DelegatingProvider delegating_provider;
  log_unknown.RecordEnvironment(&delegating_provider);
  EXPECT_FALSE(log_unknown.system_profile().has_uma_default_state());

  client.set_enable_default(EnableMetricsDefault::OPT_IN);
  TestMetricsLog log_opt_in(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                            &client);
  log_opt_in.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_opt_in.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_IN,
            log_opt_in.system_profile().uma_default_state());

  client.set_enable_default(EnableMetricsDefault::OPT_OUT);
  TestMetricsLog log_opt_out(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client);
  log_opt_out.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_opt_out.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_OUT,
            log_opt_out.system_profile().uma_default_state());

  client.set_reporting_is_managed(true);
  TestMetricsLog log_managed(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client);
  log_managed.RecordEnvironment(&delegating_provider);
  EXPECT_TRUE(log_managed.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_POLICY_FORCED_ENABLED,
            log_managed.system_profile().uma_default_state());
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG,
                     &client);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  DelegatingProvider delegating_provider;
  delegating_provider.RegisterMetricsProvider(
      base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(&delegating_provider);
  log.RecordPreviousSessionData(&delegating_provider);

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  DelegatingProvider delegating_provider;
  delegating_provider.RegisterMetricsProvider(
      base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(&delegating_provider);
  log.RecordCurrentSessionData(&delegating_provider, base::TimeDelta(),
                               base::TimeDelta());

  // The test provider should have been called upon to provide regular but not
  // initial stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, ChromeChannelWrittenToProtobuf) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  EXPECT_TRUE(log.uma_proto().system_profile().has_channel());
}

TEST_F(MetricsLogTest, ProductNotSetIfDefault) {
  TestMetricsServiceClient client;
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, client.GetProduct());
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  // Check that the product isn't set, since it's default and also verify the
  // default value is indeed equal to Chrome.
  EXPECT_FALSE(log.uma_proto().has_product());
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, log.uma_proto().product());
}

TEST_F(MetricsLogTest, ProductSetIfNotDefault) {
  const int32_t kTestProduct = 100;
  EXPECT_NE(ChromeUserMetricsExtension::CHROME, kTestProduct);

  TestMetricsServiceClient client;
  client.set_product(kTestProduct);
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  // Check that the product is set to |kTestProduct|.
  EXPECT_TRUE(log.uma_proto().has_product());
  EXPECT_EQ(kTestProduct, log.uma_proto().product());
}

TEST_F(MetricsLogTest, TruncateEvents) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);

  for (int i = 0; i < internal::kUserActionEventLimit * 2; ++i) {
    log.RecordUserAction("BasicAction");
    EXPECT_EQ(i + 1, log.uma_proto().user_action_event_size());
  }
  for (int i = 0; i < internal::kOmniboxEventLimit * 2; ++i) {
    // Add an empty omnibox event. Not fully realistic since these are normally
    // supplied by a metrics provider.
    log.mutable_uma_proto()->add_omnibox_event();
    EXPECT_EQ(i + 1, log.uma_proto().omnibox_event_size());
  }

  // Truncate, and check that the current size is the limit.
  log.TruncateEvents();
  EXPECT_EQ(internal::kUserActionEventLimit,
            log.uma_proto().user_action_event_size());
  EXPECT_EQ(internal::kOmniboxEventLimit, log.uma_proto().omnibox_event_size());
}

}  // namespace metrics
