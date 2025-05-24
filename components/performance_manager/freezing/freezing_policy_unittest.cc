// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_instance_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

using FreezingType = FreezingPolicy::FreezingType;
using freezing::CannotFreezeReason;
using freezing::CannotFreezeReasonSet;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;

class MockFreezingPolicy : public FreezingPolicy {
 public:
  explicit MockFreezingPolicy(
      std::unique_ptr<freezing::Discarder> discarder,
      std::unique_ptr<freezing::OptOutChecker> opt_out_checker = nullptr)
      : FreezingPolicy(std::move(discarder), std::move(opt_out_checker)) {}
  ~MockFreezingPolicy() override = default;

  base::TimeTicks GenerateRandomPeriodicUnfreezePhase() const override {
    //  Make the periodic unfreeze phase non-random for tests.
    return base::TimeTicks();
  }

  MOCK_METHOD(void,
              RecordFreezingEligibilityUKMForPage,
              (ukm::SourceId source_id,
               double highest_cpu_current_interval,
               double highest_cpu_without_battery_saver_cannot_freeze,
               CannotFreezeReasonSet cannot_freeze_reasons),
              (override));
};

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

class FreezingPolicyTest_BaseWithNoPage : public GraphTestHarness {
 public:
  FreezingPolicyTest_BaseWithNoPage()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FreezingPolicyTest_BaseWithNoPage() override = default;
  FreezingPolicyTest_BaseWithNoPage(
      const FreezingPolicyTest_BaseWithNoPage& other) = delete;
  FreezingPolicyTest_BaseWithNoPage& operator=(
      const FreezingPolicyTest_BaseWithNoPage&) = delete;

  void OnGraphCreated(GraphImpl* graph) override {
    // The freezing logic relies on the existence of the page live state data.
    graph->PassToGraph(std::make_unique<PageLiveStateDecorator>());
    auto discarder = std::make_unique<MockDiscarder>();
    discarder_ = discarder.get();
    // Create the policy and pass it to the graph.
    auto policy = std::make_unique<MockFreezingPolicy>(
        std::move(discarder), CreateTestOptOutChecker());
    policy_ = policy.get();
    auto freezer = std::make_unique<MockFreezer>();
    freezer_ = freezer.get();
    policy_->SetFreezerForTesting(std::move(freezer));
    graph->PassToGraph(std::move(policy));
    process_node_ = CreateNode<ProcessNodeImpl>();
  }

  // Override to create an OptOutChecker for the test.
  virtual std::unique_ptr<freezing::OptOutChecker> CreateTestOptOutChecker() {
    return nullptr;
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

  // Adds CPU usage for `context` to `cpu_result_map`, with "now" as the
  // measurement time. `cumulative_background_cpu` is used as cumulative
  // background CPU and `cumulative_cpu` is used as cumulative CPU
  // (`cumulative_background_cpu` is used as cumulative CPU if `cumulative_cpu`
  // is nullopt).
  void AddCPUMeasurement(
      resource_attribution::QueryResultMap& cpu_result_map,
      resource_attribution::ResourceContext context,
      base::TimeDelta cumulative_background_cpu,
      std::optional<base::TimeDelta> cumulative_cpu = std::nullopt) {
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
  }

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
    AddCPUMeasurement(cpu_result_map, context, cumulative_background_cpu,
                      cumulative_cpu);
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(cpu_result_map));
  }

  std::pair<TestNodeWrapper<PageNodeImpl>, TestNodeWrapper<FrameNodeImpl>>
  CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId browsing_instance_id,
      const std::string& browsing_context_id = "") {
    auto page =
        CreateNode<PageNodeImpl>(/*web_contents=*/nullptr, browsing_context_id);
    page->SetType(PageType::kTab);
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

  // Expect that the `CannotFreezeReason`s applicable to `freezing_type` for
  // `page_node` match `matcher`.
  template <typename MatcherType>
  void ExpectCannotFreezeReasons(
      const PageNode* page_node,
      FreezingType freezing_type,
      MatcherType matcher,
      const base::Location& location = base::Location::Current()) {
    EXPECT_THAT(
        base::Intersection(
            policy()->GetCanFreezeDetails(page_node).cannot_freeze_reasons,
            FreezingPolicy::CannotFreezeReasonsForType(freezing_type)),
        matcher)
        << location.ToString();
  }

  // Expect that the `CannotFreezeReason`s applicable to `freezing_type` for
  // pages connected to `page_node` match `matcher`.
  template <typename MatcherType>
  void ExpectConnectedCannotFreezeReasons(
      const PageNode* page_node,
      FreezingType freezing_type,
      MatcherType matcher,
      const base::Location& location = base::Location::Current()) {
    EXPECT_THAT(base::Intersection(
                    policy()
                        ->GetCanFreezeDetails(page_node)
                        .cannot_freeze_reasons_connected_pages,
                    FreezingPolicy::CannotFreezeReasonsForType(freezing_type)),
                matcher)
        << location.ToString();
  }

  ProcessNodeImpl* process_node() { return process_node_.get(); }
  MockFreezingPolicy* policy() { return policy_; }
  MockFreezer* freezer() { return freezer_; }
  MockDiscarder* discarder() { return discarder_; }

  const content::BrowsingInstanceId kBrowsingInstanceA =
      content::BrowsingInstanceId(1);
  const content::BrowsingInstanceId kBrowsingInstanceB =
      content::BrowsingInstanceId(2);
  const content::BrowsingInstanceId kBrowsingInstanceC =
      content::BrowsingInstanceId(3);
  const resource_attribution::OriginInBrowsingInstanceContext kContext{
      url::Origin(), kBrowsingInstanceA};

 private:
  TestNodeWrapper<ProcessNodeImpl> process_node_;
  raw_ptr<MockFreezer> freezer_;
  raw_ptr<MockDiscarder> discarder_;
  raw_ptr<MockFreezingPolicy> policy_;
};

