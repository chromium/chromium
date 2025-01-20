// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_aggregator.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageAggregatorTest : public GraphTestHarness {
 public:
  void SetUp() override {
    GetGraphFeatures().EnablePageAggregator();
    GraphTestHarness::SetUp();
  }
};

}  // namespace

TEST_F(PageAggregatorTest, WebLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any WebLock.
  EXPECT_FALSE(page->IsHoldingWebLock());

  // |frame_0| now holds a WebLock, the corresponding property should be set on
  // the page node.
  frame_0->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // |frame_1| also holding a WebLock shouldn't affect the page property.
  frame_1->SetIsHoldingWebLock(true);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // |frame_1| still holds a WebLock after this.
  frame_0->SetIsHoldingWebLock(false);
  EXPECT_TRUE(page->IsHoldingWebLock());

  // Destroying |frame_1| without explicitly releasing the WebLock it's
  // holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->IsHoldingWebLock());
}

TEST_F(PageAggregatorTest, BlockingIndexedDBLocksAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page shouldn't hold any blocking IndexedDB lock.
  EXPECT_FALSE(page->IsHoldingBlockingIndexedDBLock());

  // |frame_0| now holds a blocking IndexedDB lock, the corresponding property
  // should be set on the page node.
  frame_0->SetIsHoldingBlockingIndexedDBLock(true);
  EXPECT_TRUE(page->IsHoldingBlockingIndexedDBLock());

  // |frame_1| also holding a blocking IndexedDB lock shouldn't affect the page
  // property.
  frame_1->SetIsHoldingBlockingIndexedDBLock(true);
  EXPECT_TRUE(page->IsHoldingBlockingIndexedDBLock());

  // |frame_1| still holds a blocking IndexedDB lock after this.
  frame_0->SetIsHoldingBlockingIndexedDBLock(false);
  EXPECT_TRUE(page->IsHoldingBlockingIndexedDBLock());

  // Destroying |frame_1| without explicitly releasing the blocking IndexedDB
  // lock it's holding should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->IsHoldingBlockingIndexedDBLock());
}

TEST_F(PageAggregatorTest, WebRTCAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page doesn't use WebRTC.
  EXPECT_FALSE(page->UsesWebRTC());

  // |frame_0| now uses WebRTC, the corresponding property should be set on the
  // page node.
  frame_0->OnStartedUsingWebRTC();
  EXPECT_TRUE(page->UsesWebRTC());

  // |frame_1| also using WebRTC shouldn't affect the page property.
  frame_1->OnStartedUsingWebRTC();
  EXPECT_TRUE(page->UsesWebRTC());

  // |frame_1| still uses WebRTC after this.
  frame_0->OnStoppedUsingWebRTC();
  EXPECT_TRUE(page->UsesWebRTC());

  // Destroying |frame_1| without explicitly notifying that it stopped using
  // WebRTC should update the corresponding page property.
  frame_1.reset();
  EXPECT_FALSE(page->UsesWebRTC());
}

TEST_F(PageAggregatorTest, FreezingOriginTrialAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page doesn't have a freezing origin trial opt-out.
  EXPECT_FALSE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| is opted-out -> the page is opted-out.
  frame_0->OnFreezingOriginTrialOptOut();
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_1| is also opted-out -> the page is still opted-out.
  frame_1->OnFreezingOriginTrialOptOut();
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_1| becomes non-current -> the page is still opted-out.
  FrameNodeImpl::UpdateCurrentFrame(frame_1.get(), nullptr, graph());
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| becomes non-current -> the page is no longer opted-out.
  FrameNodeImpl::UpdateCurrentFrame(frame_0.get(), nullptr, graph());
  EXPECT_FALSE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| becomes current -> the page is opted-out.
  FrameNodeImpl::UpdateCurrentFrame(nullptr, frame_0.get(), graph());
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| is destroyed -> the page is no longer opted-out.
  frame_0.reset();
  EXPECT_FALSE(page->HasFreezingOriginTrialOptOut());
}

}  // namespace performance_manager
