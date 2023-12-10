// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"

#include <string>
#include <utility>

#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "execution_context.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace metrics {

using ::testing::Eq;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

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

namespace {
void AttachProfileMetadata(SampledProfile& profile,
                           uint32_t name_hash_index,
                           int64_t key,
                           int64_t value) {
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  CallStackProfile::MetadataItem* item =
      call_stack_profile->mutable_profile_metadata()->Add();
  item->set_name_hash_index(name_hash_index);
  item->set_key(key);
  item->set_value(value);
}
}  // namespace

// Checks that internal-use profile metadata is removed, when retrieving
// profiles from the Provider.
TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedSerialized) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric0"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric1"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric2"));
  AttachProfileMetadata(profile, 1, 2, 12);
  AttachProfileMetadata(profile, 3, 3, 13);
  AttachProfileMetadata(profile, 1, 4, 14);
  AttachProfileMetadata(profile, 3, 5, 15);

  // Irrelevant profile metadata that should not be removed.
  AttachProfileMetadata(profile, 0, 0, 0);
  AttachProfileMetadata(profile, 2, 0, 0);
  AttachProfileMetadata(profile, 4, 0, 0);

  EXPECT_EQ(5, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(7, profile.call_stack_profile().profile_metadata_size());

  // Receive serialized profiles.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/false, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  ASSERT_EQ(3, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(base::HashMetricName("OtherMetric0"),
            result_profile.call_stack_profile().metadata_name_hash(0));
  EXPECT_EQ(base::HashMetricName("OtherMetric1"),
            result_profile.call_stack_profile().metadata_name_hash(1));
  EXPECT_EQ(base::HashMetricName("OtherMetric2"),
            result_profile.call_stack_profile().metadata_name_hash(2));

  ASSERT_EQ(3, result_profile.call_stack_profile().profile_metadata_size());

  const auto& profile_metadata =
      result_profile.call_stack_profile().profile_metadata();
  EXPECT_EQ(0, profile_metadata[0].name_hash_index());
  EXPECT_EQ(1, profile_metadata[1].name_hash_index());
  EXPECT_EQ(2, profile_metadata[2].name_hash_index());
}

TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedUnserialized) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  // Unserialized profiles.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric0"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric1"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric2"));
  AttachProfileMetadata(profile, 1, 2, 12);
  AttachProfileMetadata(profile, 3, 3, 13);
  AttachProfileMetadata(profile, 1, 4, 14);
  AttachProfileMetadata(profile, 3, 5, 15);

  // Irrelevant profile metadata that should not be removed.
  AttachProfileMetadata(profile, 0, 0, 0);
  AttachProfileMetadata(profile, 2, 0, 0);
  AttachProfileMetadata(profile, 4, 0, 0);

  EXPECT_EQ(5, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(7, profile.call_stack_profile().profile_metadata_size());

  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  ASSERT_EQ(3, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(base::HashMetricName("OtherMetric0"),
            result_profile.call_stack_profile().metadata_name_hash(0));
  EXPECT_EQ(base::HashMetricName("OtherMetric1"),
            result_profile.call_stack_profile().metadata_name_hash(1));
  EXPECT_EQ(base::HashMetricName("OtherMetric2"),
            result_profile.call_stack_profile().metadata_name_hash(2));

  ASSERT_EQ(3, result_profile.call_stack_profile().profile_metadata_size());
  const auto& profile_metadata =
      result_profile.call_stack_profile().profile_metadata();
  EXPECT_EQ(0, profile_metadata[0].name_hash_index());
  EXPECT_EQ(1, profile_metadata[1].name_hash_index());
  EXPECT_EQ(2, profile_metadata[2].name_hash_index());
}

namespace {
void AttachSampleWithMetadata(SampledProfile& profile,
                              uint32_t name_hash_index,
                              int64_t key,
                              int64_t value) {
  auto* stack_sample =
      profile.mutable_call_stack_profile()->mutable_stack_sample()->Add();
  auto* item = stack_sample->mutable_metadata()->Add();
  item->set_name_hash_index(name_hash_index);
  item->set_key(key);
  item->set_value(value);
}
}  // namespace

// After name hash index array change, all name hash index mapping in stack
// sample metadata should remain correct.
TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedStackSampleMetadataHashIndex) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric0"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric1"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric2"));

  // Metadata item on stack sample.
  AttachSampleWithMetadata(profile, 0, 0, 0);
  AttachSampleWithMetadata(profile, 2, 0, 0);
  AttachSampleWithMetadata(profile, 4, 0, 0);

  EXPECT_EQ(5, profile.call_stack_profile().metadata_name_hash_size());

  // Receive serialized profiles.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/false, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  ASSERT_EQ(3, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(base::HashMetricName("OtherMetric0"),
            result_profile.call_stack_profile().metadata_name_hash(0));
  EXPECT_EQ(base::HashMetricName("OtherMetric1"),
            result_profile.call_stack_profile().metadata_name_hash(1));
  EXPECT_EQ(base::HashMetricName("OtherMetric2"),
            result_profile.call_stack_profile().metadata_name_hash(2));

  ASSERT_EQ(3, result_profile.call_stack_profile().stack_sample_size());
  EXPECT_EQ(0, result_profile.call_stack_profile()
                   .stack_sample(0)
                   .metadata(0)
                   .name_hash_index());
  EXPECT_EQ(1, result_profile.call_stack_profile()
                   .stack_sample(1)
                   .metadata(0)
                   .name_hash_index());
  EXPECT_EQ(2, result_profile.call_stack_profile()
                   .stack_sample(2)
                   .metadata(0)
                   .name_hash_index());
}