class FreezingPolicyTest : public FreezingPolicyTest_BaseWithNoPage {
 public:
  FreezingPolicyTest() = default;
  ~FreezingPolicyTest() override = default;

  void OnGraphCreated(GraphImpl* graph) override {
    FreezingPolicyTest_BaseWithNoPage::OnGraphCreated(graph);

    std::tie(page_node_, frame_node_) =
        CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  }

  PageNodeImpl* page_node() { return page_node_.get(); }

 private:
  TestNodeWrapper<PageNodeImpl> page_node_;
  TestNodeWrapper<FrameNodeImpl> frame_node_;
};

// A page with no `CannotFreezeReason` that is alone in its browsing instance is
// frozen when it has a freezing vote.
TEST_F(FreezingPolicyTest, Basic) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
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
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                                     IsEmpty());

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page2.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
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
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page3.get(), FreezingType::kVoting,
                                     IsEmpty());

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page2.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectConnectedCannotFreezeReasons(
      page3.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
}

// A browsing instance with one page that has a `CannotFreezeReason` is not
// frozen when all its pages get a freeze vote.
TEST_F(FreezingPolicyTest,
       AddFreezeVotesToBrowsingInstanceWithManyPagesAndCannotFreezeReason) {
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  page_node()->SetIsHoldingWebLockForTesting(true);
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page2.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));

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
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page2.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectConnectedCannotFreezeReasons(
      page3.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));

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
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page2.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectConnectedCannotFreezeReasons(
      page3.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
  frame2.reset();
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page3.get(), FreezingType::kVoting,
                                     IsEmpty());
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
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page_node(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectConnectedCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page3.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));

  // Deleting `frame2` puts `page_node()` in a different connected set than
  // `page2` and `page3`. `page_node()` cannot be frozen because it has a
  // `CannotFreezeReason`. `page2` and `page3` can be frozen because they have
  // freeze votes and no `CannotFreezeReason`.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  frame2.reset();
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kHoldingWebLock));
  ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  ExpectConnectedCannotFreezeReasons(page_node(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                                     IsEmpty());
  ExpectConnectedCannotFreezeReasons(
      page3.get(), FreezingType::kVoting,
      ElementsAre(CannotFreezeReason::kHoldingWebLock));
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

TEST_F(FreezingPolicyTest, FreezeVoteWithOriginTrialOptOut) {
  page_node()->SetHasFreezingOriginTrialOptOutForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after removing the origin trial opt-out.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetHasFreezingOriginTrialOptOutForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, OriginTrialOptOutWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetHasFreezingOriginTrialOptOutForTesting(true);
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
  page_node()->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, FreezeVoteWhenHoldingBlockingIndexedDBLock) {
  page_node()->SetIsHoldingBlockingIndexedDBLockForTesting(true);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing after the transaction completes.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->SetIsHoldingBlockingIndexedDBLockForTesting(false);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, AcquiresBlockingIndexedDBLockWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->SetIsHoldingBlockingIndexedDBLockForTesting(true);
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

TEST_F(FreezingPolicyTest, FreezeVoteWithNotificationPermission) {
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);

  // Don't expect freezing.
  policy()->AddFreezeVote(page_node());

  // Expect freezing if the permission is revoked.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);
  VerifyFreezerExpectations();
}

TEST_F(FreezingPolicyTest, NotificationPermissionWhenFrozen) {
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();

  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);
  VerifyFreezerExpectations();

  // Changing to ASK removes the opt-out.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::ASK);
  VerifyFreezerExpectations();

  // Changing to DENIED does nothing, since there is already no opt-out.
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);

  // Changing to GRANTED adds the opt-out.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);
  VerifyFreezerExpectations();

  // Changing to DENIED removes the opt-out.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  page_node()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::DENIED);
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

// Regression test for crbug.com/407522185: When a (non-frozen) page is added to
// a browsing instance in which all pages are frozen, the post-freezing memory
// estimates are cleared.
TEST_F(FreezingPolicyTest, DiscardGrowingPrivateMemory_PageAddedAfterFreezing) {
  base::test::ScopedFeatureList feature_list{
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF};
  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();
  // Pretend that the page is frozen.
  page_node()->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // First memory measurement after freezing.
  constexpr int kInitialPMFKb = 10;
  ReportMemoryUsage(kContext, kInitialPMFKb);

  // Add a (non-frozen) page to the browsing instance.
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);

  // Memory measurement crossing the growth threshold. This should not result in
  // discarding (or crash) since post-freezing memory estimates were cleared.
  const int kSecondPMFKb = kInitialPMFKb + growth_threshold_kb + 1;
  ReportMemoryUsage(kContext, kSecondPMFKb);

  // Pretend that the new page is frozen.
  page2->SetLifecycleStateForTesting(PageNode::LifecycleState::kFrozen);

  // Memory measurement crossing the growth threshold. Should not result in
  // discarding since it's the first measurement since the new page was added.
  const int kThirdPMFKb = kSecondPMFKb + growth_threshold_kb + 1;
  ReportMemoryUsage(kContext, kThirdPMFKb);

  // Memory measurement crossing the growth threshold. This should result in
  // discarding.
  const int kFourthPMFKb = kThirdPMFKb + growth_threshold_kb + 1;
  EXPECT_CALL(*discarder(),
              DiscardPages(testing::_, testing::UnorderedElementsAre(
                                           page_node(), page2.get())));
  ReportMemoryUsage(kContext, kFourthPMFKb);
  VerifyDiscarderExpectations();
}

