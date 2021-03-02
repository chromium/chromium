// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

class TestOptimizationGuideDecider
    : public optimization_guide::TestOptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider() = default;
  ~TestOptimizationGuideDecider() override = default;

  void RegisterOptimizationTargets(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets) override {
    registered_optimization_targets_ =
        base::flat_set<optimization_guide::proto::OptimizationTarget>(
            optimization_targets.begin(), optimization_targets.end());
  }

  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override {
    registered_optimization_types_ =
        base::flat_set<optimization_guide::proto::OptimizationType>(
            optimization_types.begin(), optimization_types.end());
  }

  // Returns the optimization types registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() {
    return registered_optimization_types_;
  }

  // Returns the optimization targets registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
  registered_optimization_targets() {
    return registered_optimization_targets_;
  }

  void ShouldTargetNavigationAsync(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::flat_map<optimization_guide::proto::ClientModelFeature,
                           float>& client_model_features,
      optimization_guide::OptimizationGuideTargetDecisionCallback callback)
      override {
    // This method should always be called with the painful page load target.
    EXPECT_EQ(optimization_target,
              optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    // We expect that the opt guide will calculate all features for us and we
    // will not override with anything.
    EXPECT_TRUE(client_model_features.empty());

    net::EffectiveConnectionType ect =
        network_quality_tracker_.GetEffectiveConnectionType();
    if (ect == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
      std::move(callback).Run(
          optimization_guide::OptimizationGuideDecision::kUnknown);
    } else if (ect <= net::EFFECTIVE_CONNECTION_TYPE_2G) {
      std::move(callback).Run(
          optimization_guide::OptimizationGuideDecision::kTrue);
    } else {
      std::move(callback).Run(
          optimization_guide::OptimizationGuideDecision::kFalse);
    }
  }

  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    auto response_iter =
        responses_.find(std::make_tuple(url, optimization_type));
    if (response_iter == responses_.end())
      return optimization_guide::OptimizationGuideDecision::kFalse;

    auto response = response_iter->second;
    if (optimization_metadata)
      *optimization_metadata = std::get<1>(response);

    return std::get<0>(response);
  }

  void SetResponses(
      std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
               std::tuple<optimization_guide::OptimizationGuideDecision,
                          optimization_guide::OptimizationMetadata>>
          responses) {
    responses_ = responses;
  }

  void ReportEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    network_quality_tracker_.ReportEffectiveConnectionTypeForTesting(
        effective_connection_type);
  }

 private:
  // The optimization types that were registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  // The optimization targets that were registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      registered_optimization_targets_;

  std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
           std::tuple<optimization_guide::OptimizationGuideDecision,
                      optimization_guide::OptimizationMetadata>>
      responses_;

  network::TestNetworkQualityTracker network_quality_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestOptimizationGuideDecider);
};

class PreviewsOptimizationGuideTest : public testing::Test {
 public:
  void SetUp() override {
    optimization_guide_decider_.reset(new TestOptimizationGuideDecider);
  }

  TestOptimizationGuideDecider* optimization_guide_decider() {
    return optimization_guide_decider_.get();
  }

  void SeedOptimizationGuideDeciderWithDefaultResponses() {
    optimization_guide::OptimizationMetadata default_metadata;


    std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
             std::tuple<optimization_guide::OptimizationGuideDecision,
                        optimization_guide::OptimizationMetadata>>
        responses = {
            {std::make_tuple(hint_not_loaded_url(),
                             optimization_guide::proto::NOSCRIPT),
             std::make_tuple(
                 optimization_guide::OptimizationGuideDecision::kUnknown,
                 default_metadata)},
            {std::make_tuple(hint_not_loaded_url(),
                             optimization_guide::proto::DEFER_ALL_SCRIPT),
             std::make_tuple(
                 optimization_guide::OptimizationGuideDecision::kUnknown,
                 default_metadata)},
        };

    optimization_guide_decider()->SetResponses(responses);
  }

  GURL blocklisted_lpr_url() { return GURL("https://blocklistedlpr.com/123"); }

  GURL hint_not_loaded_url() { return GURL("https://hintnotloaded.com/123"); }