namespace {
void AttachSampleWithTimestamp(SampledProfile& profile,
                               int32_t timestamp_offset) {
  auto* stack_sample =
      profile.mutable_call_stack_profile()->mutable_stack_sample()->Add();
  stack_sample->set_sample_time_offset_ms(timestamp_offset);
}
}  // namespace

// Timestamps on each stack sample are removed.
TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedTimestamps) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.mutable_call_stack_profile()->set_profile_time_offset_ms(100);
  AttachSampleWithTimestamp(profile, 100);
  AttachSampleWithTimestamp(profile, 300);

  // Receive serialized profiles.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/false, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_FALSE(
      result_profile.call_stack_profile().has_profile_time_offset_ms());
  ASSERT_EQ(2, result_profile.call_stack_profile().stack_sample_size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_FALSE(result_profile.call_stack_profile()
                     .stack_sample(i)
                     .has_sample_time_offset_ms());
  }
}

// Heap profiles should not be affected.
TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedTimestampsHeapProfile) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_HEAP_COLLECTION);
  profile.mutable_call_stack_profile()->set_profile_time_offset_ms(100);

  // Receive serialized profiles.
  std::string contents;
  profile.SerializeToString(&contents);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      profile_start_time, /*is_heap_profile=*/true, std::move(contents));

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_TRUE(result_profile.call_stack_profile().has_profile_time_offset_ms());
}

TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataRemovedMissingHashIndex) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  // Unserialized profiles.
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  // Hash index for "Internal.LargestContentfulPaint.DocumentToken" not present.
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric0"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("OtherMetric1"));

  AttachProfileMetadata(profile, 1, 2, 12);
  AttachProfileMetadata(profile, 1, 4, 14);

  // Irrelevant profile metadata that should not be removed.
  AttachProfileMetadata(profile, 0, 0, 0);
  AttachProfileMetadata(profile, 2, 0, 0);

  EXPECT_EQ(3, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(4, profile.call_stack_profile().profile_metadata_size());

  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  ASSERT_EQ(2, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(base::HashMetricName("OtherMetric0"),
            result_profile.call_stack_profile().metadata_name_hash(0));
  EXPECT_EQ(base::HashMetricName("OtherMetric1"),
            result_profile.call_stack_profile().metadata_name_hash(1));

  ASSERT_EQ(2, result_profile.call_stack_profile().profile_metadata_size());
  const auto& profile_metadata =
      result_profile.call_stack_profile().profile_metadata();
  EXPECT_EQ(0, profile_metadata[0].name_hash_index());
  EXPECT_EQ(1, profile_metadata[1].name_hash_index());
}

TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataInvalidDataMissingDocumentToken) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));
  // Missing DocumentToken.
  AttachProfileMetadata(profile, 0, 2, 12);
  AttachProfileMetadata(profile, 0, 4, 14);

  EXPECT_EQ(2, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(2, profile.call_stack_profile().profile_metadata_size());
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  EXPECT_EQ(0, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(0, result_profile.call_stack_profile().profile_metadata_size());
}

TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataInvalidDataMissingNavigationStart) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));
  // Missing NavigationStart.
  AttachProfileMetadata(profile, 0, 2, 12);
  AttachProfileMetadata(profile, 0, 4, 14);

  EXPECT_EQ(2, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(2, profile.call_stack_profile().profile_metadata_size());
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  EXPECT_EQ(0, result_profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(0, result_profile.call_stack_profile().profile_metadata_size());
}

// Should not crash when the provided name_hash_index is beyond the end of
// name_hash array.
TEST_F(CallStackProfileMetricsProviderTest,
       InternalProfileMetadataInvalidDataInvalidIndices) {
  CallStackProfileMetricsProvider provider;
  base::TimeTicks profile_start_time = base::TimeTicks::Now();

  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  CallStackProfile* call_stack_profile = profile.mutable_call_stack_profile();
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.NavigationStart"));
  call_stack_profile->mutable_metadata_name_hash()->Add(
      base::HashMetricName("Internal.LargestContentfulPaint.DocumentToken"));

  AttachProfileMetadata(profile, 0, 2, 12);
  AttachProfileMetadata(profile, 2, 3, 13);  // Invalid index
  AttachProfileMetadata(profile, 0, 4, 14);
  AttachProfileMetadata(profile, 1, 5, 15);

  EXPECT_EQ(2, profile.call_stack_profile().metadata_name_hash_size());
  EXPECT_EQ(4, profile.call_stack_profile().profile_metadata_size());
  CallStackProfileMetricsProvider::ReceiveProfile(profile_start_time, profile);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  ASSERT_EQ(1, uma_proto.sampled_profile_size());

  const SampledProfile& result_profile = uma_proto.sampled_profile(0);
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION,
            result_profile.trigger_event());
  EXPECT_EQ(0, result_profile.call_stack_profile().metadata_name_hash_size());
  // The ProfileMetadata with wrong index is not detected.
  EXPECT_EQ(1, result_profile.call_stack_profile().profile_metadata_size());
}

#if BUILDFLAG(IS_CHROMEOS)

namespace {

// Sets |call_stack_profile| up enough to pass WasMinimallySuccessful()
void MakeMinimallySuccessfulCallStackProfile(
    CallStackProfile* call_stack_profile) {
  CallStackProfile::Stack* stack = call_stack_profile->add_stack();
  CallStackProfile::Location* frame = stack->add_frame();
  frame->set_address(123);
  frame->set_module_id_index(1);
  frame = stack->add_frame();
  frame->set_address(456);
  frame->set_module_id_index(0);
}

// Makes a minimally successful SampledProfile and sends it to ReceiveProfile.
void RecieveProfile(metrics::Process process, metrics::Thread thread) {
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  profile.set_process(process);
  profile.set_thread(thread);
  MakeMinimallySuccessfulCallStackProfile(profile.mutable_call_stack_profile());
  CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                  profile);
}

// Makes a minimally successful SampledProfile and sends it to
// ReceiveSerializedProfile.
void ReceiveSerializedProfile(metrics::Process process,
                              metrics::Thread thread) {
  SampledProfile profile;
  profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  profile.set_process(process);
  profile.set_thread(thread);
  MakeMinimallySuccessfulCallStackProfile(profile.mutable_call_stack_profile());
  std::string serialized_profile;
  profile.SerializeToString(&serialized_profile);
  CallStackProfileMetricsProvider::ReceiveSerializedProfile(
      base::TimeTicks::Now(), /*is_heap_profile=*/false,
      std::move(serialized_profile));
}

}  // namespace

