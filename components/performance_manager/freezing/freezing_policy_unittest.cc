// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/browsing_instance_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

using ::testing::Mock;

// Mock version of a Freezer.
class LenientMockFreezer : public Freezer {
 public:
  LenientMockFreezer() = default;
  ~LenientMockFreezer() override = default;
  LenientMockFreezer(const LenientMockFreezer& other) = delete;
  LenientMockFreezer& operator=(const LenientMockFreezer&) = delete;

  MOCK_METHOD(void,
              MaybeFreezePageNode,
              (const PageNode* page_node),
              (override));
  MOCK_METHOD(void, UnfreezePageNode, (const PageNode* page_node), (override));
};
using MockFreezer = ::testing::StrictMock<LenientMockFreezer>;

}  // namespace

class FreezingPolicyTest : public GraphTestHarness {
 public:
  FreezingPolicyTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FreezingPolicyTest() override = default;
  FreezingPolicyTest(const FreezingPolicyTest& other) = delete;
  FreezingPolicyTest& operator=(const FreezingPolicyTest&) = delete;

  void OnGraphCreated(GraphImpl* graph) override {
    // The freezing logic relies on the existence of the page live state data.
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>());
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<FreezingPolicy>();
    policy_ = policy.get();
    auto freezer = std::make_unique<MockFreezer>();
    freezer_ = freezer.get();
    policy_->SetFreezerForTesting(std::move(freezer));
    graph->PassToGraph(std::move(policy));

    process_node_ = CreateNode<ProcessNodeImpl>();
    std::tie(page_node_, frame_node_) =
        CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  }

  std::pair<TestNodeWrapper<PageNodeImpl>, TestNodeWrapper<FrameNodeImpl>>
  CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId browsing_instance_id) {
    auto page = CreateNode<PageNodeImpl>();
    auto frame = CreateFrameNodeAutoId(process_node(), page.get(),
                                       /* parent_frame_node=*/nullptr,
                                       browsing_instance_id);
    return std::make_pair(std::move(page), std::move(frame));
  }

  void VerifyFreezerExpectations() {
    Mock::VerifyAndClearExpectations(freezer());
  }

  PageNodeImpl* page_node() { return page_node_.get(); }
  ProcessNodeImpl* process_node() { return process_node_.get(); }
  FreezingPolicy* policy() { return policy_; }
  MockFreezer* freezer() { return freezer_; }

  const content::BrowsingInstanceId kBrowsingInstanceA =
      content::BrowsingInstanceId(1);
  const content::BrowsingInstanceId kBrowsingInstanceB =
      content::BrowsingInstanceId(2);

 private:
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> frame_node_;
  raw_ptr<MockFreezer> freezer_;
  raw_ptr<FreezingPolicy> policy_;
};

// A page with no `CannotFreezeReason` that is alone in its browsing instance is
// frozen when it has a freezing vote.
TEST_F(FreezingPolicyTest, Basic) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();
}

// Multiple connected pages in the same browsing instance with no
// `CannotFreezeReason` are frozen when they all have a freezing vote.
TEST_F(FreezingPolicyTest, ManyPagesSameBrowsingInstance) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  // Adding a freezing vote to each of the 2 pages in the browsing instance
  // freezes all pages.
  policy()->AddFreezeVote(page_node());
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();

  // Adding a 3rd page (with no freezing vote yet) to the browsing instance
  // unfreezes all pages.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  policy()->AddFreezeVote(page3.get());
  VerifyFreezerExpectations();

  // Multiple votes on the same page don't change anything.
  policy()->AddFreezeVote(page3.get());
  policy()->AddFreezeVote(page3.get());

  // Removing a freezing vote from one page unfreezes all pages.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
  policy()->RemoveFreezeVote(page_node());
  VerifyFreezerExpectations();

  policy()->RemoveFreezeVote(page2.get());
  policy()->RemoveFreezeVote(page3.get());
  policy()->RemoveFreezeVote(page3.get());
  policy()->RemoveFreezeVote(page3.get());
}

// Similar to ManyPagesSameBrowsingInstance, except that the 1st and 3rd pages
// don't have frames in the same browsing instance (they're indirectly connected
// via the 2nd page).
TEST_F(FreezingPolicyTest, ConnectedPages) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);

  // Adding a freezing vote to the 2 connected pages freezes them.
  policy()->AddFreezeVote(page_node());
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();

  // Adding a 3rd page (with no freezing vote yet) to the set of connected pages
  // unfreezes all pages.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  policy()->AddFreezeVote(page3.get());
  VerifyFreezerExpectations();

  // Multiple votes on the same page don't change anything.
  policy()->AddFreezeVote(page3.get());
  policy()->AddFreezeVote(page3.get());

  // Removing a freezing vote from one page unfreezes all pages.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
  policy()->RemoveFreezeVote(page_node());
  VerifyFreezerExpectations();

  policy()->RemoveFreezeVote(page2.get());
  policy()->RemoveFreezeVote(page3.get());
  policy()->RemoveFreezeVote(page3.get());
  policy()->RemoveFreezeVote(page3.get());
}

