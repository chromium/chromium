// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mutable_data_batch.h"

#include "base/memory/ptr_util.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(MutableDataBatchTest, PutAndNextWithReuse) {
  auto entity1 = std::make_unique<EntityData>();
  EntityData* entity1_ptr = entity1.get();
  auto entity2 = std::make_unique<EntityData>();
  EntityData* entity2_ptr = entity2.get();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", std::move(entity1));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("one", key1);
  EXPECT_EQ(entity1_ptr, data1.get());

  batch.Put("two", std::move(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key2, data2] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("two", key2);
  EXPECT_EQ(entity2_ptr, data2.get());
}

TEST(MutableDataBatchTest, PutAndNextInterleaved) {
  auto entity1 = std::make_unique<EntityData>();
  EntityData* entity1_ptr = entity1.get();
  auto entity2 = std::make_unique<EntityData>();
  EntityData* entity2_ptr = entity2.get();
  auto entity3 = std::make_unique<EntityData>();
  EntityData* entity3_ptr = entity3.get();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", std::move(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("two", std::move(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("one", key1);
  EXPECT_EQ(entity1_ptr, data1.get());

  batch.Put("three", std::move(entity3));
  EXPECT_TRUE(batch.HasNext());

  auto [key2, data2] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("two", key2);
  EXPECT_EQ(entity2_ptr, data2.get());

  auto [key3, data3] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("three", key3);
  EXPECT_EQ(entity3_ptr, data3.get());
}

TEST(MutableDataBatchTest, PutAndNextSharedKey) {
  auto entity1 = std::make_unique<EntityData>();
  EntityData* entity1_ptr = entity1.get();
  auto entity2 = std::make_unique<EntityData>();
  EntityData* entity2_ptr = entity2.get();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("same", std::move(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("same", std::move(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("same", key1);
  EXPECT_EQ(entity1_ptr, data1.get());

  auto [key2, data2] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("same", key2);
  EXPECT_EQ(entity2_ptr, data2.get());
}

}  // namespace syncer
