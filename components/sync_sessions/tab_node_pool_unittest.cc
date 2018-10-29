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

using testing::UnorderedElementsAre;

const int kTabNodeId1 = 10;
const int kTabNodeId2 = 5;
const int kTabNodeId3 = 30;
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

TEST_F(SyncTabNodePoolTest, TabNodeIdIncreases) {
  std::set<int> deleted_node_ids;

  // max_used_tab_node_ always increases.
  pool_.ReassociateTabNode(kTabNodeId1, kTabId1);
  EXPECT_EQ(kTabNodeId1, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(kTabNodeId2, kTabId2);
  EXPECT_EQ(kTabNodeId1, GetMaxUsedTabNodeId());
  pool_.ReassociateTabNode(kTabNodeId3, kTabId3);
  EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
  // Freeing a tab node does not change max_used_tab_node_id_.
  pool_.FreeTab(kTabId3);
  pool_.CleanupTabNodes(&deleted_node_ids);
  pool_.FreeTab(kTabId2);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  pool_.FreeTab(kTabId1);
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  for (int i = 0; i < 3; ++i) {
    const SessionID tab_id = SessionID::FromSerializedValue(i + 1);
    ASSERT_EQ(TabNodePool::kInvalidTabNodeID,
              pool_.GetTabNodeIdFromTabId(tab_id));
    EXPECT_NE(TabNodePool::kInvalidTabNodeID,
              pool_.AssociateWithFreeTabNode(tab_id));
    EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
  }
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_TRUE(deleted_node_ids.empty());
  EXPECT_EQ(kTabNodeId3, GetMaxUsedTabNodeId());
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
  std::set<int> deleted_node_ids;

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

TEST_F(SyncTabNodePoolTest, TabPoolFreeNodeLimits) {
  std::set<int> deleted_node_ids;

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
    pool_.CleanupTabNodes(&deleted_node_ids);
    EXPECT_TRUE(deleted_node_ids.empty());
  }

  // Freeing the last sync node should drop the free nodes to
  // kFreeNodesLowWatermark.
  pool_.FreeTab(
      SessionID::FromSerializedValue(TabNodePool::kFreeNodesHighWatermark + 1));
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_EQ(TabNodePool::kFreeNodesHighWatermark + 1 -
                TabNodePool::kFreeNodesLowWatermark,
            deleted_node_ids.size());
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

TEST_F(SyncTabNodePoolTest, AggressiveCleanupTabNodesMiddle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kTabNodePoolImmediateDeletion);

  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId2);

  std::set<int> deleted_node_ids;
  pool_.CleanupTabNodes(&deleted_node_ids);

  EXPECT_THAT(deleted_node_ids, UnorderedElementsAre(1));
  EXPECT_EQ(2, GetMaxUsedTabNodeId());
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupTabNodesMax) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kTabNodePoolImmediateDeletion);

  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId3);

  std::set<int> deleted_node_ids;
  pool_.CleanupTabNodes(&deleted_node_ids);

  EXPECT_THAT(deleted_node_ids, UnorderedElementsAre(2));
  EXPECT_EQ(1, GetMaxUsedTabNodeId());
  EXPECT_EQ(2, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId5));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupTabNodesMultiple) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kTabNodePoolImmediateDeletion);

  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);
  pool_.ReassociateTabNode(/*tab_node_id=*/1, kTabId2);
  pool_.ReassociateTabNode(/*tab_node_id=*/2, kTabId3);

  pool_.FreeTab(kTabId1);
  pool_.FreeTab(kTabId2);

  std::set<int> deleted_node_ids;
  pool_.CleanupTabNodes(&deleted_node_ids);

  EXPECT_THAT(deleted_node_ids, UnorderedElementsAre(0, 1));
  EXPECT_EQ(2, GetMaxUsedTabNodeId());
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId4));
  EXPECT_EQ(1, pool_.AssociateWithFreeTabNode(kTabId5));
  EXPECT_EQ(3, pool_.AssociateWithFreeTabNode(kTabId6));
}

TEST_F(SyncTabNodePoolTest, AggressiveCleanupTabNodesAll) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kTabNodePoolImmediateDeletion);

  pool_.ReassociateTabNode(/*tab_node_id=*/0, kTabId1);

  pool_.FreeTab(kTabId1);

  std::set<int> deleted_node_ids;
  pool_.CleanupTabNodes(&deleted_node_ids);
  EXPECT_THAT(deleted_node_ids, UnorderedElementsAre(0));
  EXPECT_EQ(-1, GetMaxUsedTabNodeId());
  EXPECT_EQ(0, pool_.AssociateWithFreeTabNode(kTabId4));
}

}  // namespace

}  // namespace sync_sessions
