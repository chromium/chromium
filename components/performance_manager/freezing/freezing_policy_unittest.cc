// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/browsing_instance_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  MOCK_METHOD1(MaybeFreezePageNode, void(const PageNode* page_node));
  MOCK_METHOD1(UnfreezePageNode, void(const PageNode* page_node));
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

// Multiple pages in the same browsing instance with no `CannotFreezeReason` are
// frozen when they all have a freezing vote.
TEST_F(FreezingPolicyTest, BasicManyPages) {
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
  std::vector<const PageNode*> unfrozen_pages;
  EXPECT_CALL(*freezer(), UnfreezePageNode(testing::_))
      .Times(3)
      .WillRepeatedly([&](const PageNode* page_node) {
        unfrozen_pages.push_back(page_node);
      });
  ;
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  VerifyFreezerExpectations();
  // Validate arguments after the fact because the value of `page3` isn't known
  // in advance.
  EXPECT_THAT(unfrozen_pages, testing::UnorderedElementsAre(
                                  page_node(), page2.get(), page3.get()));

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
  page_node()->SetIsAudible(true);
  VerifyFreezerExpectations();
}

// A browsing instance with one page that has a `CannotFreezeReason` is not
// frozen when all its pages get a freeze vote.
TEST_F(FreezingPolicyTest,
       AddFreezeVotesToBrowsingInstanceWithManyPagesAndCannotFreezeReason) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  page_node()->SetIsAudible(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();
}

// A page associated with many browsing instances cannot be frozen.
TEST_F(FreezingPolicyTest, ManyBrowsingInstances) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  auto other_main_frame =
      CreateFrameNodeAutoId(process_node(), page_node(),
                            /*parent_frame_node=*/nullptr, kBrowsingInstanceB);
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  other_main_frame.reset();
  VerifyFreezerExpectations();
}

// A browsing instance with many pages cannot be frozen if one of these pages is
// associated with many browsing instances.
TEST_F(FreezingPolicyTest, ManyBrowsingInstancesManyPages) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  policy()->AddFreezeVote(page_node());
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  policy()->AddFreezeVote(page2.get());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  auto other_main_frame =
      CreateFrameNodeAutoId(process_node(), page_node(),
                            /*parent_frame_node=*/nullptr, kBrowsingInstanceB);
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  other_main_frame.reset();
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
  task_env().FastForwardBy(FreezingPolicy::kAudioProtectionTime);
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
  task_env().FastForwardBy(FreezingPolicy::kAudioProtectionTime);
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

}  // namespace performance_manager
