// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/rts_receiver.h"

#include <cstdint>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"
#include "chromeos/ash/components/data_migration/testing/connection_barrier.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_connections.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_process_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_migration {
namespace {

constexpr char kRemoteEndpointId[] = "test-remote-endpoint";

class RtsReceiverTest : public testing::Test {
 protected:
  RtsReceiverTest()
      : nearby_process_manager_(kRemoteEndpointId),
        nearby_connections_manager_(&nearby_process_manager_, kServiceId) {}

  void SetUp() override {
    ConnectionBarrier connection_barrier(&nearby_connections_manager_);
    NearbyConnection* nearby_connection = connection_barrier.Wait();
    ASSERT_TRUE(nearby_connection);
    rts_receiver_.emplace(nearby_connection, &pending_file_transfer_queue_);
  }

  base::test::TaskEnvironment task_environment_;
  FakeNearbyProcessManager nearby_process_manager_;
  NearbyConnectionsManagerImpl nearby_connections_manager_;
  PendingFileTransferQueue pending_file_transfer_queue_;
  std::optional<RtsReceiver> rts_receiver_;
};

TEST_F(RtsReceiverTest, PushesPendingFilesToQueue) {
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/1, /*file payload id*/ "100"));
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/2, /*file payload id*/ "200"));
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/3, /*file payload id*/ "300"));

  base::test::TestFuture<int64_t> future;
  pending_file_transfer_queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), 100);

  future.Clear();
  pending_file_transfer_queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), 200);

  future.Clear();
  pending_file_transfer_queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), 300);

  future.Clear();
  pending_file_transfer_queue_.Pop(future.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(RtsReceiverTest, IgnoresMalformedBytes) {
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/1, "invalid-file-payload-id"));

  base::test::TestFuture<int64_t> future;
  pending_file_transfer_queue_.Pop(future.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());

  // Should keep receiving after receiving a malformed RTS.
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/2, /*file payload id*/ "200"));
  EXPECT_EQ(future.Get<0>(), 200);
}

TEST_F(RtsReceiverTest, HandlesDisconnectFromRemoteDevice) {
  ASSERT_TRUE(
      nearby_process_manager_.fake_nearby_connections().SendBytesPayload(
          /*payload_id=*/1, /*file payload id*/ "100"));

  base::test::TestFuture<int64_t> future;
  pending_file_transfer_queue_.Pop(future.GetCallback());
  EXPECT_EQ(future.Get<0>(), 100);

  ASSERT_TRUE(nearby_process_manager_.fake_nearby_connections()
                  .SimulateRemoteDisconnect());

  future.Clear();
  pending_file_transfer_queue_.Pop(future.GetCallback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

}  // namespace
}  // namespace data_migration
