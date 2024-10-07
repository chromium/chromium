// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

// The SubresourceFilterAgent with its dependencies on Blink mocked out.
//
// This approach is somewhat rudimentary, but appears to be the best compromise
// considering the alternatives:
//  -- Passing in a TestRenderFrame would itself require bringing up a
//     significant number of supporting classes.
//  -- Using a RenderViewTest would not allow having any non-filtered resource
//     loads due to not having a child thread and ResourceDispatcher.
// The real implementations of the mocked-out methods are exercised in:
//   chrome/browser/subresource_filter/subresource_filter_browsertest.cc.
class SubresourceFilterAgentUnderTest : public SubresourceFilterAgent {
 public:
  explicit SubresourceFilterAgentUnderTest(
      UnverifiedRulesetDealer* ruleset_dealer,
      bool is_subresource_filter_root,
      bool is_provisional,
      bool is_parent_ad_frame,
      bool is_frame_created_by_ad_script)
      : SubresourceFilterAgent(nullptr /* RenderFrame */, ruleset_dealer),
        is_subresource_filter_root_(is_subresource_filter_root),
        is_provisional_(is_provisional),
        is_parent_ad_frame_(is_parent_ad_frame),
        is_frame_created_by_ad_script_(is_frame_created_by_ad_script) {}

  SubresourceFilterAgentUnderTest(const SubresourceFilterAgentUnderTest&) =
      delete;
  SubresourceFilterAgentUnderTest& operator=(
      const SubresourceFilterAgentUnderTest&) = delete;

  ~SubresourceFilterAgentUnderTest() override = default;

  MOCK_METHOD0(GetDocumentURL, GURL());
  MOCK_METHOD0(OnSetSubresourceFilterForCurrentDocumentCalled, void());
  MOCK_METHOD0(SignalFirstSubresourceDisallowedForCurrentDocument, void());
  MOCK_METHOD1(SendDocumentLoadStatistics,
               void(const mojom::DocumentLoadStatistics&));
  MOCK_METHOD0(SendFrameIsAd, void());
  MOCK_METHOD0(SendFrameWasCreatedByAdScript, void());

  bool IsSubresourceFilterChild() override {
    return !is_subresource_filter_root_;
  }
  bool IsProvisional() override { return is_provisional_; }
  bool IsParentAdFrame() override { return is_parent_ad_frame_; }
  bool IsFrameCreatedByAdScript() override {
    return is_frame_created_by_ad_script_;
  }

  void SetSubresourceFilterForCurrentDocument(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) override {
    last_injected_filter_ = std::move(filter);
    OnSetSubresourceFilterForCurrentDocumentCalled();
  }

  bool IsAdFrame() override {
    return ad_evidence_ && ad_evidence_->IndicatesAdFrame();
  }

  const std::optional<blink::FrameAdEvidence>& AdEvidence() override {
    return ad_evidence_;
  }
  void SetAdEvidence(const blink::FrameAdEvidence& ad_evidence) override {
    ad_evidence_ = ad_evidence;
  }

  blink::WebDocumentSubresourceFilter* filter() {
    return last_injected_filter_.get();
  }

  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeFilter() {
    return std::move(last_injected_filter_);
  }

  void SetInheritedActivationStateForNewDocument(mojom::ActivationLevel level) {
    mojom::ActivationState state;
    state.activation_level = level;
    inherited_activation_state_for_new_document_ = state;
  }

  using SubresourceFilterAgent::ActivateForNextCommittedLoad;

 private:
  const mojom::ActivationState GetInheritedActivationStateForNewDocument()
      override {
    return inherited_activation_state_for_new_document_;
  }

  const bool is_subresource_filter_root_;
  const bool is_provisional_;
  const bool is_parent_ad_frame_;
  const bool is_frame_created_by_ad_script_;