TEST_F(FreezingPolicyTest, DiscardGrowingPrivateMemory_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kDiscardFrozenBrowsingInstancesWithGrowingPMF);

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

TEST_F(FreezingPolicyBatterySaverTest, RecordFreezingEligibilityUKM) {
  base::test::ScopedFeatureList feature_list(
      features::kRecordFreezingEligibilityUKM);
  base::MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  // page_node(), page2 and page3 are connected. page4 is disjoint.
  auto [page2, frame2] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  auto frame2b =
      CreateFrameNodeAutoId(process_node(), page2.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
  auto [page3, frame3] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);
  auto [page4, frame4] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceC);

  page_node()->SetUkmSourceId(ukm::AssignNewSourceId());
  page2->SetUkmSourceId(ukm::AssignNewSourceId());
  page3->SetUkmSourceId(ukm::AssignNewSourceId());
  page4->SetUkmSourceId(ukm::AssignNewSourceId());

  // kContext affects page_node(), page2 and page3. kContext2 and kContext3
  // affect page4.
  const resource_attribution::OriginInBrowsingInstanceContext kContext2{
      url::Origin(), kBrowsingInstanceC};
  const resource_attribution::OriginInBrowsingInstanceContext kContext3{
      url::Origin(), kBrowsingInstanceC};

  // A `CannotFreezeReason` applicable at the beginning of the CPU measurement
  // interval affects the UKM event.
  page_node()->SetIsHoldingWebLockForTesting(true);

  // Simulate initial CPU measurement.
  {
    resource_attribution::QueryResultMap cpu_result_map;
    AddCPUMeasurement(cpu_result_map, kContext, base::Seconds(0));
    AddCPUMeasurement(cpu_result_map, kContext2, base::Seconds(0));
    AddCPUMeasurement(cpu_result_map, kContext3, base::Seconds(0));
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(cpu_result_map));
  }
  AdvanceClock(base::Seconds(60));

  page_node()->SetIsHoldingWebLockForTesting(false);

  // A `CannotFreezeReason` applicable transiently during the CPU measurement
  // interval affects the UKM event.
  page_node()->SetIsHoldingBlockingIndexedDBLockForTesting(true);
  page_node()->SetIsHoldingBlockingIndexedDBLockForTesting(false);

  // Simulate 2nd CPU measurement. Expect UKM events to be reported.
  {
    CannotFreezeReasonSet expected_cannot_freeze_reasons{
        CannotFreezeReason::kHoldingWebLock,
        CannotFreezeReason::kHoldingBlockingIndexedDBLock};

    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page_node()->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.5,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0, expected_cannot_freeze_reasons));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page2->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.5,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0, expected_cannot_freeze_reasons));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page3->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.5,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0, expected_cannot_freeze_reasons));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page4->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/1.0,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    1.0, CannotFreezeReasonSet{}));

    resource_attribution::QueryResultMap cpu_result_map;
    AddCPUMeasurement(cpu_result_map, kContext, base::Seconds(30));   // 50%
    AddCPUMeasurement(cpu_result_map, kContext2, base::Seconds(45));  // 75%
    AddCPUMeasurement(cpu_result_map, kContext3, base::Seconds(60));  // 100%
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(cpu_result_map));
  }
  AdvanceClock(base::Seconds(60));

  // Simulate 3rd CPU measurement (no applicable `CannotFreezeReason` this
  // time). Expect UKM events to be reported.
  {
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page_node()->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.25,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0.25, CannotFreezeReasonSet{}));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page2->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.25,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0.25, CannotFreezeReasonSet{}));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page3->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.25,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    0.25, CannotFreezeReasonSet{}));
    EXPECT_CALL(*policy(),
                RecordFreezingEligibilityUKMForPage(
                    page4->GetUkmSourceID(),
                    /*highest_cpu_current_interval=*/0.75,
                    /*highest_cpu_without_battery_saver_cannot_freeze=*/
                    1.0, CannotFreezeReasonSet{}));

    resource_attribution::QueryResultMap cpu_result_map;
    AddCPUMeasurement(cpu_result_map, kContext, base::Seconds(45));   // 25%
    AddCPUMeasurement(cpu_result_map, kContext2, base::Seconds(90));  // 75%
    AddCPUMeasurement(cpu_result_map, kContext3, base::Seconds(66));  // 10%
    resource_attribution::QueryResultObserver* observer = policy();
    observer->OnResourceUsageUpdated(std::move(cpu_result_map));
  }
}

