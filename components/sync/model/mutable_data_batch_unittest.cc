// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mutable_data_batch.h"

#include "base/memory/ptr_util.h"
#include "components/sync/protocol/entity_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(MutableDataBatchTest, PutAndNextWithReuse) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("one", key1);
  EXPECT_EQ(entity1, data1.get());

  batch.Put("two", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key2, data2] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("two", key2);
  EXPECT_EQ(entity2, data2.get());
}

TEST(MutableDataBatchTest, PutAndNextInterleaved) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();
  EntityData* entity3 = new EntityData();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("two", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("one", key1);
  EXPECT_EQ(entity1, data1.get());

  batch.Put("three", base::WrapUnique(entity3));
  EXPECT_TRUE(batch.HasNext());

  auto [key2, data2] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("two", key2);
  EXPECT_EQ(entity2, data2.get());

  auto [key3, data3] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("three", key3);
  EXPECT_EQ(entity3, data3.get());
}

TEST(MutableDataBatchTest, PutAndNextSharedKey) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("same", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());
  batch.Put("same", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  auto [key1, data1] = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("same", key1);
  EXPECT_EQ(entity1, data1.get());

  auto [key2, data2] = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("same", key2);
  EXPECT_EQ(entity2, data2.get());
}

}  // namespace syncer
