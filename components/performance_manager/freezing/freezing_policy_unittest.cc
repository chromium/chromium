// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
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

class LenientMockDiscarder : public freezing::Discarder {
 public:
  LenientMockDiscarder() = default;
  ~LenientMockDiscarder() override = default;
  LenientMockDiscarder(const LenientMockDiscarder& other) = delete;
  LenientMockDiscarder& operator=(const LenientMockDiscarder&) = delete;

  MOCK_METHOD(void,
              DiscardPages,
              (Graph * graph, std::vector<const PageNode*> page_nodes),
              (override));
};
using MockDiscarder = ::testing::StrictMock<LenientMockDiscarder>;

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
    auto discarder = std::make_unique<MockDiscarder>();
    discarder_ = discarder.get();
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<FreezingPolicy>(std::move(discarder));
    policy_ = policy.get();
    auto freezer = std::make_unique<MockFreezer>();
    freezer_ = freezer.get();
    policy_->SetFreezerForTesting(std::move(freezer));
    graph->PassToGraph(std::move(policy));

    process_node_ = CreateNode<ProcessNodeImpl>();
    std::tie(page_node_, frame_node_) =
        CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  }

  // Reports private memory footprint for `context` to the freezing policy, with
  // "now" as the measurement time.
  void ReportMemoryUsage(resource_attribution::ResourceContext context,
                         int private_footprint_kb) {
    resource_attribution::QueryResultMap memory_result_map;
    memory_result_map[context] = resource_attribution::QueryResults{
        .memory_summary_result = resource_attribution::MemorySummaryResult{
            .metadata = resource_attribution::ResultMetadata(
                /* measurement_time=*/base::TimeTicks::Now(),
                resource_attribution::MeasurementAlgorithm::kSum),
            .resident_set_size_kb = 0u,
            .private_footprint_kb =
                base::checked_cast<uint64_t>(private_footprint_kb)}};
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(memory_result_map));
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

  void VerifyDiscarderExpectations() {
    Mock::VerifyAndClearExpectations(discarder());
  }

  PageNodeImpl* page_node() { return page_node_.get(); }
  ProcessNodeImpl* process_node() { return process_node_.get(); }
  FreezingPolicy* policy() { return policy_; }
  MockFreezer* freezer() { return freezer_; }
  MockDiscarder* discarder() { return discarder_; }

  const content::BrowsingInstanceId kBrowsingInstanceA =
      content::BrowsingInstanceId(1);
  const content::BrowsingInstanceId kBrowsingInstanceB =
      content::BrowsingInstanceId(2);
  const resource_attribution::OriginInBrowsingInstanceContext kContext{
      url::Origin(), kBrowsingInstanceA};

 private:
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> frame_node_;
  raw_ptr<MockFreezer> freezer_;
  raw_ptr<MockDiscarder> discarder_;
  raw_ptr<FreezingPolicy> policy_;
};

// A page with no `CannotFreezeReason` that is alone in its browsing instance is
// frozen when it has a freezing vote.
TEST_F(FreezingPolicyTest, Basic) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 0U);
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
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 0U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 0U);

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
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
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 0U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 0U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 0U);

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 1U);
}

// A browsing instance with one page that has a `CannotFreezeReason` is not
// frozen when all its pages get a freeze vote.
TEST_F(FreezingPolicyTest,
       AddFreezeVotesToBrowsingInstanceWithManyPagesAndCannotFreezeReason) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  page_node()->SetIsHoldingWebLockForTesting(true);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);

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
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 1U);

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
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 1U);

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  frame2.reset();
  VerifyFreezerExpectations();
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 0U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 0U);
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
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 1U);

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  frame2.reset();
  VerifyFreezerExpectations();
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page_node()).size(), 0U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page2.get()).size(), 1U);
  EXPECT_EQ(policy()->GetCannotFreezeReasons(page3.get()).size(), 1U);
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

