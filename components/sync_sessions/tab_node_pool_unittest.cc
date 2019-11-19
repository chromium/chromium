// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/tab_node_pool.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

const int kTabNodeId1 = 10;
const int kTabNodeId2 = 5;
const SessionID kTabId1 = SessionID::FromSerializedValue(1010);
const SessionID kTabId2 = SessionID::FromSerializedValue(1020);
const SessionID kTabId3 = SessionID::FromSerializedValue(1030);
const SessionID kTabId4 = SessionID::FromSerializedValue(1040);
const SessionID kTabId5 = SessionID::FromSerializedValue(1050);
const SessionID kTabId6 = SessionID::FromSerializedValue(1060);

class SyncTabNodePoolTest : public testing::Test {
 protected:
  SyncTabNodePoolTest() {}

  int GetMaxUsedTabNodeId() const { return pool_.GetMaxUsedTabNodeIdForTest(); }

  void AddFreeTabNodes(const std::vector<int>& node_ids) {
    const SessionID kTmpTabId = SessionID::FromSerializedValue(123);
    for (int node_id : node_ids) {
      pool_.ReassociateTabNode(node_id, kTmpTabId);
      pool_.FreeTab(kTmpTabId);
    }
  }

  TabNodePool pool_;
};

TEST_F(SyncTabNodePoolTest, MaxTabNodeIdShouldIncrease) {
  EXPECT_EQ(-1, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(10, kTabId1);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(5, kTabId2);
  EXPECT_EQ(10, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(20, kTabId3);
  EXPECT_EQ(20, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePoolTest, MaxTabNodeIdShouldDecrease) {
  pool_.ReassociateTabNode(10, kTabId1);
  pool_.ReassociateTabNode(5, kTabId2);
  pool_.ReassociateTabNode(20, kTabId3);
  EXPECT_EQ(20, GetMaxUsedTabNodeId());

  pool_.FreeTab(kTabId3);
  ASSERT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(20));
  EXPECT_EQ(10, GetMaxUsedTabNodeId());

  pool_.FreeTab(kTabId1);
  ASSERT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(10));
  EXPECT_EQ(5, GetMaxUsedTabNodeId());

  pool_.FreeTab(kTabId2);
  ASSERT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(5));
  EXPECT_EQ(-1, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePoolTest, MaxTabNodeIdShouldNotChange) {
  pool_.ReassociateTabNode(10, kTabId1);
  pool_.ReassociateTabNode(5, kTabId2);
  pool_.ReassociateTabNode(20, kTabId3);
  EXPECT_EQ(20, GetMaxUsedTabNodeId());

  pool_.FreeTab(kTabId1);
  ASSERT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(10));
  EXPECT_EQ(20, GetMaxUsedTabNodeId());

  pool_.FreeTab(kTabId2);
  ASSERT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(5));
  EXPECT_EQ(20, GetMaxUsedTabNodeId());
}

TEST_F(SyncTabNodePoolTest, Reassociation) {
  // Reassociate tab node 1 with tab id 1.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_FALSE(pool_.GetTabIdFromTabNodeId(kTabNodeId2).is_valid());

  // Introduce a new tab node associated with the same tab. The old tab node
  // should get added to the free pool
  pool_.ReassociateTabNode(kTabNodeId2, kTabId1);
  EXPECT_FALSE(pool_.GetTabIdFromTabNodeId(kTabNodeId1).is_valid());
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the same tab node/tab should have no effect.
  pool_.ReassociateTabNode(kTabNodeId2, kTabId1);
  EXPECT_FALSE(pool_.GetTabIdFromTabNodeId(kTabNodeId1).is_valid());
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the new tab node with a new tab should just update the
  // association tables.
  pool_.ReassociateTabNode(kTabNodeId2, kTabId2);
  EXPECT_FALSE(pool_.GetTabIdFromTabNodeId(kTabNodeId1).is_valid());
  EXPECT_EQ(kTabId2, pool_.GetTabIdFromTabNodeId(kTabNodeId2));

  // Reassociating the first tab node should make the pool empty.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(kTabId1, pool_.GetTabIdFromTabNodeId(kTabNodeId1));
  EXPECT_EQ(kTabId2, pool_.GetTabIdFromTabNodeId(kTabNodeId2));
}

