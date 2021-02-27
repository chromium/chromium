// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_attached_data.h"

#include <utility>

#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Note that a FooData basically has pointer size, because its an empty class
// with a single vtable pointer.
constexpr size_t kFooDataSize = sizeof(uintptr_t);

// A dummy node type so that we can exercise node attached storage code paths.
class DummyNode : public NodeBase, public Node {
 public:
  DummyNode() : NodeBase(NodeTypeEnum::kInvalidType) {}

  ~DummyNode() override = default;

  // NodeBase implementation:
  const Node* ToNode() const override { return static_cast<const Node*>(this); }

  // Node implementation:
  Graph* GetGraph() const override { return graph(); }
  uintptr_t GetImplType() const override {
    static const uintptr_t kImplType = reinterpret_cast<uintptr_t>(&kImplType);
    return kImplType;
  }
  const void* GetImpl() const override {
    return static_cast<const NodeBase*>(this);
  }

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kInvalidType; }

  // Internal storage for DummyData and FooData types. These would normally be
  // protected and the data classes friended, but we also want to access these
  // in the tests.
  std::unique_ptr<NodeAttachedData> dummy_data_;
  InternalNodeAttachedDataStorage<kFooDataSize> foo_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyNode);
};

// A NodeAttachedData class that can only be attached to page and process nodes
// using the map, and to DummyNodes using node owned storage.
class DummyData : public NodeAttachedDataImpl<DummyData> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl>,
                  public NodeAttachedDataInMap<ProcessNodeImpl>,
                  public NodeAttachedDataOwnedByNodeType<DummyNode> {};

  DummyData() = default;
  explicit DummyData(const PageNodeImpl* page_node) {}
  explicit DummyData(const ProcessNodeImpl* process_node) {}
  explicit DummyData(const DummyNode* dummy_node) {}
  ~DummyData() override = default;

  // Provides access to storage on DummyNodes.
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      DummyNode* dummy_node) {
    return &dummy_node->dummy_data_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyData);
};

// Another NodeAttachedData class that can only be attached to page nodes in the
// map, and to DummyNodes using node internal storage.
class FooData : public NodeAttachedDataImpl<FooData> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl>,
                  public NodeAttachedDataInternalOnNodeType<DummyNode> {};

  FooData() = default;
  explicit FooData(const PageNodeImpl* page_node) {}
  explicit FooData(const DummyNode* dummy_node) {}
  ~FooData() override = default;

  // Provides access to storage on DummyNodes.
  static InternalNodeAttachedDataStorage<kFooDataSize>* GetInternalStorage(
      DummyNode* dummy_node) {
    return &dummy_node->foo_data_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FooData);
};

// An implementation of map-stored user-data using the public interface.
class BarData : public ExternalNodeAttachedDataImpl<BarData> {
 public:
  explicit BarData(const PageNode* page_node) : page_node_(page_node) {}

  ~BarData() override = default;

  const PageNode* page_node_ = nullptr;
};

}  // namespace

class NodeAttachedDataTest : public GraphTestHarness {
 public:
  NodeAttachedDataTest() = default;
  ~NodeAttachedDataTest() override = default;

  static void AttachInMap(const Node* node,
                          std::unique_ptr<NodeAttachedData> data) {
    return NodeAttachedDataMapHelper::AttachInMap(node, std::move(data));
  }

  // Retrieves the data associated with the provided |node| and |key|. This
  // returns nullptr if no data exists.
  static NodeAttachedData* GetFromMap(const Node* node, const void* key) {
    return NodeAttachedDataMapHelper::GetFromMap(node, key);
  }

  // Detaches the data associated with the provided |node| and |key|. It is
  // harmless to call this when no data exists.
  static std::unique_ptr<NodeAttachedData> DetachFromMap(const Node* node,
                                                         const void* key) {
    return NodeAttachedDataMapHelper::DetachFromMap(node, key);
  }
};

TEST_F(NodeAttachedDataTest, UserDataKey) {
  std::unique_ptr<NodeAttachedData> data = std::make_unique<DummyData>();
  EXPECT_EQ(data->GetKey(), DummyData::UserDataKey());
}

TEST_F(NodeAttachedDataTest, RawAttachDetach) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  static constexpr NodeAttachedData* kNull = nullptr;

  std::unique_ptr<DummyData> data = std::make_unique<DummyData>();
  auto* raw_data = data.get();
  auto* raw_base = static_cast<NodeAttachedData*>(raw_data);

  // No data yet exists.
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(kNull, GetFromMap(page_node, raw_data->GetKey()));

  // Attach the data and look it up again.
  AttachInMap(page_node, std::move(data));
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(raw_base, GetFromMap(page_node, raw_data->GetKey()));

  // Detach the data.
  std::unique_ptr<NodeAttachedData> base_data =
      DetachFromMap(page_node, raw_data->GetKey());
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(raw_base, base_data.get());
  EXPECT_EQ(kNull, GetFromMap(page_node, raw_data->GetKey()));
}