TEST_F(FreezingPolicyBatterySaverTest,
       RecordFreezingEligibilityUKMForPageStatic) {
  // No "opt-out" field is set.
  // CPU usage is bucketed.
  {
    ukm::TestAutoSetUkmRecorder recorder;
    FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
        ukm::SourceId(), /*highest_cpu_current_interval=*/0.1,
        /*highest_cpu_without_battery_saver_cannot_freeze=*/
        0.2, CannotFreezeReasonSet{});
    auto entries = recorder.GetEntriesByName(
        ukm::builders::PerformanceManager_FreezingEligibility::kEntryName);
    EXPECT_EQ(entries.size(), 1U);
    auto& entry = entries.front();
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Audible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "BeingMirrored", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Capturing", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "ConnectedToDevice", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, "HighestCPUAnyIntervalWithoutOptOut", 16);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HighestCPUCurrentInterval",
                                            8);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry,
                                            "HoldingBlockingIndexedDBLock", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HoldingWebLock", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Loading", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "NotificationPermission", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "OriginTrialOptOut", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyAudible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyVisible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Visible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "WebRTC", 0);
  }

  // All "opt-out" fields are set.
  // CPU usage is bucketed.
  {
    ukm::TestAutoSetUkmRecorder recorder;
    FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
        ukm::SourceId(), /*highest_cpu_current_interval=*/0.16,
        /*highest_cpu_without_battery_saver_cannot_freeze=*/
        0.32,
        CannotFreezeReasonSet{
            CannotFreezeReason::kAudible, CannotFreezeReason::kBeingMirrored,
            CannotFreezeReason::kCapturingAudio,
            CannotFreezeReason::kConnectedToBluetoothDevice,
            CannotFreezeReason::kHoldingBlockingIndexedDBLock,
            CannotFreezeReason::kHoldingWebLock, CannotFreezeReason::kLoading,
            CannotFreezeReason::kNotificationPermission,
            CannotFreezeReason::kFreezingOriginTrialOptOut,
            CannotFreezeReason::kRecentlyAudible,
            CannotFreezeReason::kRecentlyVisible, CannotFreezeReason::kVisible,
            CannotFreezeReason::kWebRTC});
    auto entries = recorder.GetEntriesByName(
        ukm::builders::PerformanceManager_FreezingEligibility::kEntryName);
    EXPECT_EQ(entries.size(), 1U);
    auto& entry = entries.front();
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Audible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "BeingMirrored", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Capturing", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "ConnectedToDevice", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, "HighestCPUAnyIntervalWithoutOptOut", 32);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HighestCPUCurrentInterval",
                                            16);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry,
                                            "HoldingBlockingIndexedDBLock", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HoldingWebLock", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Loading", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "NotificationPermission", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "OriginTrialOptOut", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyAudible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyVisible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Visible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "WebRTC", 1);
  }

  // Opt-out fields from Audible -> Loading are set.
  // CPU usage is zero.
  {
    ukm::TestAutoSetUkmRecorder recorder;
    FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
        ukm::SourceId(), /*highest_cpu_current_interval=*/0.0,
        /*highest_cpu_without_battery_saver_cannot_freeze=*/
        0.0,
        CannotFreezeReasonSet{
            CannotFreezeReason::kAudible, CannotFreezeReason::kBeingMirrored,
            CannotFreezeReason::kCapturingVideo,
            CannotFreezeReason::kConnectedToUsbDevice,
            CannotFreezeReason::kHoldingBlockingIndexedDBLock,
            CannotFreezeReason::kHoldingWebLock, CannotFreezeReason::kLoading});
    auto entries = recorder.GetEntriesByName(
        ukm::builders::PerformanceManager_FreezingEligibility::kEntryName);
    EXPECT_EQ(entries.size(), 1U);
    auto& entry = entries.front();
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Audible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "BeingMirrored", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Capturing", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "ConnectedToDevice", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, "HighestCPUAnyIntervalWithoutOptOut", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HighestCPUCurrentInterval",
                                            0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry,
                                            "HoldingBlockingIndexedDBLock", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HoldingWebLock", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Loading", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "NotificationPermission", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "OriginTrialOptOut", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyAudible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyVisible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Visible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "WebRTC", 0);
  }

  // Opt-out fields from Notification -> WebRTC are set.
  // CPU usage is very low (bucketing has no effect at this level).
  {
    ukm::TestAutoSetUkmRecorder recorder;
    FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
        ukm::SourceId(), /*highest_cpu_current_interval=*/0.01,
        /*highest_cpu_without_battery_saver_cannot_freeze=*/
        0.02,
        CannotFreezeReasonSet{CannotFreezeReason::kNotificationPermission,
                              CannotFreezeReason::kFreezingOriginTrialOptOut,
                              CannotFreezeReason::kRecentlyAudible,
                              CannotFreezeReason::kRecentlyVisible,
                              CannotFreezeReason::kVisible,
                              CannotFreezeReason::kWebRTC});
    auto entries = recorder.GetEntriesByName(
        ukm::builders::PerformanceManager_FreezingEligibility::kEntryName);
    EXPECT_EQ(entries.size(), 1U);
    auto& entry = entries.front();
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Audible", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "BeingMirrored", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Capturing", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "ConnectedToDevice", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(
        entry, "HighestCPUAnyIntervalWithoutOptOut", 2);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HighestCPUCurrentInterval",
                                            1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry,
                                            "HoldingBlockingIndexedDBLock", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "HoldingWebLock", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Loading", 0);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "NotificationPermission", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "OriginTrialOptOut", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyAudible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "RecentlyVisible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "Visible", 1);
    ukm::TestUkmRecorder::ExpectEntryMetric(entry, "WebRTC", 1);
  }
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