  // Production can set this on the RenderFrame, which we intercept and store
  // here.
  std::optional<blink::FrameAdEvidence> ad_evidence_;
  std::unique_ptr<blink::WebDocumentSubresourceFilter> last_injected_filter_;
  mojom::ActivationState inherited_activation_state_for_new_document_;
};

constexpr const char kTestFirstURL[] = "http://example.com/alpha";
constexpr const char kTestSecondURL[] = "http://example.com/beta";
constexpr const char kTestFirstURLPathSuffix[] = "alpha";
constexpr const char kTestSecondURLPathSuffix[] = "beta";
constexpr const char kTestBothURLsPathSuffix[] = "a";

// Histogram names.
constexpr const char kDocumentLoadRulesetIsAvailable[] =
    "SubresourceFilter.DocumentLoad.RulesetIsAvailable";

constexpr const char kMainFrameLoadRulesetIsAvailableAnyActivationLevel[] =
    "SubresourceFilter.MainFrameLoad.RulesetIsAvailableAnyActivationLevel";

}  // namespace

class SubresourceFilterAgentTest : public ::testing::Test {
 public:
  SubresourceFilterAgentTest() = default;

  SubresourceFilterAgentTest(const SubresourceFilterAgentTest&) = delete;
  SubresourceFilterAgentTest& operator=(const SubresourceFilterAgentTest&) =
      delete;

 protected:
  void SetUp() override {
    ResetAgent(/*is_subresource_filter_root=*/true,
               /*is_provisional=*/false,
               /*is_parent_ad_frame=*/false,
               /*is_frame_created_by_ad_script=*/false);
  }

  void ResetAgent(bool is_subresource_filter_root,
                  bool is_provisional,
                  bool is_parent_ad_frame,
                  bool is_frame_created_by_ad_script) {
    ResetAgentWithoutInitialize(is_subresource_filter_root, is_provisional,
                                is_parent_ad_frame,
                                is_frame_created_by_ad_script);
    ExpectSendFrameWasCreatedByAdScript(!is_subresource_filter_root &&
                                                !is_provisional &&
                                                is_frame_created_by_ad_script
                                            ? 1
                                            : 0);
    ExpectSendFrameIsAd(
        !is_subresource_filter_root && !is_provisional &&
                (is_parent_ad_frame || is_frame_created_by_ad_script)
            ? 1
            : 0);
    agent_->Initialize();
    ::testing::Mock::VerifyAndClearExpectations(&*agent_);
  }

