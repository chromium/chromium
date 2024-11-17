// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/renderer/mock_renderer_agent.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/shared/renderer/filter_utils.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

namespace {
using ::testing::_;

constexpr const char kTestFirstURL[] = "http://example.com/alpha";
constexpr const char kTestSecondURL[] = "http://example.com/beta";
constexpr const char kTestFirstURLPathSuffix[] = "alpha";
constexpr const char kTestSecondURLPathSuffix[] = "beta";
constexpr const char kTestBothURLsPathSuffix[] = "a";

constexpr const char kSubresourceLoadEvaluationWallDurationHistogram[] =
    "FingerprintingProtection.SubresourceLoad.Evaluation.WallDuration";
constexpr const char kSubresourceLoadEvaluationCPUDurationHistogram[] =
    "FingerprintingProtection.SubresourceLoad.Evaluation.CPUDuration";

}  // namespace

class RendererAgentTest : public ::testing::Test {
 public:
  RendererAgentTest() = default;

  RendererAgentTest(const RendererAgentTest&) = delete;
  RendererAgentTest& operator=(const RendererAgentTest&) = delete;

  ~RendererAgentTest() override = default;

 protected:
  void SetUp() override {
    ResetAgent(/*is_top_level_main_frame=*/true, /*has_valid_opener=*/false);
  }

  void ResetAgent(bool is_top_level_main_frame,
                  bool has_valid_opener,
                  std::optional<subresource_filter::mojom::ActivationState>
                      inherited_activation = std::nullopt) {
    ResetAgentWithoutInitialize(is_top_level_main_frame, has_valid_opener);
    if (inherited_activation.has_value()) {
      ON_CALL(*agent(), GetInheritedActivationState())
          .WillByDefault(testing::Return(inherited_activation));
    }

    if (!is_top_level_main_frame || has_valid_opener) {
      // Eligible to inherit activation.
      EXPECT_CALL(*agent(), GetInheritedActivationState());
      if (inherited_activation.has_value() &&
          inherited_activation.value().activation_level !=
              subresource_filter::mojom::ActivationLevel::kDisabled) {
        EXPECT_CALL(*agent(), OnSetFilterCalled());
      } else {
        // No activation to inherit.
        EXPECT_CALL(*agent(), RequestActivationState());
      }
    } else {
      // Ineligible to inherit activation.
      EXPECT_CALL(*agent(), RequestActivationState());
    }
    agent_->Initialize();
    ::testing::Mock::VerifyAndClearExpectations(&*agent_);
  }

  // This creates the `agent_` but does not initialize it, so that tests can
  // inject gmock expectations against the `agent_` to verify or change the
  // behaviour of the initialize step.
  void ResetAgentWithoutInitialize(bool is_top_level_main_frame,
                                   bool has_valid_opener) {
    agent_ = std::make_unique<::testing::StrictMock<MockRendererAgent>>(
        &ruleset_dealer_, is_top_level_main_frame, has_valid_opener);
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
    EXPECT_CALL(*agent(), GetMainDocumentUrl()).Times(::testing::AtLeast(0));
    EXPECT_CALL(*agent(), OnSetFilterCalled());
  }

  void ExpectNoFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetMainDocumentUrl()).Times(::testing::AtLeast(0));
    EXPECT_CALL(*agent(), OnSetFilterCalled()).Times(0);
  }

  void ExpectNoSignalAboutSubresourceDisallowed() {
    EXPECT_CALL(*agent(), OnSubresourceDisallowed(_)).Times(0);
  }

  void ExpectLoadPolicy(std::string_view url_spec,
                        subresource_filter::LoadPolicy expected_policy) {
    blink::WebURL url = GURL(url_spec);
    network::mojom::RequestDestination request_destination =
        network::mojom::RequestDestination::kImage;
    subresource_filter::LoadPolicy actual_policy =
        agent()->filter()->GetLoadPolicy(
            url, subresource_filter::ToElementType(request_destination));
    EXPECT_EQ(expected_policy, actual_policy);

    // If the load policy indicated the load was filtered, simulate a filtered
    // load callback.
    if (actual_policy == subresource_filter::LoadPolicy::DISALLOW) {
      agent()->OnSubresourceDisallowed(url_spec);
    }
  }

  MockRendererAgent* agent() { return agent_.get(); }
  content::RenderFrameObserver* agent_as_rfo() {
    return static_cast<content::RenderFrameObserver*>(agent_.get());
  }

 private:
  base::test::TaskEnvironment message_loop_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
  UnverifiedRulesetDealer ruleset_dealer_;

  std::unique_ptr<MockRendererAgent> agent_;
};

TEST_F(RendererAgentTest, RulesetUnset_RulesetNotAvailable) {
  base::HistogramTester histogram_tester;
  // Do not set ruleset.
  ExpectNoFilterGetsInjected();
  // The agent should request activation state when the document changes to
  // "about:blank" even though no state will be available.
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectTotalCount(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 0);
  histogram_tester.ExpectTotalCount(DocumentLoadRulesetIsAvailableHistogramName,
                                    0);
}

TEST_F(RendererAgentTest, DisabledByDefault_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoFilterGetsInjected();
  // The agent should request activation state when the document changes to
  // "about:blank" even though no state will be available.
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadWithoutSettingActivationState();

  histogram_tester.ExpectTotalCount(DocumentLoadRulesetIsAvailableHistogramName,
                                    0);
  histogram_tester.ExpectTotalCount(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 0);

  // Metrics are emitted upon OnActivationComputed callback.
  subresource_filter::mojom::ActivationStatePtr state =
      subresource_filter::mojom::ActivationState::New();
  state->activation_level =
      subresource_filter::mojom::ActivationLevel::kDisabled;
  agent()->OnActivationComputed(std::move(state));
  FinishLoad();

  histogram_tester.ExpectTotalCount(DocumentLoadRulesetIsAvailableHistogramName,
                                    0);
  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
}