TEST_F(FreezingPolicyTest, DiscardGrowingPrivateMemory_Basic) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Another memory measurement, *not* crossing the growth threshold.
  constexpr int kSecondPMFKb = 20;
  ASSERT_LT(kSecondPMFKb - kInitialPMFKb, growth_threshold_kb);
  ReportMemoryUsage(kContext, kSecondPMFKb);

  // Another memory measurement, crossing the growth threshold. The page should
  // be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::ElementsAre(page_node())));
  ReportMemoryUsage(kContext, kInitialPMFKb + growth_threshold_kb + 1);
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest, DiscardGrowingPrivateMemory_FeatureDisabled) {
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Another memory measurement, crossing the growth threshold. The page should
  // not be discarded since the feature is disabled.
  ReportMemoryUsage(kContext, kInitialPMFKb + growth_threshold_kb + 1);
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest,
       DiscardGrowingPrivateMemory_MultipleFrozenPagesInBrowsingInstance) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  // Pretend that the pages are frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);
  page2->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Another memory measurement, crossing the growth threshold. The 2 pages
  // should be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::UnorderedElementsAre(
                                           page_node(), page2.get())));
  ReportMemoryUsage(kContext, kInitialPMFKb + growth_threshold_kb + 1);
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest,
       DiscardGrowingPrivateMemory_FrozenAndUnfrozenPagesInBrowsingInstance) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  // Pretend that the first page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing the page (2nd page still unfrozen).
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Pretend that the 2nd page is frozen.
  page2->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // Another memory measurement, crossing the growth threshold since the first
  // page was frozen (but not since *all* pages were frozen). No discarding
  // expected.
  ReportMemoryUsage(kContext, kInitialPMFKb + growth_threshold_kb + 1);

  // Another memory measurement, crossing the growth threshold since all pages
  // were frozen.  The 2 pages should be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::UnorderedElementsAre(
                                           page_node(), page2.get())));
  ReportMemoryUsage(kContext, kInitialPMFKb + 2 * (growth_threshold_kb + 1));
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest, DiscardGrowingPrivateMemory_Unfreeze) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Pretend that the page is unfrozen and re-frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kRunning);
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // Another memory measurement, crossing the growth threshold since the
  // measurement taken before unfreezing. The page should not be discarded,
  // because this is the first measurement since re-freezing.
  ReportMemoryUsage(kContext, kInitialPMFKb + growth_threshold_kb + 1);

  // Another memory measurement, crossing the growth threshold since the
  // measurement taken after re-freezing. The page should be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::ElementsAre(page_node())));
  ReportMemoryUsage(kContext, kInitialPMFKb + 2 * (growth_threshold_kb + 1));
  VerifyDiscarderExpectations();
}

TEST_F(
    FreezingPolicyTest,
    DiscardGrowingPrivateMemory_MeasurementForNewOrigin_BelowGrowthThreshold) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  const resource_attribution::OriginInBrowsingInstanceContext kOtherContext{
      url::Origin(), kBrowsingInstanceA};

  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // A memory measurement below the growth threshold for an origin not seen in
  // the first measurement. Nothing should happen.
  ReportMemoryUsage(kOtherContext, growth_threshold_kb - 1);
  VerifyDiscarderExpectations();

  // A second memory measurement above the growth threshold for an origin not
  // seen in the first measurement. The browsing instance should be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::ElementsAre(page_node())));
  ReportMemoryUsage(kOtherContext, growth_threshold_kb + 1);
  VerifyDiscarderExpectations();
}

TEST_F(
    FreezingPolicyTest,
    DiscardGrowingPrivateMemory_MeasurementForNewOrigin_AboveGrowthThreshold) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  const resource_attribution::OriginInBrowsingInstanceContext kOtherContext{
      url::Origin(), kBrowsingInstanceA};

  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // A memory measurement above the growth threshold for an origin not seen in
  // the first measurement. The browsing instance should be discarded.
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::ElementsAre(page_node())));
  ReportMemoryUsage(kOtherContext, growth_threshold_kb + 1);
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest,
       DiscardGrowingPrivateMemory_MeasurementForNewBrowsingInstance) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();

  const resource_attribution::OriginInBrowsingInstanceContext
      kUnknownBrowsingInstanceContext{url::Origin(), kBrowsingInstanceB};

  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // Simulate memory usage growth above the threshold for a browsing instance
  // not known to the `FreezingPolicy`. This should be gracefully ignored.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kUnknownBrowsingInstanceContext, kInitialPMFKb);
  ReportMemoryUsage(kUnknownBrowsingInstanceContext,
                    kInitialPMFKb + growth_threshold_kb + 1);
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