  // This creates the `agent_` but does not initialize it, so that tests can
  // inject gmock expectations against the `agent_` to verify or change the
  // behaviour of the initialize step.
  void ResetAgentWithoutInitialize(bool is_subresource_filter_root,
                                   bool is_provisional,
                                   bool is_parent_ad_frame,
                                   bool is_frame_created_by_ad_script) {
    agent_ = std::make_unique<
        ::testing::StrictMock<SubresourceFilterAgentUnderTest>>(
        &ruleset_dealer_, is_subresource_filter_root, is_provisional,
        is_parent_ad_frame, is_frame_created_by_ad_script);
    // Initialize() will see about:blank.
    EXPECT_CALL(*agent(), GetDocumentURL())
        .WillRepeatedly(::testing::Return(GURL("about:blank")));
    // Future document loads default to example.com.
    ON_CALL(*agent(), GetDocumentURL())
        .WillByDefault(::testing::Return(GURL("http://example.com/")));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(std::string_view suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_dealer_.SetRulesetFile(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  void StartLoadWithoutSettingActivationState() {
    agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
    agent_as_rfo()->ReadyToCommitNavigation(nullptr);
    agent_as_rfo()->DidCreateNewDocument();
  }

  void PerformSameDocumentNavigationWithoutSettingActivationLevel() {
    agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
    agent_as_rfo()->ReadyToCommitNavigation(nullptr);
    // No DidCreateNewDocument, since same document navigations by definition
    // don't create a new document.
    // No DidFinishLoad is called in this case.
  }

  void StartLoadAndSetActivationState(mojom::ActivationLevel level,
                                      bool is_ad_frame = false) {
    mojom::ActivationState state;
    state.activation_level = level;
    StartLoadAndSetActivationState(state, is_ad_frame);
  }

  void StartLoadAndSetActivationState(mojom::ActivationState state,
                                      bool is_ad_frame = false) {
    agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
    agent_as_rfo()->ReadyToCommitNavigation(nullptr);

    std::optional<blink::FrameAdEvidence> ad_evidence;
    if (agent()->IsSubresourceFilterChild()) {
      // Generate an evidence object matching the `ad_type`.
      ad_evidence = blink::FrameAdEvidence(false /* parent_is_ad */);
      if (is_ad_frame) {
        ad_evidence->set_created_by_ad_script(
            blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
      }
      ad_evidence->set_is_complete();
    } else {
      ASSERT_FALSE(is_ad_frame);
    }

    agent()->ActivateForNextCommittedLoad(state.Clone(), ad_evidence);
    agent_as_rfo()->DidCreateNewDocument();
  }

  void FinishLoad() { agent_as_rfo()->DidFinishLoad(); }

  void ExpectSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetDocumentURL());
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCurrentDocumentCalled());
  }

  void ExpectNoSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetDocumentURL()).Times(::testing::AtLeast(0));
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCurrentDocumentCalled())
        .Times(0);
  }

  void ExpectSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCurrentDocument());
  }

  void ExpectNoSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCurrentDocument())
        .Times(0);
  }

  void ExpectDocumentLoadStatisticsSent() {
    EXPECT_CALL(*agent(), SendDocumentLoadStatistics(::testing::_));
  }

  void ExpectSendFrameIsAd(int times) {
    EXPECT_CALL(*agent(), SendFrameIsAd()).Times(times);
  }

  void ExpectSendFrameWasCreatedByAdScript(int times) {
    EXPECT_CALL(*agent(), SendFrameWasCreatedByAdScript()).Times(times);
  }

  void ExpectLoadPolicy(
      std::string_view url_spec,
      blink::WebDocumentSubresourceFilter::LoadPolicy expected_policy) {
    blink::WebURL url = GURL(url_spec);
    blink::mojom::RequestContextType request_context =
        blink::mojom::RequestContextType::IMAGE;
    blink::WebDocumentSubresourceFilter::LoadPolicy actual_policy =
        agent()->filter()->GetLoadPolicy(url, request_context);
    EXPECT_EQ(expected_policy, actual_policy);

    // If the load policy indicated the load was filtered, simulate a filtered
    // load callback. In production, this will be called in FrameFetchContext,
    // but we simulate the call here.
    if (actual_policy == blink::WebDocumentSubresourceFilter::kDisallow)
      agent()->filter()->ReportDisallowedLoad();
  }

  SubresourceFilterAgentUnderTest* agent() { return agent_.get(); }
  content::RenderFrameObserver* agent_as_rfo() {
    return static_cast<content::RenderFrameObserver*>(agent_.get());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  UnverifiedRulesetDealer ruleset_dealer_;

  std::unique_ptr<SubresourceFilterAgentUnderTest> agent_;
};

TEST_F(SubresourceFilterAgentTest, RulesetUnset_RulesetNotAvailable) {
  base::HistogramTester histogram_tester;
  // Do not set ruleset.
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectTotalCount(kDocumentLoadRulesetIsAvailable, 0);
  histogram_tester.ExpectUniqueSample(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 0, 1);
}

TEST_F(SubresourceFilterAgentTest, DisabledByDefault_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectTotalCount(kDocumentLoadRulesetIsAvailable, 0);
  histogram_tester.ExpectUniqueSample(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 1, 1);
}

TEST_F(SubresourceFilterAgentTest, MmapFailure_FailsToInjectSubresourceFilter) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  MemoryMappedRuleset::SetMemoryMapFailuresForTesting(true);
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled,
                                 false /* is_ad_frame */);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  MemoryMappedRuleset::SetMemoryMapFailuresForTesting(false);
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled,
                                 false /* is_ad_frame */);
}