TEST_F(RendererAgentTest, MmapFailure_FailsToInjectFilter) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  subresource_filter::MemoryMappedRuleset::SetMemoryMapFailuresForTesting(true);
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(agent()));

  // Even if there is a memory mapping failure for the ruleset, the ruleset
  // dealer can still have ruleset file(s) to read from.
  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);

  subresource_filter::MemoryMappedRuleset::SetMemoryMapFailuresForTesting(
      false);
  ResetAgent(/*is_top_level_main_frame=*/true, /*has_valid_opener=*/false);
  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 2);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 2);
}

TEST_F(RendererAgentTest, Disabled_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kDisabled);
  FinishLoad();

  histogram_tester.ExpectTotalCount(DocumentLoadRulesetIsAvailableHistogramName,
                                    0);
  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
}

TEST_F(RendererAgentTest, EnabledButRulesetUnavailable_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 0, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 0, 1);
}

// Never inject a filter for root frame about:blank loads, even though we do for
// child frame loads.
TEST_F(RendererAgentTest, EmptyDocumentLoad_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  FinishLoad();

  histogram_tester.ExpectTotalCount(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1);
  histogram_tester.ExpectTotalCount(DocumentLoadRulesetIsAvailableHistogramName,
                                    1);
}

TEST_F(RendererAgentTest, Enabled_FilteringIsInEffectForOneLoad) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestFirstURL));

  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);
  FinishLoad();

  // In-page navigation should not count as a new load.
  ExpectNoFilterGetsInjected();
  ExpectNoSignalAboutSubresourceDisallowed();
  PerformSameDocumentNavigationWithoutSettingActivationLevel();
  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestFirstURL));
  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);

  ExpectNoFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
}

TEST_F(RendererAgentTest, Enabled_ActivationIsInheritedWhenAvailable) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  subresource_filter::mojom::ActivationState inherited_activation;
  inherited_activation.activation_level =
      subresource_filter::mojom::ActivationLevel::kEnabled;
  // Activation should only be inherited for child frames or main frames with a
  // valid opener.
  ResetAgent(/*is_top_level_main_frame=*/false, /*has_valid_opener=*/true,
             inherited_activation);

  EXPECT_CALL(*agent(), GetMainDocumentUrl());
  StartLoadWithoutSettingActivationState();
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestFirstURL));

  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);
  FinishLoad();

  // Not a main frame load.
  histogram_tester.ExpectTotalCount(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 0);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
}

TEST_F(RendererAgentTest, Enabled_NewRulesetIsPickedUpAtNextLoad) {
  base::HistogramTester histogram_tester;
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

  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestFirstURL));

  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);

  ExpectFilterGetsInjected();
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestSecondURL));

  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::ALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::DISALLOW);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 2);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 2);
}

// Make sure that the activation decision does not outlive a failed provisional
// load (and affect the second load).
TEST_F(RendererAgentTest,
       Enabled_FilteringNoLongerActiveAfterProvisionalLoadIsCancelled) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  EXPECT_CALL(*agent(), OnSetFilterCalled());
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

  EXPECT_CALL(*agent(), OnSetFilterCalled()).Times(0);
  agent_as_rfo()->DidStartNavigation(GURL(), std::nullopt);
  agent_as_rfo()->ReadyToCommitNavigation(nullptr);
  agent_as_rfo()->DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
}

TEST_F(RendererAgentTest, DryRun_ResourcesAreEvaluatedButNotFiltered) {
  base::HistogramTester histogram_tester;
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
                   subresource_filter::LoadPolicy::WOULD_DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
}

TEST_F(RendererAgentTest,
       FailedInitialLoad_FilterInjectedOnInitialDocumentCreation) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  ResetAgent(/*is_top_level_main_frame=*/false, /*has_valid_opener=*/false);

  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), OnSetFilterCalled());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();

  // Not a main frame load.
  histogram_tester.ExpectTotalCount(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 0);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
}

TEST_F(RendererAgentTest,
       FailedInitialMainFrameLoad_FilterInjectedOnInitialDocumentCreation) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix("somethingNotMatched"));

  ExpectNoFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  EXPECT_CALL(*agent(), OnSetFilterCalled());
  StartLoadAndSetActivationState(
      subresource_filter::mojom::ActivationLevel::kEnabled);

  ExpectNoFilterGetsInjected();
  agent_as_rfo()->DidFailProvisionalLoad();

  histogram_tester.ExpectUniqueSample(
      MainFrameLoadRulesetIsAvailableAnyActivationLevelHistogramName, 1, 1);
  histogram_tester.ExpectUniqueSample(
      DocumentLoadRulesetIsAvailableHistogramName, 1, 1);
}

TEST_F(RendererAgentTest,
       Enabled_FilteringIsInEffectForOneLoad_PerformanceMeasurementsRecorded) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectFilterGetsInjected();
  EXPECT_CALL(*agent(), RequestActivationState());
  subresource_filter::mojom::ActivationState activation_state;
  activation_state.activation_level =
      subresource_filter::mojom::ActivationLevel::kEnabled;
  activation_state.measure_performance = true;
  StartLoadAndSetActivationState(activation_state);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  EXPECT_CALL(*agent(), OnSubresourceDisallowed(kTestFirstURL));

  ExpectLoadPolicy(kTestFirstURL, subresource_filter::LoadPolicy::DISALLOW);
  ExpectLoadPolicy(kTestSecondURL, subresource_filter::LoadPolicy::ALLOW);
  FinishLoad();

  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationWallDurationHistogram, 2);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationCPUDurationHistogram, 2);
}

}  // namespace fingerprinting_protection_filter
