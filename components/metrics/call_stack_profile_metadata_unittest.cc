// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metadata.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

TEST(CallStackProfileMetadataTest, MetadataRecorder_NoItems) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());

  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(0, name_hashes.size());
  ASSERT_EQ(0, items.size());
}

TEST(CallStackProfileMetadataTest, MetadataRecorder_SetItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, base::nullopt, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

TEST(CallStackProfileMetadataTest, MetadataRecorder_RepeatItem) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, base::nullopt, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, base::nullopt, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(100, base::nullopt, 11);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(100, 50, 11);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, base::nullopt, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(101, base::nullopt, 11);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Set(101, 50, 11);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, base::nullopt, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, base::nullopt);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, 50);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  EXPECT_EQ(1, name_hashes.size());

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_TRUE(items[0].has_key());
  EXPECT_EQ(50, items[0].key());
  EXPECT_FALSE(items[0].has_value());
}

TEST(CallStackProfileMetadataTest,
     MetadataRecorder_SetMixedUnkeyedAndKeyedItems) {
  base::MetadataRecorder metadata_recorder;
  CallStackProfileMetadata metadata;
  google::protobuf::RepeatedField<uint64_t> name_hashes;

  metadata_recorder.Set(100, base::nullopt, 20);
  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
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

  metadata_recorder.Set(100, base::nullopt, 20);
  metadata_recorder.Set(100, 50, 10);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  (void)metadata.CreateSampleMetadata(&name_hashes);

  metadata_recorder.Remove(100, base::nullopt);
  metadata.RecordMetadata(metadata_recorder.CreateMetadataProvider().get());
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem> items =
      metadata.CreateSampleMetadata(&name_hashes);

  ASSERT_EQ(1, items.size());
  EXPECT_EQ(0, items[0].name_hash_index());
  EXPECT_FALSE(items[0].has_key());
  EXPECT_FALSE(items[0].has_value());
}

}  // namespace metrics