// A browsing instance with many pages that each have a freeze vote is unfrozen
// when one of the pages gets a `CannotFreezeReason`.
TEST_F(FreezingPolicyTest,
       AddCannotFreezeReasonToBrowsingInstanceWithManyPages) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  policy()->AddFreezeVote(page_node());
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
}

// Similar to AddCannotFreezeReasonToBrowsingInstanceWithManyPages, except that
// the 1st and 3rd pages don't have frames in the same browsing instance
// (they're indirectly connected via the 2nd page).
TEST_F(FreezingPolicyTest, AddCannotFreezeReasonToConnectedPages) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);

  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  policy()->AddFreezeVote(page3.get());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
}

// A browsing instance with one page that has a `CannotFreezeReason` is not
// frozen when all its pages get a freeze vote.
TEST_F(FreezingPolicyTest,
       AddFreezeVotesToBrowsingInstanceWithManyPagesAndCannotFreezeReason) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  page_node()->SetIsHoldingWebLockForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();
}

// Similar to
// AddFreezeVotesToBrowsingInstanceWithManyPagesAndCannotFreezeReason, except
// that the 1st and 3rd pages don't have frames in the same browsing instance
// (they're indirectly connected via the 2nd page).
TEST_F(FreezingPolicyTest,
       AddFreezeVotesToConnectedPagesWithCannotFreezeReason) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);
  page_node()->SetIsHoldingWebLockForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  policy()->AddFreezeVote(page3.get());
  VerifyFreezerExpectations();
}

// Verify that frozen state is correctly updated when a set of connected pages
// is broken in two by the deletion of a frame.
TEST_F(FreezingPolicyTest, BreakConnectedSet) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);

  page_node()->SetIsHoldingWebLockForTesting(true);
  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  policy()->AddFreezeVote(page3.get());

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  frame2.reset();
  VerifyFreezerExpectations();
}

// Similar to BreakConnectedSet, but the connected set left by the page from
// which a page is deleted can be frozen.
TEST_F(FreezingPolicyTest, BreakConnectedSet_LeftSetIsFrozen) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);

  page2->SetIsHoldingWebLockForTesting(true);
  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  policy()->AddFreezeVote(page3.get());

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  frame2.reset();
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenVisible) {
  page_node()->SetIsVisible(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());
}

TEST_F(FreezingPolicyTest, BecomesVisibleWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsVisible(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenRecentlyVisible) {
  page_node()->SetIsVisible(true);
  page_node()->SetIsVisible(false);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after visible protection time.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  task_env().FastForwardBy(features::kFreezingVisibleProtectionTime.Get());
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, BecomesRecentlyVisibleWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsVisible(true);
  // Don't expect freezing, because the page is still "recently visible".
  page_node()->SetIsVisible(false);

  // Expect freezing after visible protection time.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  task_env().FastForwardBy(features::kFreezingVisibleProtectionTime.Get());
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenAudible) {
  page_node()->SetIsAudible(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());
}

TEST_F(FreezingPolicyTest, BecomesAudibleWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsAudible(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenRecentlyAudible) {
  page_node()->SetIsAudible(true);
  page_node()->SetIsAudible(false);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after audio protection time.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  task_env().FastForwardBy(features::kFreezingAudioProtectionTime.Get());
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, BecomesRecentlyAudibleWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsAudible(true);
  // Don't expect freezing, because the page is still "recently audible".
  page_node()->SetIsAudible(false);

  // Expect freezing after audio protection time.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  task_env().FastForwardBy(features::kFreezingAudioProtectionTime.Get());
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenHoldingWebLock) {
  page_node()->SetIsHoldingWebLockForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after releasing the lock.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetIsHoldingWebLockForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, AcquiresWebLockWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsHoldingIndexedDBLockForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenHoldingIndexedDBLock) {
  page_node()->SetIsHoldingIndexedDBLockForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after releasing the lock.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetIsHoldingIndexedDBLockForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, AcquiresIndexedDBLockWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsHoldingIndexedDBLockForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after disconnecting from USB device.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, ConnectedToUSBDeviceWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after disconnecting from Bluetooth device.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, ConnectedToBluetoothDeviceWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenConnectedToHidDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToHidDeviceForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after disconnecting from HID device.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToHidDeviceForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, ConnectedToHidDeviceWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToHidDeviceForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenConnectedToSerialPort) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToSerialPortForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after disconnecting from HID device.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToSerialPortForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, ConnectedToSerialPortWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToSerialPortForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after stopping capture.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsCapturingVideoWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after stopping capture.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsCapturingAudioWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after mirroring stops.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsBeingMirroredWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after stopping capture.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsCapturingWindowWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after stopping capture.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsCapturingDisplayWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenUsingWebRTC) {
  page_node()->SetUsesWebRTCForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after stopping capture.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetUsesWebRTCForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsUsingWebRTCWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetUsesWebRTCForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenLoading) {
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedBusy);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after finishing loading.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, StartsLoadingWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  VerifyFreezerExpectations();
}

namespace {

class FreezingPolicyBatterySaverTest : public FreezingPolicyTest {
 public:
  FreezingPolicyBatterySaverTest() = default;

