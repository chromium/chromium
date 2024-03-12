// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/decorators/freezing_vote_decorator.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

namespace {

const freezing::FreezingVote kCannotFreezeVote(
    freezing::FreezingVoteValue::kCannotFreeze,
    "cannot freeze");
const freezing::FreezingVote kCanFreezeVote(
    freezing::FreezingVoteValue::kCanFreeze,
    "can freeze");

class FreezingPolicyAccess : public FreezingPolicy {
 public:
  using FreezingPolicy::CannotFreezeReason;
  using FreezingPolicy::CannotFreezeReasonToString;
};

// Mock version of a performance_manager::Freezer.
class LenientMockFreezer : public performance_manager::Freezer {
 public:
  LenientMockFreezer() = default;
  ~LenientMockFreezer() override = default;
  LenientMockFreezer(const LenientMockFreezer& other) = delete;
  LenientMockFreezer& operator=(const LenientMockFreezer&) = delete;

  MOCK_METHOD1(MaybeFreezePageNodeImpl, void(const PageNode* page_node));
  MOCK_METHOD1(UnfreezePageNodeImpl, void(const PageNode* page_node));

 private:
  void MaybeFreezePageNode(const PageNode* page_node) override {
    MaybeFreezePageNodeImpl(page_node);
    PageNodeImpl::FromNode(page_node)->SetLifecycleStateForTesting(
        performance_manager::mojom::LifecycleState::kFrozen);
  }
  void UnfreezePageNode(const PageNode* page_node) override {
    UnfreezePageNodeImpl(page_node);
    PageNodeImpl::FromNode(page_node)->SetLifecycleStateForTesting(
        performance_manager::mojom::LifecycleState::kRunning);
  }
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
    graph->PassToGraph(std::make_unique<FreezingVoteDecorator>());
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<FreezingPolicy>();
    policy_ = policy.get();
    graph->PassToGraph(std::move(policy));

    page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  }

  PageNodeImpl* page_node() { return page_node_.get(); }

  FreezingPolicy* policy() { return policy_; }

 private:
  performance_manager::TestNodeWrapper<performance_manager::PageNodeImpl>
      page_node_;

  raw_ptr<FreezingPolicy> policy_;
};

TEST_F(FreezingPolicyTest, AudiblePageGetsCannotFreezeVote) {
  page_node()->SetIsAudible(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kAudible));
}

TEST_F(FreezingPolicyTest, RecentlyAudiblePageGetsCannotFreezeVote) {
  page_node()->SetIsAudible(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  task_env().FastForwardBy(base::Seconds(1));
  page_node()->SetIsAudible(false);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kRecentlyAudible));
  task_env().FastForwardBy(FreezingPolicy::kAudioProtectionTime);
  EXPECT_FALSE(page_node()->GetFreezingVote().has_value());
}

TEST_F(FreezingPolicyTest, PageHoldingWeblockGetsCannotFreezeVote) {
  page_node()->SetIsHoldingWebLockForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kHoldingWebLock));
}

TEST_F(FreezingPolicyTest, PageHoldingIndexedDBLockGetsCannotFreezeVote) {
  page_node()->SetIsHoldingIndexedDBLockForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(
      page_node()->GetFreezingVote()->reason(),
      FreezingPolicyAccess::CannotFreezeReasonToString(
          FreezingPolicyAccess::CannotFreezeReason::kHoldingIndexedDBLock));
}

TEST_F(FreezingPolicyTest, CannotFreezeIsConnectedToUSBDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToUSBDeviceForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(
      page_node()->GetFreezingVote()->reason(),
      FreezingPolicyAccess::CannotFreezeReasonToString(
          FreezingPolicyAccess::CannotFreezeReason::kConnectedToUsbDevice));
}

TEST_F(FreezingPolicyTest, CannotFreezePageConnectedToBluetoothDevice) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::
                    kConnectedToBluetoothDevice));
}

TEST_F(FreezingPolicyTest, CannotFreezePageCapturingVideo) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingVideoForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kCapturingVideo));
}

TEST_F(FreezingPolicyTest, CannotFreezePageCapturingAudio) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingAudioForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kCapturingAudio));
}

TEST_F(FreezingPolicyTest, CannotFreezePageBeingMirrored) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsBeingMirroredForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kBeingMirrored));
}

TEST_F(FreezingPolicyTest, CannotFreezePageCapturingWindow) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingWindowForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
}

TEST_F(FreezingPolicyTest, CannotFreezePageCapturingDisplay) {
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node())
      ->SetIsCapturingDisplayForTesting(true);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCannotFreeze);
  EXPECT_EQ(page_node()->GetFreezingVote()->reason(),
            FreezingPolicyAccess::CannotFreezeReasonToString(
                FreezingPolicyAccess::CannotFreezeReason::kCapturingDisplay));
}

TEST_F(FreezingPolicyTest, FreezingVotes) {
  std::unique_ptr<MockFreezer> freezer = std::make_unique<MockFreezer>();
  auto* freezer_raw = freezer.get();
  policy()->SetFreezerForTesting(std::move(freezer));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);

  EXPECT_CALL(*freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCanFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);

  EXPECT_CALL(*freezer_raw, UnfreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCannotFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);

  EXPECT_CALL(*freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(kCanFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);

  EXPECT_CALL(*freezer_raw, UnfreezePageNodeImpl(page_node()));
  page_node()->set_freezing_vote(std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);

  // Sending a kCannotFreezeVote shouldn't unfreeze the page as it's already
  // in a non-freezable state.
  page_node()->set_freezing_vote(kCannotFreezeVote);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);

  // Same for removing a kCannotFreezeVote.
  page_node()->set_freezing_vote(std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);
}

TEST_F(FreezingPolicyTest, PageNodeIsntFrozenBeforeLoadingCompletes) {
  std::unique_ptr<MockFreezer> freezer = std::make_unique<MockFreezer>();
  auto* freezer_raw = freezer.get();
  policy()->SetFreezerForTesting(std::move(freezer));
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
  page_node()->set_freezing_vote(kCanFreezeVote);
  // The page freezer shouldn't be called as the page node isn't fully loaded
  // yet.
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);
  EXPECT_EQ(page_node()->GetFreezingVote()->value(),
            freezing::FreezingVoteValue::kCanFreeze);

  EXPECT_CALL(*freezer_raw, MaybeFreezePageNodeImpl(page_node()));
  // A transition to the fully loaded state should cause the page node to be
  // frozen.
  page_node()->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ::testing::Mock::VerifyAndClearExpectations(freezer_raw);
}

}  // namespace performance_manager::policies