// Checks that profiles which have been received but not send out are listed
// as successfully collected.
TEST_F(CallStackProfileMetricsProviderTest,
       SuccessfullyCollectedOnReceivedNotSent) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  RecieveProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);
  ReceiveSerializedProfile(metrics::GPU_PROCESS, metrics::MAIN_THREAD);

  EXPECT_THAT(
      CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts(),
      UnorderedElementsAre(
          Pair(Eq(metrics::GPU_PROCESS),
               UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(1)),
                                    Pair(Eq(metrics::MAIN_THREAD), Eq(1))))));
}

// Checks that profiles which have been send out are listed as successfully
// collected.
TEST_F(CallStackProfileMetricsProviderTest, SuccessfullyCollectedOnSent) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  RecieveProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);
  ReceiveSerializedProfile(metrics::BROWSER_PROCESS, metrics::IO_THREAD);

  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(2, uma_proto.sampled_profile().size());

  EXPECT_THAT(
      CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts(),
      UnorderedElementsAre(
          Pair(Eq(metrics::GPU_PROCESS),
               UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(1)))),
          Pair(Eq(metrics::BROWSER_PROCESS),
               UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(1))))));
}

// Checks that profiles which are send and profiles which are unsent are
// correctly summed together.
TEST_F(CallStackProfileMetricsProviderTest,
       SuccessfullyCollectedMixedSentUnsent) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  RecieveProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);
  ReceiveSerializedProfile(metrics::BROWSER_PROCESS, metrics::IO_THREAD);

  // Send the first 2 metrics.
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
  EXPECT_EQ(2, uma_proto.sampled_profile().size());

  RecieveProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);
  ReceiveSerializedProfile(metrics::BROWSER_PROCESS, metrics::MAIN_THREAD);

  EXPECT_THAT(
      CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts(),
      UnorderedElementsAre(
          Pair(Eq(metrics::GPU_PROCESS),
               UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(2)))),
          Pair(Eq(metrics::BROWSER_PROCESS),
               UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(1)),
                                    Pair(Eq(metrics::MAIN_THREAD), Eq(1))))));
}

// Checks that "unsuccessful" profiles (profiles with 1 or no stack) are not
// counted.
TEST_F(CallStackProfileMetricsProviderTest,
       SuccessfullyCollectedIgnoresUnsuccessful) {
  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  RecieveProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);
  ReceiveSerializedProfile(metrics::GPU_PROCESS, metrics::IO_THREAD);

  {
    SampledProfile no_stack_profile;
    no_stack_profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    no_stack_profile.set_process(metrics::BROWSER_PROCESS);
    no_stack_profile.set_thread(metrics::MAIN_THREAD);
    CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                    no_stack_profile);
    std::string serialized_no_stack_profile;
    no_stack_profile.SerializeToString(&serialized_no_stack_profile);
    CallStackProfileMetricsProvider::ReceiveSerializedProfile(
        base::TimeTicks::Now(), /*is_heap_profile=*/false,
        std::move(serialized_no_stack_profile));
  }

  {
    SampledProfile one_frame_profile;
    one_frame_profile.set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
    one_frame_profile.set_process(metrics::BROWSER_PROCESS);
    one_frame_profile.set_thread(metrics::MAIN_THREAD);
    CallStackProfile::Stack* stack =
        one_frame_profile.mutable_call_stack_profile()->add_stack();
    CallStackProfile::Location* frame = stack->add_frame();
    frame->set_address(123);
    frame->set_module_id_index(1);
    CallStackProfileMetricsProvider::ReceiveProfile(base::TimeTicks::Now(),
                                                    one_frame_profile);
    std::string serialized_one_frame_profile;
    one_frame_profile.SerializeToString(&serialized_one_frame_profile);
    CallStackProfileMetricsProvider::ReceiveSerializedProfile(
        base::TimeTicks::Now(), /*is_heap_profile=*/false,
        std::move(serialized_one_frame_profile));
  }

  // All the BROWSER_PROCESS profiles were unsuccessful, so only the GPU_PROCESS
  // profiles should be counted.

  EXPECT_THAT(CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts(),
              UnorderedElementsAre(Pair(
                  Eq(metrics::GPU_PROCESS),
                  UnorderedElementsAre(Pair(Eq(metrics::IO_THREAD), Eq(2))))));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace metrics