TEST_F(SubresourceFilterAgentTest, Disabled_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kDisabled);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       EnabledButRulesetUnavailable_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 0, 1);
  histogram_tester.ExpectUniqueSample(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 0, 1);
}

// Never inject a filter for root frame about:blank loads, even though we do for
// child frame loads. Those are tested via browser tests.
// TODO(csharrison): Refactor these unit tests so it is easier to test with real
// backing RenderFrames.
TEST_F(SubresourceFilterAgentTest, EmptyDocumentLoad_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoSubresourceFilterGetsInjected();
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  FinishLoad();

  histogram_tester.ExpectTotalCount(kDocumentLoadRulesetIsAvailable, 0);
  histogram_tester.ExpectTotalCount(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 0);
}

TEST_F(SubresourceFilterAgentTest, Enabled_FilteringIsInEffectForOneLoad) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();

  // In-page navigation should not count as a new load.
  ExpectNoSubresourceFilterGetsInjected();
  ExpectNoSignalAboutFirstSubresourceDisallowed();
  PerformSameDocumentNavigationWithoutSettingActivationLevel();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);

  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  // Resource loads after the in-page navigation should not be counted toward
  // the figures below, as they came after the original page load event. There
  // should be no samples recorded into subresource count histograms during the
  // final load where there is no activation.
  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 1);
  histogram_tester.ExpectUniqueSample(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 1, 2);
}

TEST_F(SubresourceFilterAgentTest, Enabled_HistogramSamplesOverTwoLoads) {
  for (const bool measure_performance : {false, true}) {
    base::HistogramTester histogram_tester;
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
    ExpectSubresourceFilterGetsInjected();
    mojom::ActivationState state;
    state.activation_level = mojom::ActivationLevel::kEnabled;
    state.measure_performance = measure_performance;
    StartLoadAndSetActivationState(state);
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

    ExpectSignalAboutFirstSubresourceDisallowed();
    ExpectLoadPolicy(kTestFirstURL,
                     blink::WebDocumentSubresourceFilter::kDisallow);
    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadPolicy(kTestFirstURL,
                     blink::WebDocumentSubresourceFilter::kDisallow);
    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadPolicy(kTestSecondURL,
                     blink::WebDocumentSubresourceFilter::kAllow);
    ExpectDocumentLoadStatisticsSent();
    FinishLoad();

    ExpectSubresourceFilterGetsInjected();
    StartLoadAndSetActivationState(state);
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadPolicy(kTestSecondURL,
                     blink::WebDocumentSubresourceFilter::kAllow);
    ExpectSignalAboutFirstSubresourceDisallowed();
    ExpectLoadPolicy(kTestFirstURL,
                     blink::WebDocumentSubresourceFilter::kDisallow);
    ExpectDocumentLoadStatisticsSent();
    FinishLoad();

    histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 2);
    histogram_tester.ExpectUniqueSample(
        kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 1, 2);
  }
}

TEST_F(SubresourceFilterAgentTest, Enabled_NewRulesetIsPickedUpAtNextLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // Set the new ruleset just after the deadline for being used for the current
  // load, to exercises doing filtering based on obseleted rulesets.
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestSecondURLPathSuffix));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectLoadPolicy(kTestSecondURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();
}

