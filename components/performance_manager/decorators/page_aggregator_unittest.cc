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

  // |frame_1| becomes non-active -> the page is still opted-out.
  frame_1->SetIsActive(false);
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| becomes non-active -> the page is no longer opted-out.
  frame_0->SetIsActive(false);
  EXPECT_FALSE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| becomes active -> the page is opted-out.
  frame_0->SetIsActive(true);
  EXPECT_TRUE(page->HasFreezingOriginTrialOptOut());

  // |frame_0| is destroyed -> the page is no longer opted-out.
  frame_0.reset();
  EXPECT_FALSE(page->HasFreezingOriginTrialOptOut());
}

TEST_F(PageAggregatorTest, FormInteractionAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page doesn't have form interaction.
  EXPECT_FALSE(page->HadFormInteraction());

  // |frame_0| has form interaction -> the page has form interaction.
  frame_0->SetHadFormInteraction();
  EXPECT_TRUE(page->HadFormInteraction());

  // |frame_1| also has form interaction -> the page still has form interaction.
  frame_1->SetHadFormInteraction();
  EXPECT_TRUE(page->HadFormInteraction());

  // |frame_1| becomes non-active -> the page still has form interaction.
  frame_1->SetIsActive(false);
  EXPECT_TRUE(page->HadFormInteraction());

  // |frame_0| becomes non-active -> the page no longer has form interaction.
  frame_0->SetIsActive(false);
  EXPECT_FALSE(page->HadFormInteraction());

  // |frame_0| becomes active -> the page has form interaction again.
  frame_0->SetIsActive(true);
  EXPECT_TRUE(page->HadFormInteraction());

  // |frame_1| navigates (resetting form interaction) while non-active -> page
  // still has form interaction from |frame_0|.
  frame_1->OnNavigationCommitted(
      GURL("http://www.foo.com"),
      url::Origin::Create(GURL("http://www.foo.com")),
      /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_TRUE(page->HadFormInteraction());

  // |frame_0| navigates (resetting form interaction) while active -> page no
  // longer has form interaction.
  frame_0->OnNavigationCommitted(
      GURL("http://www.bar.com"),
      url::Origin::Create(GURL("http://www.bar.com")),
      /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_FALSE(page->HadFormInteraction());
}

TEST_F(PageAggregatorTest, UserEditsAggregation) {
  // Creates a page containing 2 frames.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  TestNodeWrapper<FrameNodeImpl> frame_0 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());
  TestNodeWrapper<FrameNodeImpl> frame_1 =
      graph()->CreateFrameNodeAutoId(process.get(), page.get());

  // By default the page doesn't have user edits.
  EXPECT_FALSE(page->HadUserEdits());

  // |frame_0| has user edits -> the page has user edits.
  frame_0->SetHadUserEdits();
  EXPECT_TRUE(page->HadUserEdits());

  // |frame_1| also has user edits -> the page still has user edits.
  frame_1->SetHadUserEdits();
  EXPECT_TRUE(page->HadUserEdits());

  // |frame_1| becomes non-active -> the page still has user edits.
  frame_1->SetIsActive(false);
  EXPECT_TRUE(page->HadUserEdits());

  // |frame_0| becomes non-active -> the page no longer has user edits.
  frame_0->SetIsActive(false);
  EXPECT_FALSE(page->HadUserEdits());

  // |frame_0| becomes active -> the page has user edits again.
  frame_0->SetIsActive(true);
  EXPECT_TRUE(page->HadUserEdits());

  // |frame_1| navigates (resetting user edits) while non-active -> page still
  // has user edits from |frame_0|.
  frame_1->OnNavigationCommitted(
      GURL("http://www.foo.com"),
      url::Origin::Create(GURL("http://www.foo.com")),
      /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_TRUE(page->HadUserEdits());

  // |frame_0| navigates (resetting user edits) while active -> page no longer
  // has user edits.
  frame_0->OnNavigationCommitted(
      GURL("http://www.bar.com"),
      url::Origin::Create(GURL("http://www.bar.com")),
      /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_FALSE(page->HadUserEdits());
}

}  // namespace performance_manager