namespace {

constexpr char kOptOutUrl1[] = "http://a.com/";
constexpr char kOptOutUrl2[] = "http://b.com/";
constexpr char kBrowsingContext1[] = "browsing-context-1";
constexpr char kBrowsingContext2[] = "browsing-context-2";

// A test implementation of OptOutChecker that opts out a single URL.
class TestOptOutChecker final : public freezing::OptOutChecker {
 public:
  TestOptOutChecker() = default;
  ~TestOptOutChecker() final = default;

  TestOptOutChecker(const TestOptOutChecker&) = delete;
  TestOptOutChecker& operator=(const TestOptOutChecker&) = delete;

  // Sets the opted out url to `url`, and notifies `browser_contexts_to_notify`
  // of the change.
  void SetOptedOutUrl(
      const std::string& url,
      const std::vector<std::string>& browser_contexts_to_notify);

  // OptOutChecker:
  void SetOptOutPolicyChangedCallback(
      OnPolicyChangedForBrowserContextCallback callback) final;
  bool IsPageOptedOutOfFreezing(std::string_view browser_context_id,
                                const GURL& main_frame_url) final;

 private:
  OnPolicyChangedForBrowserContextCallback on_policy_changed_callback_;
  GURL opted_out_url_;
};

void TestOptOutChecker::SetOptedOutUrl(
    const std::string& url,
    const std::vector<std::string>& browser_contexts_to_notify = {}) {
  ASSERT_TRUE(on_policy_changed_callback_);
  opted_out_url_ = GURL(url);
  for (const std::string& browser_context_id : browser_contexts_to_notify) {
    on_policy_changed_callback_.Run(browser_context_id);
  }
}

void TestOptOutChecker::SetOptOutPolicyChangedCallback(
    OnPolicyChangedForBrowserContextCallback callback) {
  on_policy_changed_callback_ = std::move(callback);
}

bool TestOptOutChecker::IsPageOptedOutOfFreezing(
    std::string_view browser_context_id,
    const GURL& main_frame_url) {
  return opted_out_url_.is_valid() && main_frame_url == opted_out_url_;
}

class FreezingPolicyOptOutTest : public FreezingPolicyTest {
 public:
  std::unique_ptr<freezing::OptOutChecker> CreateTestOptOutChecker() override {
    auto opt_out_checker = std::make_unique<TestOptOutChecker>();
    opt_out_checker_ = opt_out_checker.get();
    return opt_out_checker;
  }

  void TearDown() override {
    // Prevent dangling raw_ptr.
    opt_out_checker_ = nullptr;
    FreezingPolicyTest::TearDown();
  }

  void NavigateToUrl(PageNodeImpl* page_node, std::string_view url) {
    page_node->OnMainFrameNavigationCommitted(
        /*same_document=*/false, base::TimeTicks::Now(), next_navigation_id_++,
        GURL(url), /*contents_mime_type=*/"",
        /*notification_permission_status=*/std::nullopt);
  }

 protected:
  raw_ptr<TestOptOutChecker> opt_out_checker_ = nullptr;

  // page_node() navigates once when the GraphTestHarness is set up.
  int next_navigation_id_ = 2;
};

}  // namespace

TEST_F(FreezingPolicyOptOutTest, MainFrameUrlChanges) {
  ASSERT_TRUE(opt_out_checker_.get());
  opt_out_checker_->SetOptedOutUrl(kOptOutUrl1);

  // Can freeze before an URL is set.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  policy()->AddFreezeVote(page_node());
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());

  // Stays frozen when navigating to an URL that's not opted out.
  NavigateToUrl(page_node(), kOptOutUrl2);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());

  // Unfreezes when navigating to an URL that's opted out.
  EXPECT_CALL(*freezer(), UnfreezePageNode(page_node()));
  NavigateToUrl(page_node(), kOptOutUrl1);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kOptedOut));

  // Navigating to the same URL does nothing.
  NavigateToUrl(page_node(), kOptOutUrl1);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting,
                            ElementsAre(CannotFreezeReason::kOptedOut));

  // Freezes when navigating away from the opted out URL.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page_node()));
  NavigateToUrl(page_node(), kOptOutUrl2);
  VerifyFreezerExpectations();
  ExpectCannotFreezeReasons(page_node(), FreezingType::kVoting, IsEmpty());
}

