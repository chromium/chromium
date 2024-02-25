// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_metadata.h"

#include <tuple>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Expects that |expected_item| was applied to |samples| at |sample_index| and
// |metadata_index|. Because of the "edge-triggered" metadata encoding, this
// expectation will be valid for the first sample seeing the item only.
void ExpectMetadataApplied(
    const base::MetadataRecorder::Item& expected_item,
    const google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>&
        samples,
    int sample_index,
    int metadata_index,
    const google::protobuf::RepeatedField<uint64_t>& name_hashes) {
  const std::string index_info =
      base::StrCat({"at sample_index ", base::NumberToString(sample_index),
                    ", metadata_index ", base::NumberToString(metadata_index)});
  const int name_hash_index =
      base::ranges::find(name_hashes, expected_item.name_hash) -
      name_hashes.begin();
  ASSERT_NE(name_hash_index, name_hashes.size()) << index_info;

  ASSERT_LT(sample_index, samples.size()) << index_info;
  const CallStackProfile::StackSample& sample = samples[sample_index];
  ASSERT_LT(metadata_index, sample.metadata_size()) << index_info;
  const CallStackProfile::MetadataItem& item = sample.metadata(metadata_index);
  EXPECT_EQ(name_hash_index, item.name_hash_index()) << index_info;

  EXPECT_EQ(expected_item.key.has_value(), item.has_key()) << index_info;
  if (expected_item.key.has_value())
    EXPECT_EQ(*expected_item.key, item.key()) << index_info;
  EXPECT_EQ(expected_item.value, item.value()) << index_info;
}

// Expects that the |item| was unapplied at |sample|. Because of the
// "edge-triggered" metadata encoding, this expectation will be valid for the
// sample following the last sample with the item only.
void ExpectMetadataUnapplied(
    const base::MetadataRecorder::Item& expected_item,
    const google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>&
        samples,
    int sample_index,
    int metadata_index,
    const google::protobuf::RepeatedField<uint64_t>& name_hashes) {
  const std::string index_info =
      base::StrCat({"at sample_index ", base::NumberToString(sample_index),
                    ", metadata_index ", base::NumberToString(metadata_index)});
  const int name_hash_index =
      base::ranges::find(name_hashes, expected_item.name_hash) -
      name_hashes.begin();
  ASSERT_NE(name_hash_index, name_hashes.size()) << index_info;

  ASSERT_LT(sample_index, samples.size()) << index_info;
  const CallStackProfile::StackSample& sample = samples[sample_index];
  ASSERT_LT(metadata_index, sample.metadata_size()) << index_info;
  const CallStackProfile::MetadataItem& item = sample.metadata(metadata_index);
  EXPECT_EQ(name_hash_index, item.name_hash_index()) << index_info;

  EXPECT_EQ(expected_item.key.has_value(), item.has_key()) << index_info;
  if (expected_item.key.has_value())
    EXPECT_EQ(*expected_item.key, item.key()) << index_info;
  EXPECT_FALSE(item.has_value()) << index_info;
}

}  // namespace

