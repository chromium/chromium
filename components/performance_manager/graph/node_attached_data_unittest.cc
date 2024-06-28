// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_attached_data.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// An implementation of map-stored user-data using the public interface.
class BarData : public ExternalNodeAttachedDataImpl<BarData> {
 public:
  explicit BarData(const PageNode* page_node) : page_node_(page_node) {}

  ~BarData() override = default;

  raw_ptr<const PageNode> page_node_ = nullptr;
};

}  // namespace

using NodeAttachedDataTest = GraphTestHarness;

TEST_F(NodeAttachedDataTest, PublicNodeAttachedData) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  PageNodeImpl* page_node_impl = mock_graph.page.get();
  const PageNode* page_node = page_node_impl;

  BarData* bar_data = BarData::Get(page_node);
  EXPECT_FALSE(bar_data);

  bar_data = BarData::GetOrCreate(page_node);
  EXPECT_TRUE(bar_data);
  EXPECT_EQ(page_node, bar_data->page_node_);

  EXPECT_EQ(bar_data, BarData::Get(page_node));

  EXPECT_TRUE(BarData::Destroy(page_node));
  EXPECT_FALSE(BarData::Destroy(page_node));
  EXPECT_FALSE(BarData::Get(page_node));
}

}  // namespace performance_manager