TEST_F(FreezingPolicyOptOutTest, OptOutPolicyChanges) {
  ASSERT_TRUE(opt_out_checker_.get());
  opt_out_checker_->SetOptedOutUrl(kOptOutUrl1);

  // Give each page a unique browsing instance so that there are no connected
  // pages. kBrowsingInstanceA is already used by page_node().
  auto [page1, frame1] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(10), kBrowsingContext1);
  auto [page2, frame2] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(11), kBrowsingContext1);
  auto [page3, frame3] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(12), kBrowsingContext2);

  // Each VerifyFreezerExpectations() call is inside a SCOPED_TRACE to help
  // interpret StrictMock "unexpected function call" failures, which don't
  // include line numbers. Also dump the PageNode pointers for each failure to
  // help interpret the arguments of unexpected functions.
  SCOPED_TRACE(::testing::Message()
               << "page1: " << page1.get() << ", page2: " << page2.get()
               << ", page3: " << page3.get());

  NavigateToUrl(page1.get(), kOptOutUrl1);
  NavigateToUrl(page2.get(), kOptOutUrl2);
  NavigateToUrl(page3.get(), kOptOutUrl2);

  // Only page1's URL is opted out.
  {
    SCOPED_TRACE("add freeze votes");
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page1.get())).Times(0);
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
    policy()->AddFreezeVote(page1.get());
    policy()->AddFreezeVote(page2.get());
    policy()->AddFreezeVote(page3.get());
    VerifyFreezerExpectations();
    ExpectCannotFreezeReasons(page1.get(), FreezingType::kVoting,
                              ElementsAre(CannotFreezeReason::kOptedOut));
    ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
    ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  }

  // Change which URL is opted out. Notify kBrowsingContext1 of the change.
  {
    SCOPED_TRACE("change opted out URL");
    EXPECT_CALL(*freezer(), UnfreezePageNode(page2.get()));
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page1.get()));
    opt_out_checker_->SetOptedOutUrl(kOptOutUrl2, {kBrowsingContext1});
    VerifyFreezerExpectations();
    ExpectCannotFreezeReasons(page1.get(), FreezingType::kVoting, IsEmpty());
    ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                              ElementsAre(CannotFreezeReason::kOptedOut));
    ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  }

  // Now notify kBrowsingContext2.
  {
    SCOPED_TRACE("notify other context");
    EXPECT_CALL(*freezer(), UnfreezePageNode(page3.get()));
    opt_out_checker_->SetOptedOutUrl(kOptOutUrl2, {kBrowsingContext2});
    VerifyFreezerExpectations();
    ExpectCannotFreezeReasons(page1.get(), FreezingType::kVoting, IsEmpty());
    ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting,
                              ElementsAre(CannotFreezeReason::kOptedOut));
    ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting,
                              ElementsAre(CannotFreezeReason::kOptedOut));
  }

  // Remove the opt out and notify both contexts. Every page should freeze.
  {
    SCOPED_TRACE("remove opt outs");
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page2.get()));
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(page3.get()));
    opt_out_checker_->SetOptedOutUrl("",
                                     {kBrowsingContext1, kBrowsingContext2});
    VerifyFreezerExpectations();
    ExpectCannotFreezeReasons(page1.get(), FreezingType::kVoting, IsEmpty());
    ExpectCannotFreezeReasons(page2.get(), FreezingType::kVoting, IsEmpty());
    ExpectCannotFreezeReasons(page3.get(), FreezingType::kVoting, IsEmpty());
  }
}

namespace {

class FreezingPolicyInfiniteTabsTest
    : public FreezingPolicyTest_BaseWithNoPage {
 protected:
  FreezingPolicyInfiniteTabsTest() = default;

  void OnGraphCreated(GraphImpl* graph) override {
    FreezingPolicyTest_BaseWithNoPage::OnGraphCreated(graph);

    // Start the test outside of the periodic unfreeze period.
    AdvanceToAlignedTime(base::Minutes(1));
    AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeDuration.Get());

    // Create "num protected tabs" hidden pages. All pages have their own
    // browsing instance, which is not equal to `kBrowsingInstance(A|B|C)`.
    for (int i = 0; i < features::kInfiniteTabsFreezing_NumProtectedTabs.Get();
         ++i) {
      auto [page, frame] =
          CreatePageAndFrameWithBrowsingInstanceId(content::BrowsingInstanceId(
              kBrowsingInstanceC.GetUnsafeValue() + i + 1));
      ASSERT_FALSE(page->IsVisible());
      pages_.push_back(std::move(page));
      frames_.push_back(std::move(frame));
      AdvanceClock(base::Milliseconds(1));
    }
  }

  // Advances the clock to a time aligned on `interval`.
  void AdvanceToAlignedTime(base::TimeDelta interval) {
    const base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeTicks next_aligned_time =
        now.SnappedToNextTick(base::TimeTicks(), interval);
    AdvanceClock(next_aligned_time - now);
  }

  std::vector<TestNodeWrapper<PageNodeImpl>> pages_;
  std::vector<TestNodeWrapper<FrameNodeImpl>> frames_;

 private:
  base::test::ScopedFeatureList feature_list_{features::kInfiniteTabsFreezing};
};

}  // namespace

// Verify that under "Infinite Tabs Freezing", tabs are frozen if not in the
// list of most recently used.
TEST_F(FreezingPolicyInfiniteTabsTest, MostRecentlyUsed) {
  // Create a new page, which takes the spot of the `pages_[0]` in the list
  // of most recently used pages. `pages_[0]` should be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  auto [page5, frame5] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  AdvanceClock(base::Milliseconds(1));
  VerifyFreezerExpectations();

  // Create a new page, which takes the spot of `pages_[1]` in the list
  // of most recently used pages. `pages_[1]` should be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[1].get()));
  auto [page6, frame6] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);
  AdvanceClock(base::Milliseconds(1));
  VerifyFreezerExpectations();

  // Make `pages_[1]` visible. It should take the spot of `pages_[2]` in the
  // list of most recently used pages. `pages_[2]` should be frozen.
  EXPECT_CALL(*freezer(), UnfreezePageNode(pages_[1].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[2].get()));
  pages_[1]->SetIsVisible(true);
  VerifyFreezerExpectations();

  // Make `pages_[2]` visible. It should take the spot of `pages_[3]]` in the
  // list of most recently used pages. `pages_[4]` should be frozen.
  EXPECT_CALL(*freezer(), UnfreezePageNode(pages_[2].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[3].get()));
  pages_[2]->SetIsVisible(true);
  VerifyFreezerExpectations();

  // Hide `pages_[3]` and `pages_[1]`. This should have no effect on freezing.
  pages_[2]->SetIsVisible(false);
  AdvanceClock(base::Milliseconds(1));
  pages_[1]->SetIsVisible(false);
  AdvanceClock(base::Milliseconds(1));

  // Create new pages. At each page creation, the least recently used protected
  // page should loose its protection and be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[4].get()));
  auto [page7, frame7] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(100));
  VerifyFreezerExpectations();
  AdvanceClock(base::Milliseconds(1));

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page5.get()));
  auto [page8, frame8] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(101));
  VerifyFreezerExpectations();
  AdvanceClock(base::Milliseconds(1));

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(page6.get()));
  auto [page9, frame9] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(102));
  VerifyFreezerExpectations();
  AdvanceClock(base::Milliseconds(1));

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[2].get()));
  auto [page10, frame10] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(103));
  VerifyFreezerExpectations();
  AdvanceClock(base::Milliseconds(1));

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[1].get()));
  auto [page11, frame11] = CreatePageAndFrameWithBrowsingInstanceId(
      content::BrowsingInstanceId(104));
  VerifyFreezerExpectations();
  AdvanceClock(base::Milliseconds(1));
}

