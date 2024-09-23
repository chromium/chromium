// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"

#include <cstdint>

#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_migration {

class PendingFileTransferQueueTest : public testing::Test {
 protected:
  PendingFileTransferQueue queue_;
};

TEST_F(PendingFileTransferQueueTest, PopWithItemsAlreadyInQueue) {
  queue_.Push(1);
  queue_.Push(2);

  base::test::TestFuture<int64_t> future;
  queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get(), 1);

  future.Clear();
  queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get(), 2);
}

TEST_F(PendingFileTransferQueueTest, PopWithNoItemsInQueue) {
  base::test::TestFuture<int64_t> future;
  queue_.Pop(future.GetCallback());

  queue_.Push(1);
  EXPECT_EQ(future.Get(), 1);

  future.Clear();
  queue_.Pop(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
}

}  // namespace data_migration
