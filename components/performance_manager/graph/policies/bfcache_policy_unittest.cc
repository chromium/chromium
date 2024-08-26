// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/bfcache_policy.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

namespace {

// Mock version of a performance_manager::BFCachePolicy.
class LenientMockBFCachePolicy : public BFCachePolicy {
 public:
  LenientMockBFCachePolicy() = default;
  ~LenientMockBFCachePolicy() override = default;
  LenientMockBFCachePolicy(const LenientMockBFCachePolicy& other) = delete;
  LenientMockBFCachePolicy& operator=(const LenientMockBFCachePolicy&) = delete;
  MOCK_METHOD(void,
              MaybeFlushBFCache,
              (const PageNode* page_node,
               MemoryPressureLevel memory_pressure_level),
              (override));
};
using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;
using MockBFCachePolicy = ::testing::StrictMock<LenientMockBFCachePolicy>;

}  // namespace

class BFCachePolicyTest : public GraphTestHarness {
 public:
  BFCachePolicyTest() = default;
  ~BFCachePolicyTest() override = default;
  BFCachePolicyTest(const BFCachePolicyTest& other) = delete;
  BFCachePolicyTest& operator=(const BFCachePolicyTest&) = delete;

  void OnGraphCreated(GraphImpl* graph) override {
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<MockBFCachePolicy>();
    policy_ = policy.get();
    graph->PassToGraph(std::move(policy));

    page_node_ = CreateNode<performance_manager::PageNodeImpl>();

    // Add 2 main frame nodes to the page and mark one of them as not current to
    // pretend that one of these main frames is in the bfcache.
    process_node_ = CreateNode<ProcessNodeImpl>();
    frame_node_1_ =
        CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
    frame_node_2_ =
        CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
    // TODO: There is 2 current frame nodes and then we change 1 to not be
    // current anymore but that breaks the "only 1 current frame" invariant.
    FrameNodeImpl::UpdateCurrentFrame(frame_node_2_.get(), nullptr, graph);
  }

 protected:
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node_;
  performance_manager::TestNodeWrapper<performance_manager::ProcessNodeImpl>
      process_node_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      frame_node_1_;
  performance_manager::TestNodeWrapper<performance_manager::FrameNodeImpl>
      frame_node_2_;

  raw_ptr<MockBFCachePolicy> policy_;
};

TEST_F(BFCachePolicyTest, BFCacheFlushedOnMemoryPressure) {
  page_node_->SetIsVisible(true);
  page_node_->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  ::testing::Mock::VerifyAndClearExpectations(policy_);

  EXPECT_CALL(
      *policy_,
      MaybeFlushBFCache(page_node_.get(),
                        MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE));
  GetSystemNode()->OnMemoryPressureForTesting(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  ::testing::Mock::VerifyAndClearExpectations(policy_);

  EXPECT_CALL(
      *policy_,
      MaybeFlushBFCache(page_node_.get(),
                        MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL));
  GetSystemNode()->OnMemoryPressureForTesting(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
  ::testing::Mock::VerifyAndClearExpectations(policy_);
}

}  // namespace performance_manager::policies
