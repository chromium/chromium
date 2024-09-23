// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

namespace {

// The RendererAgent with its dependencies on Blink mocked out.
//
// This approach is somewhat rudimentary, but appears to be the best compromise
// considering the alternatives:
//  -- Passing in a TestRenderFrame would itself require bringing up a
//     significant number of supporting classes.
//  -- Using a RenderViewTest would not allow having any non-filtered resource
//     loads due to not having a child thread and ResourceDispatcher.
// The real implementations of the mocked-out methods will be exercised in
// browsertests.
class RendererAgentUnderTest : public RendererAgent {
 public:
  explicit RendererAgentUnderTest(UnverifiedRulesetDealer* ruleset_dealer,
                                  bool is_top_level_main_frame)
      : RendererAgent(/*render_frame=*/nullptr, ruleset_dealer),
        is_top_level_main_frame_(is_top_level_main_frame) {}

  RendererAgentUnderTest(const RendererAgentUnderTest&) = delete;
  RendererAgentUnderTest& operator=(const RendererAgentUnderTest&) = delete;

  ~RendererAgentUnderTest() override = default;

  MOCK_METHOD0(GetMainDocumentUrl, GURL());
  MOCK_METHOD0(RequestActivationState, void());
  MOCK_METHOD0(OnSetFilterForCurrentDocumentCalled, void());
  MOCK_METHOD0(OnSubresourceDisallowed, void());

  bool IsTopLevelMainFrame() override { return !is_top_level_main_frame_; }

  void SetFilterForCurrentDocument(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) override {
    last_injected_filter_ = std::move(filter);
    OnSetFilterForCurrentDocumentCalled();
  }

  blink::WebDocumentSubresourceFilter* filter() {
    return last_injected_filter_.get();
  }

  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeFilter() {
    return std::move(last_injected_filter_);
  }

  void SetInheritedActivationStateForNewDocument(
      subresource_filter::mojom::ActivationLevel level) {
    subresource_filter::mojom::ActivationState state;
    state.activation_level = level;
    inherited_activation_state_ = state;
  }

  using RendererAgent::OnActivationComputed;

 private:
  const bool is_top_level_main_frame_;

  std::unique_ptr<blink::WebDocumentSubresourceFilter> last_injected_filter_;
  subresource_filter::mojom::ActivationState inherited_activation_state_;
};

constexpr const char kTestFirstURL[] = "http://example.com/alpha";
constexpr const char kTestSecondURL[] = "http://example.com/beta";
constexpr const char kTestFirstURLPathSuffix[] = "alpha";
constexpr const char kTestSecondURLPathSuffix[] = "beta";
constexpr const char kTestBothURLsPathSuffix[] = "a";

}  // namespace

class RendererAgentTest : public ::testing::Test {
 public:
  RendererAgentTest() = default;

  RendererAgentTest(const RendererAgentTest&) = delete;
  RendererAgentTest& operator=(const RendererAgentTest&) = delete;

  ~RendererAgentTest() override = default;

 protected:
  void SetUp() override { ResetAgent(/*is_top_level_main_frame=*/true); }

  void ResetAgent(bool is_top_level_main_frame) {
    ResetAgentWithoutInitialize(is_top_level_main_frame);
    // The agent should always try to get activation when initialized.
    EXPECT_CALL(*agent(), RequestActivationState());
    agent_->Initialize();
    ::testing::Mock::VerifyAndClearExpectations(&*agent_);
  }

  // This creates the `agent_` but does not initialize it, so that tests can
  // inject gmock expectations against the `agent_` to verify or change the
  // behaviour of the initialize step.
  void ResetAgentWithoutInitialize(bool is_top_level_main_frame) {
    agent_ = std::make_unique<::testing::StrictMock<RendererAgentUnderTest>>(
        &ruleset_dealer_, is_top_level_main_frame);
    // Initialize() will see about:blank.
    EXPECT_CALL(*agent(), GetMainDocumentUrl())
        .WillRepeatedly(testing::Return(GURL("about:blank")));
    // Future document loads default to example.com.
    ON_CALL(*agent(), GetMainDocumentUrl())
        .WillByDefault(testing::Return(GURL("http://example.com/")));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(std::string_view suffix) {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_dealer_.SetRulesetFile(
        subresource_filter::testing::TestRuleset::Open(
            test_ruleset_pair.indexed));
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

  void StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel level) {
    subresource_filter::mojom::ActivationState state;
    state.activation_level = level;
    StartLoadAndSetActivationState(state);
  }

  void StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationState state) {
    agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
    agent_as_rfo()->ReadyToCommitNavigation(nullptr);
    agent()->OnActivationComputed(state.Clone());
    agent_as_rfo()->DidCreateNewDocument();
  }

  void FinishLoad() { agent_as_rfo()->DidFinishLoad(); }

  void ExpectFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetMainDocumentUrl());
    EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled());
  }

  void ExpectNoFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetMainDocumentUrl()).Times(::testing::AtLeast(0));
    EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled()).Times(0);
  }

  void ExpectNoSignalAboutSubresourceDisallowed() {
    EXPECT_CALL(*agent(), OnSubresourceDisallowed()).Times(0);
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
    if (actual_policy == blink::WebDocumentSubresourceFilter::kDisallow) {
      agent()->filter()->ReportDisallowedLoad();
    }
  }

  RendererAgentUnderTest* agent() { return agent_.get(); }
  content::RenderFrameObserver* agent_as_rfo() {
    return static_cast<content::RenderFrameObserver*>(agent_.get());
  }

 private:
  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
  UnverifiedRulesetDealer ruleset_dealer_;

  std::unique_ptr<RendererAgentUnderTest> agent_;
};