// Verify that under "Infinite Tabs Freezing", a tab created visible (as opposed
// to hidden, as in other tests) doesn't have
// `CannotFreezeReason::kMostRecentlyUsed`, but still counts towards the limit
// of most recently used tabs protected against "Infinite Tabs Freezing".
TEST_F(FreezingPolicyInfiniteTabsTest, InitiallyVisible) {
  // Create a new page, which takes the spot of the `pages_[0]` in the list
  // of most recently used pages. `pages_[0]` should be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  auto page5 = CreateNode<PageNodeImpl>(
      /*web_contents=*/nullptr, /*browsing_context_id=*/std::string(), GURL(),
      PagePropertyFlags{PagePropertyFlag::kIsVisible});
  EXPECT_TRUE(page5->IsVisible());
  page5->SetType(PageType::kTab);
  auto frame5 =
      CreateFrameNodeAutoId(process_node(), page5.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceA);
  VerifyFreezerExpectations();
}

// Verify that under "Infinite Tabs Freezing", when there are more visible tabs
// than the desired number of protected tabs and they subsequently become
// hidden, freezing happens (as opposed to all tabs being transferred to the
// most recently used list, exceeding the limit).
TEST_F(FreezingPolicyInfiniteTabsTest, ManyVisibleTabs) {
  // Create more visible tabs than the limit of protected tabs. All existing
  // tabs are frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[1].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[2].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[3].get()));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[4].get()));

  std::vector<TestNodeWrapper<PageNodeImpl>> more_pages;
  std::vector<TestNodeWrapper<FrameNodeImpl>> more_frames;
  for (int i = 0;
       i < features::kInfiniteTabsFreezing_NumProtectedTabs.Get() * 2; ++i) {
    more_pages.push_back(CreateNode<PageNodeImpl>(
        /*web_contents=*/nullptr, /*browsing_context_id=*/std::string(), GURL(),
        PagePropertyFlags{PagePropertyFlag::kIsVisible}));
    more_pages.back()->SetType(PageType::kTab);
    more_frames.push_back(CreateFrameNodeAutoId(
        process_node(), more_pages.back().get(),
        /* parent_frame_node=*/nullptr, content::BrowsingInstanceId(100 + i)));
  }

  VerifyFreezerExpectations();

  // Hide half of tabs. They should be frozen immediately, not put in the list
  // of most recently used, as there are already more visible tabs that the
  // limit of protected tabs.
  for (int i = 0; i < features::kInfiniteTabsFreezing_NumProtectedTabs.Get();
       ++i) {
    EXPECT_CALL(*freezer(), MaybeFreezePageNode(more_pages[i].get()));
    more_pages[i]->SetIsVisible(false);
    VerifyFreezerExpectations();
  }
}

// Verify that under "Infinite Tabs Freezing", a `CannotFreezeReason` that
// applies to all types of freezing is honored.
TEST_F(FreezingPolicyInfiniteTabsTest, UniversalCannotFreezeReason) {
  // Create a new page, which takes the spot of the `pages_[0]` in the list
  // of most recently used pages. `pages_[0]` should be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  auto [page5, frame5] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  AdvanceClock(base::Milliseconds(1));
  VerifyFreezerExpectations();

  // Add a `CannotFreezeReason` to `pages_[0]`. It should be unfrozen.
  EXPECT_CALL(*freezer(), UnfreezePageNode(pages_[0].get()));
  pages_[0]->SetIsHoldingWebLockForTesting(true);
  VerifyFreezerExpectations();

  // Add a `CannotFreezeReason` to `pages_[1]`. This has no effect since it's
  // not frozen.
  pages_[1]->SetIsHoldingWebLockForTesting(true);

  // Create a new page, which takes the spot of `pages_[1]` in the list of most
  // recently used pages. `pages_[1]` should not be frozen since it holds a Web
  // Lock.
  auto [page6, frame6] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceB);
  AdvanceClock(base::Milliseconds(1));

  // When `pages_[0]` and `pages_[1]` release their lock, they're both frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  pages_[0]->SetIsHoldingWebLockForTesting(false);
  VerifyFreezerExpectations();
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[1].get()));
  pages_[1]->SetIsHoldingWebLockForTesting(false);
}

