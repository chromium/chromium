// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mutable_data_batch.h"

#include "base/memory/ptr_util.h"
#include "components/sync/engine/entity_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(MutableDataBatchTest, PutAndNextWithReuse) {
  EntityData* entity1 = new EntityData();
  EntityData* entity2 = new EntityData();

  MutableDataBatch batch;
  EXPECT_FALSE(batch.HasNext());

  batch.Put("one", base::WrapUnique(entity1));
  EXPECT_TRUE(batch.HasNext());

  const KeyAndData& pair1 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("one", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  batch.Put("two", base::WrapUnique(entity2));
  EXPECT_TRUE(batch.HasNext());

  const KeyAndData& pair2 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("two", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());
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

  const KeyAndData& pair1 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("one", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  batch.Put("three", base::WrapUnique(entity3));
  EXPECT_TRUE(batch.HasNext());

  const KeyAndData& pair2 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("two", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());

  const KeyAndData& pair3 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("three", pair3.first);
  EXPECT_EQ(entity3, pair3.second.get());
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

  const KeyAndData& pair1 = batch.Next();
  EXPECT_TRUE(batch.HasNext());
  EXPECT_EQ("same", pair1.first);
  EXPECT_EQ(entity1, pair1.second.get());

  const KeyAndData& pair2 = batch.Next();
  EXPECT_FALSE(batch.HasNext());
  EXPECT_EQ("same", pair2.first);
  EXPECT_EQ(entity2, pair2.second.get());
}

}  // namespace syncer