TEST(CallStackProfileMetadataTest, MetadataRecorder_NoItems) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));

  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(0, name_hashes.size());
  ASSERT_EQ(0, items.size());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_SetItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(100u, name_hashes[0]);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_EQ(10, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_SetKeyedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(100u, name_hashes[0]);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_TRUE(items[0].has_key());
  EXPECT_EQ(50, items[0].key());
  EXPECT_EQ(10, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_SetThreadItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, base::PlatformThread::CurrentId(),
                        10);
  metadata_recorder.Set(100, std::nullopt, base::kInvalidThreadId, 20);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(100u, name_hashes[0]);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_EQ(10, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_RepeatItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  // The second sample shouldn't have any metadata because it's all the same
  // as the last sample.
  EXPECT_EQ(1, name_hashes.size());
  EXPECT_TRUE(items.empty());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_RepeatKeyedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  // The second sample shouldn't have any metadata because it's all the same
  // as the last sample.
  EXPECT_EQ(1, name_hashes.size());
  EXPECT_TRUE(items.empty());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_ModifiedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 11);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_EQ(11, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_ModifiedKeyedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(100, 50, std::nullopt, 11);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_TRUE(items[0].has_key());
  EXPECT_EQ(50, items[0].key());
  EXPECT_EQ(11, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_NewItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(101, std::nullopt, std::nullopt, 11);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(2, name_hashes.size());
  EXPECT_EQ(101u, name_hashes[1]);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(1, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_EQ(11, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_NewKeyedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(101, 50, std::nullopt, 11);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(2, name_hashes.size());
  EXPECT_EQ(101u, name_hashes[1]);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(1, items[0].name_hash_index());
  EXPECT_TRUE(items[0].has_key());
  EXPECT_EQ(50, items[0].key());
  EXPECT_EQ(11, items[0].value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_RemovedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, std::nullopt, std::nullopt);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_FALSE(items[0].has_value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_RemovedKeyedItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, 50, std::nullopt);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_TRUE(items[0].has_key());
  EXPECT_EQ(50, items[0].key());
  EXPECT_FALSE(items[0].has_value());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_RemovedThreadItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, base::PlatformThread::CurrentId(),
                        10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, std::nullopt,
                           base::PlatformThread::CurrentId());
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_FALSE(items[0].has_value());
}

TEST(CallStackProfileMetadataTest,
     MetadataRecorder_SetMixedUnkeyedAndKeyedItems) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 20);
  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(2, items.size());

  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_EQ(20, items[0].value());

  EXPECT_EQ(0, items[1].name_hash_index());
  EXPECT_TRUE(items[1].has_key());
  EXPECT_EQ(50, items[1].key());
  EXPECT_EQ(10, items[1].value());
}

TEST(CallStackProfileMetadataTest,
     MetadataRecorder_RemoveMixedUnkeyedAndKeyedItems) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, std::nullopt, std::nullopt, 20);
  metadata_recorder.Set(100, 50, std::nullopt, 10);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  std::ignore = metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, std::nullopt, std::nullopt);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_FALSE(items[0].has_value());
}

// Checks that applying metadata results in the expected application and removal
// of the metadata.
TEST(CallStackProfileMetadataTest, ApplyMetadata_Basic) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);
  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  // One metadata item should be recorded when the metadata starts.
  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  // And one item should be recorded without value after the metadata ends.
  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks that metadata items with different name hashes are applied
// independently.
TEST(CallStackProfileMetadataTest, ApplyMetadata_DifferentNameHashes) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(4, 30, std::nullopt, 300);
  metadata.ApplyMetadata(item1, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);
  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(2, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);
  EXPECT_EQ(4u, name_hashes[1]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  EXPECT_EQ(2, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 1, 0, name_hashes);
  ExpectMetadataApplied(item2, stack_samples, 1, 1, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(2, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item1, stack_samples, 4, 0, name_hashes);
  ExpectMetadataUnapplied(item2, stack_samples, 4, 1, name_hashes);
}

// Checks that metadata items with different keys are applied independently.
TEST(CallStackProfileMetadataTest, ApplyMetadata_DifferentKeys) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 40, std::nullopt, 300);
  const base::MetadataRecorder::Item item3(3, std::nullopt, std::nullopt, 300);
  metadata.ApplyMetadata(item1, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);
  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);
  metadata.ApplyMetadata(item3, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  EXPECT_EQ(3, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 1, 0, name_hashes);
  ExpectMetadataApplied(item2, stack_samples, 1, 1, name_hashes);
  ExpectMetadataApplied(item3, stack_samples, 1, 2, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(3, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item1, stack_samples, 4, 0, name_hashes);
  ExpectMetadataUnapplied(item2, stack_samples, 4, 1, name_hashes);
  ExpectMetadataUnapplied(item3, stack_samples, 4, 2, name_hashes);
}

// Checks that applying to an empty range has no effect.
TEST(CallStackProfileMetadataTest, ApplyMetadata_EmptyRange) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);
  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 1, &stack_samples,
                         &name_hashes);

  EXPECT_EQ(0, name_hashes.size());

  for (int i = 0; i < 5; i++)
    EXPECT_EQ(0, stack_samples[i].metadata_size());
}

// Checks that applying metadata through the end is recorded as unapplied on the
// next sample taken.
TEST(CallStackProfileMetadataTest, ApplyMetadata_ThroughEnd) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);
  metadata.ApplyMetadata(item, stack_samples.begin() + 1, stack_samples.end(),
                         &stack_samples, &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  // One metadata item should be recorded when the metadata starts.
  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());
  EXPECT_EQ(0, stack_samples[4].metadata_size());

  base::MetadataRecorder metadata_recorder;
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  // And the following sample should have the metadata unapplied.
  ExpectMetadataUnapplied(item, stack_samples, 5, 0, name_hashes);
}

