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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using LifecycleState = PageNodeImpl::LifecycleState;

class LenientMockProcessNodeObserver : public ProcessNode::ObserverDefaultImpl {
 public:
  LenientMockProcessNodeObserver() = default;

  LenientMockProcessNodeObserver(const LenientMockProcessNodeObserver&) =
      delete;
  LenientMockProcessNodeObserver& operator=(
      const LenientMockProcessNodeObserver&) = delete;

  ~LenientMockProcessNodeObserver() override = default;

  MOCK_METHOD(void,
              OnAllFramesInProcessFrozen,
              (const ProcessNode*),
              (override));
};

using MockProcessNodeObserver =
    ::testing::StrictMock<LenientMockProcessNodeObserver>;

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

  template <typename NodeType>
  void ExpectData(NodeType* node,
                  uint32_t current_frame_count,
                  uint32_t frozen_frame_count) {
    FrozenData& data = FrozenData::Get(node);
    EXPECT_EQ(current_frame_count, data.current_frame_count());
    EXPECT_EQ(frozen_frame_count, data.frozen_frame_count());
  }

  void ExpectPageData(uint32_t current_frame_count,
                      uint32_t frozen_frame_count) {
    ExpectData(page_node_.get(), current_frame_count, frozen_frame_count);
  }

  void ExpectProcessData(uint32_t current_frame_count,
                         uint32_t frozen_frame_count) {
    ExpectData(process_node_.get(), current_frame_count, frozen_frame_count);
  }

  void ExpectRunning() {
    EXPECT_EQ(LifecycleState::kRunning, page_node_->GetLifecycleState());
  }

  void ExpectFrozen() {
    EXPECT_EQ(LifecycleState::kFrozen, page_node_->GetLifecycleState());
  }

  TestNodeWrapper<FrameNodeImpl> CreateFrame(FrameNodeImpl* parent_frame_node,
                                             bool is_current = true) {
    return TestNodeWrapper<FrameNodeImpl>::Create(
        graph(), process_node_.get(), page_node_.get(), parent_frame_node,
        /*outer_document_for_fenced_frame=*/nullptr, NextTestFrameRoutingId(),
        blink::LocalFrameToken(), content::BrowsingInstanceId(),
        content::SiteInstanceGroupId(), is_current);
  }

  raw_ptr<FrozenFrameAggregator> ffa_;
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
};

TEST_F(FrozenFrameAggregatorTest, NotCurrent) {
  ExpectProcessData(0, 0);

  // Add a non-current main frame.
  auto f0 = CreateFrame(nullptr, /*is_current=*/false);
  ExpectProcessData(0, 0);

  // Make it current. The frame starts being counted.
  FrameNodeImpl::UpdateCurrentFrame(/*previous_frame_node=*/nullptr,
                                    /*current_frame_node=*/f0.get(), graph());
  ExpectProcessData(1, 0);

  f0->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(1, 1);

  // Make no longer current. Stops being counted.
  FrameNodeImpl::UpdateCurrentFrame(/*previous_frame_node=*/f0.get(),
                                    /*current_frame_node=*/nullptr, graph());
  ExpectProcessData(0, 0);
}

TEST_F(FrozenFrameAggregatorTest, ProcessAggregation) {
  MockProcessNodeObserver obs;
  graph()->AddProcessNodeObserver(&obs);

  ExpectProcessData(0, 0);

  // Add a main frame.
  auto f0 = CreateFrame(nullptr);
  ExpectProcessData(1, 0);

  // Make the frame frozen and expect a notification.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(process_node_.get()));
  f0->SetLifecycleState(LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&obs);
  ExpectProcessData(1, 1);

  // Create another process and another page.
  auto proc2 = CreateNode<ProcessNodeImpl>();
  auto page2 = CreateNode<PageNodeImpl>();
  ExpectProcessData(1, 1);

  // Create a child frame for the first page hosted in the second process.
  auto f1 = CreateFrameNodeAutoId(proc2.get(), page_node_.get(), f0.get());
  ExpectProcessData(1, 1);

  // Freeze the child frame and expect |proc2| to receive an event, but not
  // |process_node_|.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(proc2.get()));
  f1->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(1, 1);

  // Unfreeze both frames.
  f0->SetLifecycleState(LifecycleState::kRunning);
  ExpectProcessData(1, 0);
  f1->SetLifecycleState(LifecycleState::kRunning);
  ExpectProcessData(1, 0);

  // Create a main frame in the second page, but that's in the first process.
  auto f2 = CreateFrameNodeAutoId(process_node_.get(), page2.get(), nullptr);
  ExpectProcessData(2, 0);

  // Freeze the main frame in the second page.
  f2->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(2, 1);

  // Freeze the child frame of the first page, hosted in the other process.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(proc2.get()));
  f1->SetLifecycleState(LifecycleState::kFrozen);
  ExpectProcessData(2, 1);

  // Freeze the main frame of the first page.
  EXPECT_CALL(obs, OnAllFramesInProcessFrozen(process_node_.get()));
  f0->SetLifecycleState(LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&obs);
  ExpectProcessData(2, 2);

  // Destroy the child frame in the other process, and then kill that process.
  f1.reset();
  ExpectProcessData(2, 2);
  proc2.reset();
  ExpectProcessData(2, 2);

  // Kill the main frame of the second page.
  f2.reset();
  ExpectProcessData(1, 1);

  // Kill the main frame of the first page.
  f0.reset();
  ExpectProcessData(0, 0);

  graph()->RemoveProcessNodeObserver(&obs);
}

TEST_F(FrozenFrameAggregatorTest, PageAggregation) {
  ExpectPageData(0, 0);
  ExpectRunning();

  // Add a current frame.
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

  // Create a third frame that is not current.
  auto f1a = CreateFrame(f0.get(), /*is_current=*/false);
  ExpectPageData(2, 0);
  ExpectRunning();

  // Swap the f1 and f1a.
  FrameNodeImpl::UpdateCurrentFrame(/*previous_frame_node=*/f1.get(),
                                    /*current_frame_node=*/f1a.get(), graph());
  ExpectPageData(2, 0);
  ExpectRunning();

  // Freeze the original frame and swap it back.
  f1->SetLifecycleState(LifecycleState::kFrozen);
  FrameNodeImpl::UpdateCurrentFrame(/*previous_frame_node=*/f1a.get(),
                                    /*current_frame_node=*/f1.get(), graph());
  ExpectPageData(2, 1);
  ExpectRunning();

  // Freeze the non-current frame and expect nothing to change.
  f1a->SetLifecycleState(LifecycleState::kFrozen);
  ExpectPageData(2, 1);
  ExpectRunning();

  // Remove the non-current frame and expect nothing to change.
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
