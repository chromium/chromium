// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

// This test fixture enables the feature that
// CallStackProfileMetricsProvider depends on to report a profile.
class CallStackProfileMetricsProviderTest : public testing::Test {
 public:
  CallStackProfileMetricsProviderTest() {
    TestState::ResetStaticStateForTesting();
    scoped_feature_list_.InitAndEnableFeature(kSamplingProfilerReporting);
  }

  CallStackProfileMetricsProviderTest(
      const CallStackProfileMetricsProviderTest&) = delete;
  CallStackProfileMetricsProviderTest& operator=(
      const CallStackProfileMetricsProviderTest&) = delete;

 protected:
  // Exposes the feature from the CallStackProfileMetricsProvider.
  class TestState : public CallStackProfileMetricsProvider {
   public:
    using CallStackProfileMetricsProvider::ResetStaticStateForTesting;
  };

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Checks that the unserialized pending profile is encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataUnserialized) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  profile);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            uma_proto.sampled_profile(0).trigger_event());
}

// Checks that the serialized pending profile is encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataSerialized) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  std::string contents;
  {
    SampledProfile profile;
    profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    profile.SerializeToString(&contents);
  }
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      base::TimeTicks::Now(), /*is_heap_profile=*/false, std::move(contents));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            uma_proto.sampled_profile(0).trigger_event());
}

// Checks that both the unserialized and serialized pending profiles are
// encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataUnserializedAndSerialized) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();

  // Receive an unserialized profile.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PROCESS_STARTUP);
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  std::move(profile));

  // Receive a serialized profile.
  std::string contents;
  {
    SampledProfile serialized_profile;
    serialized_profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    serialized_profile.SerializeToString(&contents);
  }
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      base::TimeTicks::Now(), /*is_heap_profile=*/false, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(2, uma_proto.sampled_profile().size());
  EXPECT_EQ(SampledProfile::PROCESS_STARTUP,
            uma_proto.sampled_profile(0).trigger_event());
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            uma_proto.sampled_profile(1).trigger_event());
}

// Checks that the pending profiles above the total cap are dropped therefore
// not encoded in the session data.
TEST_F(CallStackProfileMetricsProviderTest,
       ProvideCurrentSessionDataExceedTotalCap) {
  // The value must be consistent with that in
  // call_stack_profile_metrics_provider.cc so that this test is meaningful.
  const int kMaxPendingProfiles = 1250;

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();

  // Receive (kMaxPendingProfiles + 1) profiles.
  for (int i = 0; i < kMaxPendingProfiles + 1; ++i) {
    SampledProfile profile;
    profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                    std::move(profile));
  }

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);

  // Only kMaxPendingProfiles profiles are encoded, with the additional one
  // dropped.
  ASSERT_EQ(kMaxPendingProfiles, uma_proto.sampled_profile().size());
  for (int i = 0; i < kMaxPendingProfiles; ++i) {
    EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
              uma_proto.sampled_profile(i).trigger_event());
  }
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// when collected before CallStackProfileMetricsProvider is instantiated.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileProvidedWhenCollectedBeforeInstantiation) {
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  SampledProfile());
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(1, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// while recording is disabled.
TEST_F(CallStackProfileMetricsProviderTest, ProfileNotProvidedWhileDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is disabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingDisabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is not provided to ProvideCurrentSessionData
// if recording is enabled, but then disabled and reenabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeToDisabledThenEnabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingDisabled();
  provider.OnRecordingEnabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that the pending profile is provided to ProvideCurrentSessionData
// if recording is disabled, but then enabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfileNotProvidedAfterChangeFromDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();
  provider.OnRecordingEnabled();
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  SampledProfile());
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that a heap profile is not reported when recording is disabled.
TEST_F(CallStackProfileMetricsProviderTest,
       HeapProfileNotProvidedWhenDisabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  // Unserialized profile.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_HEAP_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  // Serialized profile.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/true, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that a heap profile is provided to ProvideCurrentSessionData
// if recording is enabled.
TEST_F(CallStackProfileMetricsProviderTest, HeapProfileProvidedWhenEnabled) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  // Unserialized profile.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_HEAP_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  // Serialized profile.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/true, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(2, uma_proto.sampled_profile_size());
}

// Checks that heap profiles but not CPU profiles are reported when sampling CPU
// Finch is disabled.
TEST_F(CallStackProfileMetricsProviderTest, CpuProfileNotProvidedWithoutFinch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kSamplingProfilerReporting);
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  // Unserialized profiles.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  SampledProfile heap_profile;
  heap_profile.set_trigger_event(SampledProfile::PERIODIC_HEAP_COLLECTION);
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time,
                                                  heap_profile);

  // Serialized profiles.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/false, std::move(contents));

  std::string heap_contents;
  heap_profile.SerializeToString(&heap_contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/true, std::move(heap_contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(2, uma_proto.sampled_profile_size());
  EXPECT_EQ(SampledProfile::PERIODIC_HEAP_COLLECTION,
            uma_proto.sampled_profile(0).trigger_event());
  EXPECT_EQ(SampledProfile::PERIODIC_HEAP_COLLECTION,
            uma_proto.sampled_profile(1).trigger_event());
}

}  // namespace metrics