  // Reports CPU usage for `context` to the the freezing policy, with "now" as
  // the measurement time. `cumulative_background_cpu` is used as cumulative
  // background CPU and `cumulative_cpu` is used as cumulative CPU
  // (`cumulative_background_cpu` is used as cumulative CPU if `cumulative_cpu`
  // is nullopt).
  void ReportCumulativeCPUUsage(
      resource_attribution::ResourceContext context,
      base::TimeDelta cumulative_background_cpu,
      std::optional<base::TimeDelta> cumulative_cpu = std::nullopt) {
    resource_attribution::QueryResultMap cpu_result_map;
    cpu_result_map[context] = resource_attribution::QueryResults{
        .cpu_time_result = resource_attribution::CPUTimeResult{
            .metadata = resource_attribution::ResultMetadata(
                /* measurement_time=*/base::TimeTicks::Now(),
                resource_attribution::MeasurementAlgorithm::kSum),
            .start_time = base::TimeTicks(),
            .cumulative_cpu = cumulative_cpu.has_value()
                                  ? cumulative_cpu.value()
                                  : cumulative_background_cpu,
            .cumulative_background_cpu = cumulative_background_cpu}};
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(cpu_result_map));
  }

  const resource_attribution::OriginInBrowsingInstanceContext kContext{
      url::Origin(), kBrowsingInstanceA};

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kFreezingOnBatterySaver};
};

}  // namespace

TEST_F(FreezingPolicyBatterySaverTest, Basic) {
  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // The page should be frozen when a browsing instance connected to it consumes
  // >=25% CPU in background.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
}

TEST_F(FreezingPolicyBatterySaverTest, ConnectedPages) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);

  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // The page should be frozen when a browsing instance connected to it consumes
  // >=25% CPU in background.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
}

TEST_F(FreezingPolicyBatterySaverTest, CannotFreeze) {
  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // Add a `CannotFreezeReason`.
  page_node()->SetIsHoldingWebLockForTesting(true);

  // The page should not be frozen when a browsing instance connected to it
  // consumes >=25% CPU in background, because it has a `CannotFreezeReason`.
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
  AdvanceClock(base::Seconds(60));

  // Remove the `CannotFreezeReason`. This should not cause the page to be
  // frozen, since there was a `CannotFreezeReason` when high CPU usage was
  // measured.
  page_node()->SetIsHoldingWebLockForTesting(false);

  // The page should not be frozen when a browsing instance connected to it
  // consumes >=25% CPU in background, because it transiently had a
  // `CannotFreezeReason` during the measurement interval.
  ReportCumulativeCPUUsage(kContext, base::Seconds(90));
  AdvanceClock(base::Seconds(60));

  // The page should be frozen when a browsing instance connected to it consumes
  // >=25% CPU in background and there was no `CannotFreezeReason` at any point
  // during the measurement interval.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  ReportCumulativeCPUUsage(kContext, base::Seconds(105));
}

TEST_F(FreezingPolicyBatterySaverTest, CannotFreezeTransient) {
  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // Transiently add a `CannotFreezeReason`.
  page_node()->SetIsHoldingWebLockForTesting(true);
  page_node()->SetIsHoldingWebLockForTesting(false);

  // The page should not be frozen when a browsing instance connected to it
  // consumes >=25% CPU in background, because it transiently had a
  // `CannotFreezeReason` during the measurement interval.
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
}

TEST_F(FreezingPolicyBatterySaverTest, BatterySaverInactive) {
  // Battery Saver is not active in this test.

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // The page should not be frozen when a browsing instance connected to it
  // consumes >=25% CPU in background, because Battery Saver is not active.
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
}

TEST_F(FreezingPolicyBatterySaverTest, ForegroundCPU) {
  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext,
                           /*cumulative_background_cpu=*/base::Seconds(60),
                           /*cumulative_cpu=*/base::Seconds(60));
  AdvanceClock(base::Seconds(60));

  // The page should not be frozen when a browsing instance connected to it
  // consumes >=25% CPU in foreground, but little CPU in background.
  ReportCumulativeCPUUsage(kContext,
                           /*cumulative_background_cpu=*/base::Seconds(62),
                           /*cumulative_cpu=*/base::Seconds(90));
}

TEST_F(FreezingPolicyBatterySaverTest, DeactivateBatterySaver) {
  policy()->ToggleFreezingOnBatterySaverMode(true);

  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));
  VerifyFreezerExpectations();

  // The page should be unfrozen when Battery Saver becomes inactive.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  policy()->ToggleFreezingOnBatterySaverMode(false);
}

TEST_F(FreezingPolicyBatterySaverTest, ActivateBatterySaverAfterHighCPU) {
  // Battery Saver is not active at the beginning of this test.

  // Report high background CPU usage.
  ReportCumulativeCPUUsage(kContext, base::Seconds(60));
  AdvanceClock(base::Seconds(60));
  ReportCumulativeCPUUsage(kContext, base::Seconds(75));

  // The page should be frozen when Battery Saver becomes active.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->ToggleFreezingOnBatterySaverMode(true);
}

}  // namespace performance_manager