TEST_F(NodeAttachedDataTest, TypedAttachDetach) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  static constexpr DummyData* kNull = nullptr;

  // Lookup non-existent data.
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(kNull, DummyData::Get(page_node));

  // Create the data and look it up again.
  auto* data = DummyData::GetOrCreate(page_node);
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_NE(kNull, data);
  EXPECT_EQ(data, DummyData::Get(page_node));

  // Destroy the data and expect it to no longer exist.
  EXPECT_TRUE(DummyData::Destroy(page_node));
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(kNull, DummyData::Get(page_node));
}

TEST_F(NodeAttachedDataTest, NodeDeathDestroysData) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();
  ProcessNodeImpl* proc_node = mock_graph.process.get();

  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(
                    page_node, DummyData::UserDataKey()));
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(
                    proc_node, DummyData::UserDataKey()));

  // Add data to both the process and the page.
  DummyData::GetOrCreate(page_node);
  FooData::GetOrCreate(page_node);
  auto* proc_data = DummyData::GetOrCreate(proc_node);
  EXPECT_EQ(3u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(2u,
            graph()->GetNodeAttachedDataCountForTesting(page_node, nullptr));
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(
                    proc_node, DummyData::UserDataKey()));

  // Release the page node and expect the node attached data to have been
  // cleaned up.
  mock_graph.frame.reset();
  mock_graph.page.reset();
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(0u,
            graph()->GetNodeAttachedDataCountForTesting(page_node, nullptr));
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(
                    proc_node, DummyData::UserDataKey()));

  // The process data should still exist.
  EXPECT_EQ(proc_data, DummyData::Get(proc_node));
}

TEST_F(NodeAttachedDataTest, NodeOwnedStorage) {
  DummyNode dummy_node;

  // The storage in the dummy node should not be initialized.
  EXPECT_FALSE(dummy_node.dummy_data_.get());
  EXPECT_FALSE(DummyData::Get(&dummy_node));

  // Creating a DummyData on the DummyNode should not create storage in the map.
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  DummyData* dummy_data = DummyData::GetOrCreate(&dummy_node);
  NodeAttachedData* nad = static_cast<NodeAttachedData*>(dummy_data);
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));

  // The storage in the dummy node should now be initialized.
  EXPECT_EQ(nad, dummy_node.dummy_data_.get());
  EXPECT_EQ(dummy_data, DummyData::Get(&dummy_node));

  // Destroying the data should free up the storage.
  DummyData::Destroy(&dummy_node);
  EXPECT_FALSE(dummy_node.dummy_data_.get());
  EXPECT_FALSE(DummyData::Get(&dummy_node));
}

TEST_F(NodeAttachedDataTest, NodeInternalStorage) {
  DummyNode dummy_node;

  // The storage in the dummy node should not be initialized.
  EXPECT_FALSE(dummy_node.foo_data_.Get());
  EXPECT_FALSE(FooData::Get(&dummy_node));

  // Creating a FooData on the DummyNode should not create storage in the map.
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  FooData* foo_data = FooData::GetOrCreate(&dummy_node);
  NodeAttachedData* nad = static_cast<NodeAttachedData*>(foo_data);
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));

  // The storage in the dummy node should now be initialized.
  EXPECT_EQ(nad, dummy_node.foo_data_.Get());
  EXPECT_EQ(foo_data, FooData::Get(&dummy_node));

  // Destroying the data should free up the storage.
  FooData::Destroy(&dummy_node);
  EXPECT_FALSE(dummy_node.foo_data_.Get());
  EXPECT_FALSE(FooData::Get(&dummy_node));
}

TEST_F(NodeAttachedDataTest, PublicNodeAttachedData) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  PageNodeImpl* page_node_impl = mock_graph.page.get();
  const PageNode* page_node = page_node_impl;

  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  BarData* bar_data = BarData::Get(page_node);
  EXPECT_FALSE(bar_data);

  bar_data = BarData::GetOrCreate(page_node);
  EXPECT_TRUE(bar_data);
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_EQ(1u, graph()->GetNodeAttachedDataCountForTesting(
                    page_node_impl, bar_data->GetKey()));
  EXPECT_EQ(page_node, bar_data->page_node_);

  EXPECT_EQ(bar_data, BarData::Get(page_node));

  EXPECT_TRUE(BarData::Destroy(page_node));
  EXPECT_EQ(0u, graph()->GetNodeAttachedDataCountForTesting(nullptr, nullptr));
  EXPECT_FALSE(BarData::Destroy(page_node));
  EXPECT_FALSE(BarData::Get(page_node));
}

}  // namespace performance_manager
