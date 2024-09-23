// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_attached_data.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

// A second implementation, which should not be confused with BarData.
class FooData : public ExternalNodeAttachedDataImpl<FooData> {
 public:
  explicit FooData(const PageNode* page_node) : page_node_(page_node) {}

  ~FooData() override = default;

  raw_ptr<const PageNode> page_node_ = nullptr;

  // Exposes weak pointers to track when the data is deleted.
  base::WeakPtrFactory<FooData> weak_factory_{this};
};

}  // namespace

using NodeAttachedDataTest = GraphTestHarness;

TEST_F(NodeAttachedDataTest, PublicNodeAttachedData) {
  base::WeakPtr<FooData> weak_foo_data;
  base::WeakPtr<FooData> weak_foo_data2;
  {
    MockMultiplePagesInSingleProcessGraph mock_graph(graph());
    const PageNode* page_node = mock_graph.page.get();
    const PageNode* other_page_node = mock_graph.other_page.get();

    BarData* bar_data = BarData::Get(page_node);
    EXPECT_FALSE(bar_data);

    bar_data = BarData::GetOrCreate(page_node);
    EXPECT_TRUE(bar_data);
    EXPECT_EQ(page_node, bar_data->page_node_);

    EXPECT_EQ(bar_data, BarData::Get(page_node));

    // Make sure FooData and BarData are not aliased.
    FooData* foo_data = FooData::Get(page_node);
    EXPECT_FALSE(foo_data);

    foo_data = FooData::GetOrCreate(page_node);
    EXPECT_TRUE(foo_data);
    EXPECT_EQ(page_node, foo_data->page_node_);

    EXPECT_EQ(foo_data, FooData::Get(page_node));

    EXPECT_NE(foo_data->GetKey(), bar_data->GetKey());

    // Make sure data can be stored in multiple nodes.
    FooData* foo_data2 = FooData::Get(other_page_node);
    EXPECT_FALSE(foo_data2);

    foo_data2 = FooData::GetOrCreate(other_page_node);
    EXPECT_TRUE(foo_data2);
    EXPECT_EQ(other_page_node, foo_data2->page_node_);

    EXPECT_EQ(foo_data2, FooData::Get(other_page_node));

    EXPECT_NE(foo_data, foo_data2);
    EXPECT_EQ(foo_data->GetKey(), foo_data2->GetKey());

    // Make sure data can be destroyed.
    EXPECT_TRUE(BarData::Destroy(page_node));
    EXPECT_FALSE(BarData::Destroy(page_node));
    EXPECT_FALSE(BarData::Get(page_node));

    // Data should also be destroyed when the node is deleted.
    weak_foo_data = foo_data->weak_factory_.GetWeakPtr();
    weak_foo_data2 = foo_data->weak_factory_.GetWeakPtr();
    EXPECT_TRUE(weak_foo_data);
    EXPECT_TRUE(weak_foo_data2);
  }

  // Mock graph went out of scope.
  EXPECT_FALSE(weak_foo_data);
  EXPECT_FALSE(weak_foo_data2);
}

}  // namespace performance_manager