// Checks that metadata is properly applied when mixing RecordMetadata and
// ApplyMetadata over the same samples.
TEST(CallStackProfileMetadataTest, ApplyMetadata_WithRecordMetadata) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(5, 50, std::nullopt, 500);

  stack_samples.Add();

  // Apply then remove item1.
  metadata_recorder.Set(item1.name_hash, *item1.key, item1.thread_id,
                        item1.value);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(item1.name_hash, *item1.key, item1.thread_id);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  stack_samples.Add();

  ASSERT_EQ(5, stack_samples.size());

  // Apply item2.
  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(2, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);
  EXPECT_EQ(5u, name_hashes[1]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  // Each of the two items should be recorded when their metadata starts.
  ASSERT_EQ(2, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 1, 0, name_hashes);
  ExpectMetadataApplied(item2, stack_samples, 1, 1, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());

  // The original item should still be present.
  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataUnapplied(item1, stack_samples, 3, 0, name_hashes);

  // And one item should be recorded without value after the metadata ends.
  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item2, stack_samples, 4, 0, name_hashes);
}

// Checks that metadata is properly applied when using ApplyMetadata while
// a RecordMetadata-applied value is active.
TEST(CallStackProfileMetadataTest, ApplyMetadata_WithActiveMetadata) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  // Record item1 on an ongoing basis via RecordMetadata.
  metadata_recorder.Set(item1.name_hash, *item1.key, item1.thread_id,
                        item1.value);
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(3, stack_samples.size());

  // Apply item2 via ApplyMetadata up to the last sample.
  metadata.ApplyMetadata(item2, stack_samples.begin(), stack_samples.end(),
                         &stack_samples, &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());

  // The next recorded sample should have item1 applied since it's still active.
  metadata.RecordMetadata(base::MetadataRecorder::MetadataProvider(
      &metadata_recorder, base::PlatformThread::CurrentId()));
  *stack_samples.Add()->mutable_metadata() =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 3, 0, name_hashes);
}

// Checks application of the same item across non-overlapping ranges.
TEST(CallStackProfileMetadataTest, ApplyMetadata_IndependentRanges) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over two non-overlapping ranges.
  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 2,
                         &stack_samples, &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin() + 3,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());

  EXPECT_EQ(1, stack_samples[2].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 2, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 3, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks application of the same item across back-to-back ranges. The common
// sample should not have a metadata item set because it's unnecessary.
TEST(CallStackProfileMetadataTest, ApplyMetadata_BackToBackRanges) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over two ranges where the second starts on the same sample
  // that the first ends. This should result in one range covering both.
  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 2,
                         &stack_samples, &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin() + 2,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks application of different values across back-to-back ranges. The common
// sample must have the second metadata value set.
TEST(CallStackProfileMetadataTest,
     ApplyMetadata_BackToBackRangesWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  metadata.ApplyMetadata(item1, stack_samples.begin(),
                         stack_samples.begin() + 2, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin() + 2,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());

  EXPECT_EQ(1, stack_samples[2].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 2, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item2, stack_samples, 4, 0, name_hashes);
}

// Checks application of the same item over a range within a range where the
// item was already set. No metadata changes should be recorded on the interior
// range because they are unnecessary.
TEST(CallStackProfileMetadataTest, ApplyMetadata_UpdateWithinExistingRange) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 4,
                         &stack_samples, &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks application of a second value over a range within a range where the
// first value was already set. Metadata changes for the second value must be
// recorded on the interior range.
TEST(CallStackProfileMetadataTest,
     ApplyMetadata_UpdateWithinExistingRangeWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  // Apply metadata over a range, then over a range fully enclosed within the
  // first one.
  metadata.ApplyMetadata(item1, stack_samples.begin(),
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());

  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 3, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item1, stack_samples, 4, 0, name_hashes);
}

// Checks application of the same item over a range enclosing a range where the
// item was already set. No metadata changes should be recorded on the interior
// range because they are unnecessary.
TEST(CallStackProfileMetadataTest, ApplyMetadata_UpdateEnclosesExistingRange) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over a range, then over a range that fully encloses the
  // first one.
  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 4,
                         &stack_samples, &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks application of a second value over a range enclosing a range where the