TEST_F(SyncTabNodePoolTest, ReassociateThenFree) {
  // Verify old tab nodes are reassociated correctly.
  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);
  // Free tabs 2 and 3.
  pool_.FreeTab(kTabId2);
  pool_.FreeTab(kTabId3);

  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId2));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId3));
  EXPECT_NE(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId1));

  // Free node pool should have 1 (for kTabId2) and 2 (for kTabId3).
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(2, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AssociateWithFreeTabNode) {
  ASSERT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId1));
  ASSERT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId2));
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId1));
  EXPECT_EQ(0, pool_.GetTabNodeIdFromTabId(kTabId1));
  ASSERT_EQ(TabNodePool::kInvalidTabNodeID,
            pool_.GetTabNodeIdFromTabId(kTabId2));
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId2));
  EXPECT_EQ(1, pool_.GetTabNodeIdFromTabId(kTabId2));
  pool_.FreeTab(kTabId1);
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId3));
}

TEST_F(SyncTabNodePoolTest, TabPoolFreeNodeWatermarkLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kTabNodePoolImmediateDeletion);

  // Allocate TabNodePool::kFreeNodesHighWatermark + 1 nodes and verify that
  // freeing the last node reduces the free node pool size to
  // kFreeNodesLowWatermark.
  std::vector<int> used_sync_ids;
  for (size_t i = 1; i <= TabNodePool::kFreeNodesHighWatermark + 1; ++i) {
    used_sync_ids.push_back(
        pool_.AssociateWithFreeTabNode(SessionID::FromSerializedValue(i)));
  }

  // Free all except one node.
  used_sync_ids.pop_back();

  for (size_t i = 1; i <= used_sync_ids.size(); ++i) {
    pool_.FreeTab(SessionID::FromSerializedValue(i));
    EXPECT_THAT(pool_.CleanupFreeTabNodes(), IsEmpty());
  }

  // Freeing the last sync node should drop the free nodes to
  // kFreeNodesLowWatermark.
  pool_.FreeTab(
      SessionID::FromSerializedValue(TabNodePool::kFreeNodesHighWatermark + 1));
  std::set<int> deleted_node_ids = pool_.CleanupFreeTabNodes();
  EXPECT_EQ(deleted_node_ids.size(), TabNodePool::kFreeNodesHighWatermark + 1 -
                                         TabNodePool::kFreeNodesLowWatermark);
  // Make sure the highest ones are deleted.
  EXPECT_EQ(0U,
            deleted_node_ids.count(TabNodePool::kFreeNodesLowWatermark - 1));
  EXPECT_NE(0U, deleted_node_ids.count(TabNodePool::kFreeNodesLowWatermark));
  EXPECT_NE(0U, deleted_node_ids.count(TabNodePool::kFreeNodesHighWatermark));
}

TEST_F(SyncTabNodePoolTest, AssociateWithFreeTabNodesContiguous) {
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId1);
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId2));
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId3));
  // Tab node 2 is already used, so it should be skipped.
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId4));
}

// Tests that, when *both* a free tab node and a "hole" exists,
// AssociateWithFreeTabNode() returns the smallest of them.
TEST_F(SyncTabNodePoolTest, AssociateWithFreeTabNodeReturnsMinimum) {
  // Set up the pool such that tab node 1 is freed, and nodes 0 and 2 are holes
  // (missing).
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/3, kTabId2);
  pool_.FreeTab(kTabId1);

  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId3));
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(2, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupFreeTabNodesMiddle) {
  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId2);

  EXPECT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(1));
  EXPECT_EQ(2, GetMaxUsedTabNodeId());
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupFreeTabNodesMax) {
  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId3);

  EXPECT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(2));
  EXPECT_EQ(1, GetMaxUsedTabNodeId());
  EXPECT_EQ(2, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupFreeTabNodesMultiple) {
  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId1);
  pool_.FreeTab(kTabId2);

  EXPECT_THAT(pool_.CleanupFreeTabNodes(), UnorderedElementsAre(0, 1));
  EXPECT_EQ(2, GetMaxUsedTabNodeId());
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId5));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId6));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupFreeTabNodesAll) {
  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);

  pool_.FreeTab(kTabId1);

  EXPECT_THAT(pool_.CleanupFreeTabNodes(), ElementsAre(0));
  EXPECT_EQ(-1, GetMaxUsedTabNodeId());
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId4));
}

}  // namespace

}  // namespace sync_sessions