 private:
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(PreviewsOptimizationGuideTest,
       InitializationRegistersCorrectOptimizationTypesAndTargets) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kDeferAllScriptPreviews}, {});

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(1u, registered_optimization_types.size());
  // We expect for DEFER_ALL_SCRIPT, NOSCRIPT, and RESOURCE_LOADING to be
  // registered.
  EXPECT_TRUE(registered_optimization_types.find(
                  optimization_guide::proto::DEFER_ALL_SCRIPT) !=
              registered_optimization_types.end());

  // We expect that the PAINFUL_PAGE_LOAD optimization target is always
  // registered.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      registered_optimization_targets =
          optimization_guide_decider()->registered_optimization_targets();
  EXPECT_EQ(1u, registered_optimization_targets.size());
  EXPECT_TRUE(
      registered_optimization_targets.find(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) !=
      registered_optimization_targets.end());
}

TEST_F(PreviewsOptimizationGuideTest, InitializationRegistersOnlyEnabledTypes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {previews::features::kDeferAllScriptPreviews});

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(0u, registered_optimization_types.size());

  EXPECT_EQ(registered_optimization_types.find(
                optimization_guide::proto::DEFER_ALL_SCRIPT),
            registered_optimization_types.end());

  // We expect that the PAINFUL_PAGE_LOAD optimization target is always
  // registered.
  base::flat_set<optimization_guide::proto::OptimizationTarget>
      registered_optimization_targets =
          optimization_guide_decider()->registered_optimization_targets();
  EXPECT_EQ(1u, registered_optimization_targets.size());
  EXPECT_TRUE(
      registered_optimization_targets.find(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD) !=
      registered_optimization_targets.end());
}

TEST_F(PreviewsOptimizationGuideTest,
       CanApplyPreviewWithUnknownDecisionReturnsFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kPreviews,
        {{"apply_deferallscript_when_guide_decision_unknown", "false"}}}},
      {});

  PreviewsOptimizationGuide guide(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(hint_not_loaded_url());

  EXPECT_FALSE(guide.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::DEFER_ALL_SCRIPT));
  EXPECT_FALSE(guide.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest,
       CanApplyDeferWithUnknownDecisionReturnsTrue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kPreviews,
        {{"apply_deferallscript_when_guide_decision_unknown", "true"}}}},
      {});

  PreviewsOptimizationGuide guide(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(hint_not_loaded_url());

  EXPECT_TRUE(guide.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest,
       ShouldShowPreviewWithOverrideFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kPreviews,
        {{"override_should_show_preview_check", "true"}}}},
      {});

  // Set ECT to 4G so that if feature wasn't on then we would return false.
  optimization_guide_decider()->ReportEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("doesntmatter"));

  // This should be a no-op, but call it just to make sure any decisions are
  // overridden.
  guide.StartCheckingIfShouldShowPreview(&navigation_handle);

  EXPECT_TRUE(guide.ShouldShowPreview(&navigation_handle));
}

TEST_F(PreviewsOptimizationGuideTest,
       ShouldShowPreviewWithOverrideFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{previews::features::kPreviews,
        {{"override_should_show_preview_check", "false"}}}},
      {});

  // Set ECT to 4G so that if feature wasn't on then we would return false.
  optimization_guide_decider()->ReportEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("doesntmatter"));

  guide.StartCheckingIfShouldShowPreview(&navigation_handle);

  EXPECT_FALSE(guide.ShouldShowPreview(&navigation_handle));
}

TEST_F(PreviewsOptimizationGuideTest,
       ShouldShowPreviewWithTrueDecisionReturnsTrue) {
  optimization_guide_decider()->ReportEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("doesntmatter"));

  guide.StartCheckingIfShouldShowPreview(&navigation_handle);

  EXPECT_TRUE(guide.ShouldShowPreview(&navigation_handle));
}

TEST_F(PreviewsOptimizationGuideTest,
       ShouldShowPreviewWithFalseDecisionReturnsFalse) {
  optimization_guide_decider()->ReportEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_3G);

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("doesntmatter"));

  guide.StartCheckingIfShouldShowPreview(&navigation_handle);

  EXPECT_FALSE(guide.ShouldShowPreview(&navigation_handle));
}

TEST_F(PreviewsOptimizationGuideTest,
       ShouldShowPreviewWithUnknownDecisionReturnsFalse) {
  optimization_guide_decider()->ReportEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  PreviewsOptimizationGuide guide(optimization_guide_decider());

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("doesntmatter"));

  guide.StartCheckingIfShouldShowPreview(&navigation_handle);

  EXPECT_FALSE(guide.ShouldShowPreview(&navigation_handle));
}

}  // namespace previews