// first value was already set. Metadata changes for both values must be
// recorded.
TEST(CallStackProfileMetadataTest,
     ApplyMetadata_UpdateEnclosesExistingRangeWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  // Apply metadata over a range, then over a range that fully encloses the
  // first one.
  metadata.ApplyMetadata(item1, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin(),
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item2, stack_samples, 4, 0, name_hashes);
}

// Checks application of an item over a range overlapping the start (but not
// end) of a range where the item was already set. No metadata changes should be
// recorded on the interior application because it is unnecessary.
TEST(CallStackProfileMetadataTest, ApplyMetadata_UpdateOverlapsBegin) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over a range, then over a range that overlaps the beginning
  // (but not the end) of first one.
  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 2,
                         &stack_samples, &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());

  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 3, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[4].metadata_size());
}

// Checks application of a second different value over a range overlapping the
// start (but not end) of a range where the first value was already
// set. Metadata changes must be recorded on the interior application.
TEST(CallStackProfileMetadataTest,
     ApplyMetadata_UpdateOverlapsBeginWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  // Apply metadata over a range, then over a range that overlaps the beginning
  // (but not the end) of first one.
  metadata.ApplyMetadata(item1, stack_samples.begin() + 1,
                         stack_samples.begin() + 3, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin(),
                         stack_samples.begin() + 2, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(1, stack_samples[2].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 2, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[3].metadata_size());
  ExpectMetadataUnapplied(item1, stack_samples, 3, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[4].metadata_size());
}

// Checks application of an item over a range overlapping the end (but not
// start) of a range where the item was already set. No metadata changes should
// be recorded on the interior application because it is unnecessary.
TEST(CallStackProfileMetadataTest, ApplyMetadata_UpdateOverlapsEnd) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over a range, then over a range that overlaps the beginning
  // (but not the end) of first one.
  metadata.ApplyMetadata(item, stack_samples.begin(), stack_samples.begin() + 2,
                         &stack_samples, &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[1].metadata_size());
  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks application of a second different value over a range overlapping the
// end (but not start) of a range where the first value was already
// set. Metadata changes must be recorded on the interior application.
TEST(CallStackProfileMetadataTest,
     ApplyMetadata_UpdateOverlapsEndWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  // Apply metadata over a range, then over a range that overlaps the beginning
  // (but not the end) of first one.
  metadata.ApplyMetadata(item1, stack_samples.begin(),
                         stack_samples.begin() + 2, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(1, stack_samples[0].metadata_size());
  ExpectMetadataApplied(item1, stack_samples, 0, 0, name_hashes);

  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item2, stack_samples, 4, 0, name_hashes);
}

// Checks that updating the same range multiple times with the same item
// produces the same result as what we'd expect with just one update.
TEST(CallStackProfileMetadataTest, ApplyMetadata_Update) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item(3, 30, std::nullopt, 300);

  // Apply metadata over the same range with one value, then a different value.
  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item, stack_samples, 4, 0, name_hashes);
}

// Checks that applying to the same range with a different value overwrites the
// initial value.
TEST(CallStackProfileMetadataTest, ApplyMetadata_UpdateWithDifferentValues) {
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>
      stack_samples;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  for (int i = 0; i < 5; i++)
    stack_samples.Add();

  const base::MetadataRecorder::Item item1(3, 30, std::nullopt, 300);
  const base::MetadataRecorder::Item item2(3, 30, std::nullopt, 400);

  // Apply metadata over the same range with one value, then a different value.
  metadata.ApplyMetadata(item1, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  metadata.ApplyMetadata(item2, stack_samples.begin() + 1,
                         stack_samples.begin() + 4, &stack_samples,
                         &name_hashes);

  ASSERT_EQ(1, name_hashes.size());
  EXPECT_EQ(3u, name_hashes[0]);

  EXPECT_EQ(0, stack_samples[0].metadata_size());

  EXPECT_EQ(1, stack_samples[1].metadata_size());
  ExpectMetadataApplied(item2, stack_samples, 1, 0, name_hashes);

  EXPECT_EQ(0, stack_samples[2].metadata_size());
  EXPECT_EQ(0, stack_samples[3].metadata_size());

  EXPECT_EQ(1, stack_samples[4].metadata_size());
  ExpectMetadataUnapplied(item2, stack_samples, 4, 0, name_hashes);
}

}  // namespace metrics
