// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include "base/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(PartitionedLockManager, TestRangePopulation) {
  PartitionedLockRange range = {"1", "2"};
  EXPECT_EQ("1", range.begin);
  EXPECT_EQ("2", range.end);
  EXPECT_TRUE(range.IsValid());
}

TEST(PartitionedLockManager, TestInvalidRange) {
  PartitionedLockRange range = {"2", "1"};
  EXPECT_FALSE(range.IsValid());
  range = {"2", "2"};
  EXPECT_FALSE(range.IsValid());
}

}  // namespace
}  // namespace content
