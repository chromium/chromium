// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/frozen_frame_aggregator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using LifecycleState = PageNodeImpl::LifecycleState;

}  // namespace

class FrozenFrameAggregatorTest : public GraphTestHarness {
 public:
  FrozenFrameAggregatorTest(const FrozenFrameAggregatorTest&) = delete;
  FrozenFrameAggregatorTest& operator=(const FrozenFrameAggregatorTest&) =
      delete;

 protected:
  using Super = GraphTestHarness;

  FrozenFrameAggregatorTest() = default;
  ~FrozenFrameAggregatorTest() override = default;

  void SetUp() override {
    Super::SetUp();
    ffa_ = new FrozenFrameAggregator();
    graph()->PassToGraph(base::WrapUnique(ffa_.get()));
    process_node_ = CreateNode<ProcessNodeImpl>();
    page_node_ = CreateNode<PageNodeImpl>();
  }

  void ExpectPageData(uint32_t active_frame_count,
                      uint32_t frozen_frame_count) {
    FrozenData& data = FrozenData::Get(page_node_.get());
    EXPECT_EQ(active_frame_count, data.active_frame_count());
    EXPECT_EQ(frozen_frame_count, data.frozen_frame_count());
  }

  void ExpectRunning() {
    EXPECT_EQ(LifecycleState::kRunning, page_node_->GetLifecycleState());
  }

  void ExpectFrozen() {
    EXPECT_EQ(LifecycleState::kFrozen, page_node_->GetLifecycleState());
  }

  TestNodeWrapper<FrameNodeImpl> CreateFrame(FrameNodeImpl* parent_frame_node,
                                             bool is_active = true) {
    return TestNodeWrapper<FrameNodeImpl>::Create(
        graph(), process_node_.get(), page_node_.get(), parent_frame_node,
        /*outer_document_for_fenced_frame=*/nullptr, NextTestFrameRoutingId(),
        blink::LocalFrameToken(), NextTestFrameTreeNodeId(),
        content::BrowsingInstanceId(), content::SiteInstanceGroupId(),
        is_active);
  }

  raw_ptr<FrozenFrameAggregator> ffa_;
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
};

TEST_F(FrozenFrameAggregatorTest, NotActive) {
  ExpectPageData(0, 0);

  // Add a non-active main frame.
  auto f0 = CreateFrame(nullptr, /*is_active=*/false);
  ExpectPageData(0, 0);

  // Make it active. The frame starts being counted.
  f0->SetIsActive(true);
  ExpectPageData(1, 0);

  f0->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(1, 1);

  // Make no longer active. Stops being counted.
  f0->SetIsActive(false);
  ExpectPageData(0, 0);
}

TEST_F(FrozenFrameAggregatorTest, PageAggregation) {
  ExpectPageData(0, 0);
  ExpectRunning();

  // Add an active frame.
  auto f0 = CreateFrame(nullptr);
  ExpectPageData(1, 0);
  ExpectRunning();

  // Freeze the frame.
  f0->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(1, 1);
  ExpectFrozen();

  // Unfreeze the frame.
  f0->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(1, 0);
  ExpectRunning();

  // Add a child frame.
  auto f1 = CreateFrame(f0.get());
  ExpectPageData(2, 0);
  ExpectRunning();

  // Freeze them both.
  f1->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 1);
  ExpectRunning();
  f0->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 2);
  ExpectFrozen();

  // Unfreeze them both.
  f0->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(2, 1);
  ExpectRunning();
  f1->SetLifecycleState(LifecycleState::kRunning);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Create a third frame that is not active.
  auto f1a = CreateFrame(f0.get(), /*is_active=*/false);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Swap f1 and f1a.
  f1a->SetIsActive(true);
  f1->SetIsActive(false);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Freeze the original frame and swap it back.
  f1->SetLifecycleState(LifecycleState::kFrozen);
  f1->SetIsActive(true);
  f1a->SetIsActive(false);
  ExpectPageData(2, 1);
  ExpectRunning();

  // Freeze the non-active frame and expect nothing to change.
  f1a->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 1);
  ExpectRunning();

  // Remove the non-active frame and expect nothing to change.
  f1a.reset();
  ExpectPageData(2, 1);
  ExpectRunning();

  // Remove the frozen child frame and expect a change.
  f1.reset();
  ExpectPageData(1, 0);
  ExpectRunning();

  // Freeze the main frame again.
  f0->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(1, 1);
  ExpectFrozen();

  // Remove the main frame. An empty page is always considered as "running".
  f0.reset();
  ExpectPageData(0, 0);
  ExpectRunning();
}

}  // namespace performance_manager
