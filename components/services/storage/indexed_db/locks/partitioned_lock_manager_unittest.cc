// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include "base/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(PartitionedLockManager, TestIdPopulation) {
  PartitionedLockId lock_id = {1, "2"};
  EXPECT_EQ(1, lock_id.partition);
  EXPECT_EQ("2", lock_id.key);
}

}  // namespace
}  // namespace content