// Make sure that the activation decision does not outlive a failed provisional
// load (and affect the second next load).
TEST_F(SubresourceFilterAgentTest,
       Enabled_FilteringNoLongerEffectAfterProvisionalLoadIsCancelled) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
  agent_as_rfo()->ReadyToCommitNavigation(nullptr);
  mojom::ActivationStatePtr state = mojom::ActivationState::New();
  state->activation_level = mojom::ActivationLevel::kEnabled;
  state->measure_performance = true;
  agent()->ActivateForNextCommittedLoad(std::move(state), std::nullopt);
  agent_as_rfo()->DidFailProvisionalLoad();
  agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
  agent_as_rfo()->ReadyToCommitNavigation(nullptr);
  agent_as_rfo()->DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, DryRun_ResourcesAreEvaluatedButNotFiltered) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kDryRun);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // In dry-run mode, loads to the first URL should be recorded as
  // `MatchedRules`, but still be allowed to proceed and not recorded as
  // `Disallowed`.
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kWouldDisallow);
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kWouldDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();

  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 1);
  histogram_tester.ExpectUniqueSample(
      kMainFrameLoadRulesetIsAvailableAnyActivationLevel, 1, 1);

  EXPECT_FALSE(agent()->IsAdFrame());
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_OncePerDocumentLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectNoSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_ComesAfterAgentDestroyed) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  auto filter = agent()->TakeFilter();
  ResetAgent(/*is_subresource_filter_root=*/true, /*is_provisional=*/false,
             /*is_parent_ad_frame=*/false,
             /*is_frame_created_by_ad_script=*/false);

  // The filter has been disconnected from the agent, so a call to
  // reportDisallowedLoad() should not signal a first resource disallowed call
  // to the agent, nor should it cause a crash.
  ExpectNoSignalAboutFirstSubresourceDisallowed();

  filter->ReportDisallowedLoad();
}

TEST_F(SubresourceFilterAgentTest,
       FailedInitialLoad_FilterInjectedOnInitialDocumentCreation) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  ResetAgent(/*is_subresource_filter_root=*/false, /*is_provisional=*/false,
             /*is_parent_ad_frame=*/false,
             /*is_frame_created_by_ad_script=*/false);
  agent()->SetInheritedActivationStateForNewDocument(
      mojom::ActivationLevel::kEnabled);

  ExpectNoSubresourceFilterGetsInjected();
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  EXPECT_CALL(*agent(), OnSetSubresourceFilterForCurrentDocumentCalled());
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);

  ExpectNoSubresourceFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();
}

TEST_F(SubresourceFilterAgentTest,
       FailedInitialMainFrameLoad_FilterInjectedOnInitialDocumentCreation) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  agent()->SetInheritedActivationStateForNewDocument(
      mojom::ActivationLevel::kEnabled);

  ExpectNoSubresourceFilterGetsInjected();
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  EXPECT_CALL(*agent(), OnSetSubresourceFilterForCurrentDocumentCalled());
  StartLoadAndSetActivationState(mojom::ActivationLevel::kEnabled);

  ExpectNoSubresourceFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();
}

TEST_F(SubresourceFilterAgentTest,
       DryRun_IsAssociatedWithAdFrameforDocumentOrDedicatedWorker) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ResetAgent(/*is_subresource_filter_root=*/false, /*is_provisional=*/false,
             /*is_parent_ad_frame=*/false,
             /*is_frame_created_by_ad_script=*/false);

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kDryRun,
                                 true /* is_ad_frame */);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // Test the ad frame value that is set at the filter.  This also represents
  // the flag passed to a dedicated worker filter created.  For testing the flag
  // passed to the dedicated worker filter, the unit test is not able to test
  // the implementation of WillCreateWorkerFetchContext as that will require
  // setup of a WebWorkerFetchContextImpl.
  EXPECT_TRUE(agent()->IsAdFrame());
}