TEST_F(RendererAgentTest, RulesetUnset_RulesetNotAvailable) {
  // Do not set ruleset.
  ExpectNoFilterGetsInjected();
  // The agent should request activation state when the document changes to
  // "about:blank" even though no state will be available.
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadWithoutSettingActivationState();
  FinishLoad();
}

TEST_F(RendererAgentTest, DisabledByDefault_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoFilterGetsInjected();
  // The agent should request activation state when the document changes to
  // "about:blank" even though no state will be available.
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadWithoutSettingActivationState();
  FinishLoad();
}

TEST_F(RendererAgentTest, MmapFailure_FailsToInjectFilter) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  subresource_filter::MemoryMappedRuleset::SetMemoryMapFailuresForTesting(true);
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(agent()));

  subresource_filter::MemoryMappedRuleset::SetMemoryMapFailuresForTesting(
      false);
  ResetAgent(/*is_top_level_main_frame=*/true);
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
}

TEST_F(RendererAgentTest, Disabled_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kDisabled);
  FinishLoad();
}

TEST_F(RendererAgentTest, EnabledButRulesetUnavailable_NoFilterIsInjected) {
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  FinishLoad();
}

// Never inject a filter for root frame about:blank loads, even though we do for
// child frame loads. Those are tested via browser tests.
TEST_F(RendererAgentTest, EmptyDocumentLoad_NoFilterIsInjected) {
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), GetMainDocumentUrl())
      .WillOnce(::testing::Return(GURL("about:blank")));
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  FinishLoad();
}

TEST_F(RendererAgentTest, Enabled_FilteringIsInEffectForOneLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed());
  ;
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  FinishLoad();

  // In-page navigation should not count as a new load.
  ExpectNoFilterGetsInjected();
  ExpectNoSignalAboutSubresourceDisallowed();
  PerformSameDocumentNavigationWithoutSettingActivationLevel();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);

  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadWithoutSettingActivationState();
  FinishLoad();
}

TEST_F(RendererAgentTest, Enabled_NewRulesetIsPickedUpAtNextLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // Set the new ruleset just after the deadline for being used for the current
  // load, to exercises doing filtering based on obseleted rulesets.
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestSecondURLPathSuffix));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed());
  ;
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  FinishLoad();

  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed());
  ;
  ExpectLoadPolicy(kTestFirstURL, blink::WebDocumentSubresourceFilter::kAllow);
  ExpectLoadPolicy(kTestSecondURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  FinishLoad();
}

// Make sure that the activation decision does not outlive a failed provisional
// load (and affect the second next load).
TEST_F(RendererAgentTest,
       Enabled_FilteringNoLongerActiveAfterProvisionalLoadIsCancelled) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled());
  agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
  agent_as_rfo()->ReadyToCommitNavigation(nullptr);
  subresource_filter::mojom::ActivationStatePtr state =
      subresource_filter::mojom::ActivationState::New();
  state->activation_level =
      subresource_filter::mojom::ActivationLevel::kEnabled;
  state->measure_performance = true;
  agent()->OnActivationComputed(std::move(state));
  agent_as_rfo()->DidFailProvisionalLoad();
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled()).Times(0);
  agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
  agent_as_rfo()->ReadyToCommitNavigation(nullptr);
  agent_as_rfo()->DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  FinishLoad();
}

TEST_F(RendererAgentTest, DryRun_ResourcesAreEvaluatedButNotFiltered) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kDryRun);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // In dry-run mode, loads to the first URL should be differentiated from URLs
  // that don't match the filter but still be allowed to proceed.
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kWouldDisallow);
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kWouldDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  FinishLoad();
}

TEST_F(RendererAgentTest,
       SignalFirstSubresourceDisallowed_OncePerDocumentLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed());
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectNoSignalAboutSubresourceDisallowed();
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  FinishLoad();

  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectLoadPolicy(kTestSecondURL, blink::WebDocumentSubresourceFilter::kAllow);
  EXPECT_CALL(*agent(), OnSubresourceDisallowed());
  ;
  ExpectLoadPolicy(kTestFirstURL,
                   blink::WebDocumentSubresourceFilter::kDisallow);
  FinishLoad();
}

TEST_F(RendererAgentTest,
       SignalFirstSubresourceDisallowed_ComesAfterAgentDestroyed) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  auto filter = agent()->TakeFilter();
  ResetAgent(/*is_top_level_main_frame=*/true);

  // The filter has been disconnected from the agent, so a call to
  // reportDisallowedLoad() should not signal a first resource disallowed call
  // to the agent, nor should it cause a crash.
  ExpectNoSignalAboutSubresourceDisallowed();

  filter->ReportDisallowedLoad();
}

TEST_F(RendererAgentTest,
       FailedInitialLoad_FilterInjectedOnInitialDocumentCreation) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  ResetAgent(/*is_top_level_main_frame=*/false);
  agent()->SetInheritedActivationStateForNewDocument(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), GetMainDocumentUrl())
      .WillOnce(::testing::Return(GURL("about:blank")));
  EXPECT_CALL(*agent(), RequestActivationState());
  EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();
}

TEST_F(RendererAgentTest,
       FailedInitialMainFrameLoad_FilterInjectedOnInitialDocumentCreation) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  agent()->SetInheritedActivationStateForNewDocument(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), GetMainDocumentUrl())
      .WillOnce(::testing::Return(GURL("about:blank")));
  EXPECT_CALL(*agent(), RequestActivationState());
  EXPECT_CALL(*agent(), OnSetFilterForCurrentDocumentCalled());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();
}

}  // namespace fingerprinting_protection_filter