// Verify that under "Infinite Tabs Freezing", frozen tabs are periodically
// unfrozen.
TEST_F(FreezingPolicyInfiniteTabsTest, PeriodicUnfreeze) {
  // Advance to the beginning of the next periodic unfreeze period.
  AdvanceToAlignedTime(base::Minutes(1));

  // Create a new page. This should remove
  // `CannotFreezeReason::kMostRecentlyUsed` from `pages_[0]`. However, it's not
  // frozen yet since it's still in its periodic unfreeze period.
  auto [page, frame] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  ASSERT_FALSE(page->IsVisible());

  // Advance to the end of the periodic unfreeze period. `pages_[0]` should be
  // frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeDuration.Get());
  VerifyFreezerExpectations();

  // Advance to the beginning of the next periodic unfreeze period. `pages_[0]`
  // should be unfrozen.
  EXPECT_CALL(*freezer(), UnfreezePageNode(pages_[0].get()));
  AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeInterval.Get() -
               features::kInfiniteTabsFreezing_UnfreezeDuration.Get());
  VerifyFreezerExpectations();

  // Advance to the end of the periodic unfreeze period. `pages_[0]` should be
  // frozen again.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeDuration.Get());
  VerifyFreezerExpectations();

  // Add a `CannotFreezeReason`. `pages_[0]` is unfrozen.
  EXPECT_CALL(*freezer(), UnfreezePageNode(pages_[0].get()));
  pages_[0]->SetUsesWebRTCForTesting(true);
  VerifyFreezerExpectations();

  // At the next periodic unfreeze period, `pages_[0]` remains unfrozen.
  AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeInterval.Get() -
               features::kInfiniteTabsFreezing_UnfreezeDuration.Get());

  // When the periodic unfreeze period ends, `pages_[0]` is not re-frozen.
  AdvanceClock(features::kInfiniteTabsFreezing_UnfreezeDuration.Get());

  // When the `CannotFreezeReason` is removed, `pages_[0]` is frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  pages_[0]->SetUsesWebRTCForTesting(false);
  VerifyFreezerExpectations();
}

// Verify that a page with a freeze vote can be frozen even if it's in the list
// of most recently used tabs (this list only affects Infinite Tabs Freezing).
TEST_F(FreezingPolicyInfiniteTabsTest, InteractionWithVoting) {
  EXPECT_EQ(policy()->GetCanFreezeDetails(pages_[0].get()).can_freeze,
            freezing::CanFreeze::kVaries);
  ExpectCannotFreezeReasons(pages_[0].get(), FreezingType::kVoting, IsEmpty());
  ExpectCannotFreezeReasons(pages_[0].get(), FreezingType::kInfiniteTabs,
                            ElementsAre(CannotFreezeReason::kMostRecentlyUsed));

  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  policy()->AddFreezeVote(pages_[0].get());
  VerifyFreezerExpectations();
}

// Verify that a page which is CPU-intensive in the background while Battery
// Saver is active can be frozen even if it's in the list of most recently
// used tabs (this list only affects Infinite Tabs Freezing).
TEST_F(FreezingPolicyInfiniteTabsTest, InteractionWithBatterySaver) {
  base::test::ScopedFeatureList feature_list{features::kFreezingOnBatterySaver};
  policy()->ToggleFreezingOnBatterySaverMode(true);

  EXPECT_EQ(policy()->GetCanFreezeDetails(pages_[0].get()).can_freeze,
            freezing::CanFreeze::kVaries);
  ExpectCannotFreezeReasons(pages_[0].get(), FreezingType::kBatterySaver,
                            IsEmpty());
  ExpectCannotFreezeReasons(pages_[0].get(), FreezingType::kInfiniteTabs,
                            ElementsAre(CannotFreezeReason::kMostRecentlyUsed));

  const resource_attribution::OriginInBrowsingInstanceContext kPage0Context{
      url::Origin(), frames_[0]->GetBrowsingInstanceId()};

  ReportCumulativeCPUUsage(kPage0Context, base::Seconds(60));
  AdvanceClock(base::Seconds(60));
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  ReportCumulativeCPUUsage(kPage0Context, base::Seconds(75));

  VerifyFreezerExpectations();
}

// Verify that visible pages which are not tabs don't affect Infinite Tabs
// Freezing.
TEST_F(FreezingPolicyInfiniteTabsTest, NonTab) {
  // Create a new page of type `kTab` which takes the spot of the `pages_[0]` in
  // the list of most recently used pages. `pages_[0]` should be frozen.
  EXPECT_CALL(*freezer(), MaybeFreezePageNode(pages_[0].get()));
  auto [page5, frame5] =
      CreatePageAndFrameWithBrowsingInstanceId(kBrowsingInstanceA);
  EXPECT_EQ(page5->GetType(), PageType::kTab);
  AdvanceClock(base::Milliseconds(1));
  VerifyFreezerExpectations();

  // Create a new page of type `kExtension`. Unlike the previous case, this
  // should have no effect on freezing.
  auto non_tab_page = CreateNode<PageNodeImpl>(
      /*web_contents=*/nullptr, /* browsing_context_id=*/std::string(), GURL(),
      PagePropertyFlags{PagePropertyFlag::kIsVisible});
  non_tab_page->SetType(PageType::kExtension);
  auto non_tab_frame =
      CreateFrameNodeAutoId(process_node(), non_tab_page.get(),
                            /* parent_frame_node=*/nullptr, kBrowsingInstanceB);
}

}  // namespace performance_manager