TEST_F(SubresourceFilterAgentTest, DryRun_AdFrameIsUntaggedByBrowser) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ResetAgent(/*is_subresource_filter_root=*/false, /*is_provisional=*/false,
             /*is_parent_ad_frame=*/false,
             /*is_frame_created_by_ad_script=*/false);

  // Browser tags the frame as an ad.
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kDryRun,
                                 true /* is_ad_frame */);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_TRUE(agent()->IsAdFrame());
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();

  // Browser then untags the frame as an ad frame.
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(mojom::ActivationLevel::kDryRun,
                                 false /* is_ad_frame */);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_FALSE(agent()->IsAdFrame());
  ExpectDocumentLoadStatisticsSent();
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, DryRun_SendsFrameIsAd) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/true,
                              /*is_frame_created_by_ad_script=*/false);
  ExpectSendFrameIsAd(1);
  agent()->Initialize();
  // SendFrameIsAd() is sent from Initialize();
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument verify that SendFrameIsAd is not called again.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest,
       DryRun_SendFrameIsAdNotSentFromProvisionalFrame) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/true,
                              /*is_parent_ad_frame=*/true,
                              /*is_frame_created_by_ad_script=*/false);
  ExpectSendFrameIsAd(0);
  agent()->Initialize();
  // SendFrameIsAd() is not sent from Initialize() since the frame is
  // provisional.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameIsAd is not called from
  // there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest, DryRun_SendFrameIsAdNotSentFromNonAdFrame) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/false,
                              /*is_frame_created_by_ad_script=*/false);
  ExpectSendFrameIsAd(0);
  agent()->Initialize();
  // SendFrameIsAd() is not sent from Initialize() since the frame is not an ad
  // frame.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameIsAd is not called from
  // there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest, DryRun_SendFrameIsAdNotSentFromRootFrame) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/true,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/true,
                              /*is_frame_created_by_ad_script=*/false);
  ExpectSendFrameIsAd(0);
  agent()->Initialize();
  // SendFrameIsAd() is not sent from Initialize() since the frame is the root
  // frame, even though it's an ad frame.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameIsAd is not called from
  // there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest,
       DryRun_SendFrameIsAdNotSentFromNonAdSubFrame) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/false,
                              /*is_frame_created_by_ad_script*/ false);
  ExpectSendFrameIsAd(0);
  agent()->Initialize();
  // SendFrameIsAd() is not sent from Initialize() since the frame is the root
  // frame, even though it's an ad frame.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameIsAd is not called from
  // there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest, DryRun_SendsFrameWasCreatedByAdScript) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/false,
                              /*is_frame_created_by_ad_script=*/true);
  ExpectSendFrameWasCreatedByAdScript(1);
  ExpectSendFrameIsAd(1);
  agent()->Initialize();

  // Call DidCreateNewDocument twice and verify that
  // SendFrameWasCreatedByAdScript is only called once.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(SubresourceFilterAgentTest,
       DryRun_SendFrameWasCreatedByAdScriptNotSentFromProvisionalFrame) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/true,
                              /*is_parent_ad_frame=*/false,
                              /*is_frame_created_by_ad_script=*/true);
  ExpectSendFrameWasCreatedByAdScript(0);
  agent()->Initialize();
  // SendFrameWasCreatedByAdScript() is not sent from Initialize() since the
  // frame is provisional, even though it's created by ad script.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameWasCreatedByAdScript is
  // not called from there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

TEST_F(
    SubresourceFilterAgentTest,
    DryRun_SendFrameWasCreatedByAdScriptNotSentFromFrameNotCreatedByAdScript) {
  ResetAgentWithoutInitialize(/*is_subresource_filter_root=*/false,
                              /*is_provisional=*/false,
                              /*is_parent_ad_frame=*/false,
                              /*is_frame_created_by_ad_script=*/false);
  ExpectSendFrameWasCreatedByAdScript(0);
  agent()->Initialize();
  // SendFrameWasCreatedByAdScript() is not sent from Initialize() since the
  // frame was not created by ad script.
  ::testing::Mock::VerifyAndClearExpectations(agent());

  // Call DidCreateNewDocument and verify that SendFrameWasCreatedByAdScript is
  // not called from there either.
  EXPECT_CALL(*agent(), GetDocumentURL())
      .WillOnce(::testing::Return(GURL("about:blank")));
  agent_as_rfo()->DidCreateNewDocument();
}

}  // namespace subresource_filter
