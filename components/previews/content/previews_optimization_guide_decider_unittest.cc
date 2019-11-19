// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide_decider.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

class TestOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider() = default;
  ~TestOptimizationGuideDecider() override = default;

  void RegisterOptimizationTypesAndTargets(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types,
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets) override {
    registered_optimization_types_ =
        base::flat_set<optimization_guide::proto::OptimizationType>(
            optimization_types.begin(), optimization_types.end());

    registered_optimization_targets_ =
        base::flat_set<optimization_guide::proto::OptimizationTarget>(
            optimization_targets.begin(), optimization_targets.end());
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

  optimization_guide::OptimizationGuideDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target)
      override {
    // Should not be called.
    EXPECT_TRUE(false);
    return optimization_guide::OptimizationGuideDecision::kFalse;
  }

  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    auto response_iter = responses_.find(
        std::make_tuple(navigation_handle->GetURL(), optimization_type));
    if (response_iter == responses_.end())
      return optimization_guide::OptimizationGuideDecision::kFalse;
    return std::get<0>(response_iter->second);
  }

  optimization_guide::OptimizationGuideDecision
  ShouldTargetNavigationAndCanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata)
      override {
    // Previews should always call this method with painful page load as the
    // target.
    DCHECK(optimization_target ==
           optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

    auto response_iter = responses_.find(
        std::make_tuple(navigation_handle->GetURL(), optimization_type));
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

  DISALLOW_COPY_AND_ASSIGN(TestOptimizationGuideDecider);
};

class PreviewsOptimizationGuideDeciderTest : public testing::Test {
 public:
  void SetUp() override {
    optimization_guide_decider_.reset(new TestOptimizationGuideDecider);
  }

  TestOptimizationGuideDecider* optimization_guide_decider() {
    return optimization_guide_decider_.get();
  }

  void SeedOptimizationGuideDeciderWithDefaultResponses() {
    optimization_guide::OptimizationMetadata default_metadata;

    optimization_guide::OptimizationMetadata rlh_metadata;
    rlh_metadata.previews_metadata.set_inflation_percent(123);
    rlh_metadata.previews_metadata.set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_3G);
    auto* rlh1 = rlh_metadata.previews_metadata.add_resource_loading_hints();
    rlh1->set_resource_pattern("resource1");
    rlh1->set_loading_optimization_type(
        optimization_guide::proto::LOADING_BLOCK_RESOURCE);
    auto* rlh2 = rlh_metadata.previews_metadata.add_resource_loading_hints();
    rlh2->set_resource_pattern("resource2");
    rlh2->set_loading_optimization_type(
        optimization_guide::proto::LOADING_BLOCK_RESOURCE);
    rlh_metadata.previews_metadata.add_resource_loading_hints()
        ->set_resource_pattern("shouldbeskipped");
    // Should also be skipped since the resource pattern is empty.
    rlh_metadata.previews_metadata.add_resource_loading_hints()
        ->set_loading_optimization_type(
            optimization_guide::proto::LOADING_BLOCK_RESOURCE);

    std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
             std::tuple<optimization_guide::OptimizationGuideDecision,
                        optimization_guide::OptimizationMetadata>>
        responses = {
            {std::make_tuple(blacklisted_lpr_url(),
                             optimization_guide::proto::LITE_PAGE_REDIRECT),
             std::make_tuple(
                 optimization_guide::OptimizationGuideDecision::kFalse,
                 default_metadata)},
            {std::make_tuple(hint_not_loaded_url(),
                             optimization_guide::proto::NOSCRIPT),
             std::make_tuple(
                 optimization_guide::OptimizationGuideDecision::kUnknown,
                 default_metadata)},
            {std::make_tuple(resource_loading_hints_url(),
                             optimization_guide::proto::RESOURCE_LOADING),
             std::make_tuple(
                 optimization_guide::OptimizationGuideDecision::kTrue,
                 rlh_metadata)},
        };

    optimization_guide_decider()->SetResponses(responses);
  }

  GURL blacklisted_lpr_url() { return GURL("https://blacklistedlpr.com/123"); }

  GURL hint_not_loaded_url() { return GURL("https://hintnotloaded.com/123"); }

  GURL resource_loading_hints_url() {
    return GURL("https://hasresourceloadinghints.com/123");
  }

 private:
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(PreviewsOptimizationGuideDeciderTest,
       InitializationRegistersCorrectOptimizationTypesAndTargets) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kLitePageServerPreviews,
       previews::features::kDeferAllScriptPreviews,
       previews::features::kNoScriptPreviews,
       previews::features::kResourceLoadingHints},
      {});

  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(4u, registered_optimization_types.size());
  // We expect for LITE_PAGE_REDIRECT, DEFER_ALL_SCRIPT, NOSCRIPT, and
  // RESOURCE_LOADING to be registered.
  EXPECT_TRUE(registered_optimization_types.find(
                  optimization_guide::proto::LITE_PAGE_REDIRECT) !=
              registered_optimization_types.end());
  EXPECT_TRUE(registered_optimization_types.find(
                  optimization_guide::proto::DEFER_ALL_SCRIPT) !=
              registered_optimization_types.end());
  EXPECT_TRUE(
      registered_optimization_types.find(optimization_guide::proto::NOSCRIPT) !=
      registered_optimization_types.end());
  EXPECT_TRUE(registered_optimization_types.find(
                  optimization_guide::proto::RESOURCE_LOADING) !=
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

TEST_F(PreviewsOptimizationGuideDeciderTest,
       InitializationRegistersOnlyEnabledTypes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kLitePageServerPreviews},
      {previews::features::kDeferAllScriptPreviews,
       previews::features::kNoScriptPreviews,
       previews::features::kResourceLoadingHints});

  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(1u, registered_optimization_types.size());
  // We only expect for LITE_PAGE_REDIRECT to be registered, as it's the only
  // type that is enabled.
  EXPECT_TRUE(registered_optimization_types.find(
                  optimization_guide::proto::LITE_PAGE_REDIRECT) !=
              registered_optimization_types.end());
  EXPECT_EQ(registered_optimization_types.find(
                optimization_guide::proto::DEFER_ALL_SCRIPT),
            registered_optimization_types.end());
  EXPECT_EQ(
      registered_optimization_types.find(optimization_guide::proto::NOSCRIPT),
      registered_optimization_types.end());
  EXPECT_EQ(registered_optimization_types.find(
                optimization_guide::proto::RESOURCE_LOADING),
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

TEST_F(PreviewsOptimizationGuideDeciderTest,
       PreviewsTypeWithoutCorrespondingOptimizationTypeReturnsFalse) {
  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://whatever.com/"));

  EXPECT_FALSE(decider.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::DEPRECATED_LOFI));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       LitePageRedirectConvertsToOptimizationTypeCorrectly) {
  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(blacklisted_lpr_url());

  EXPECT_FALSE(decider.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       LitePageRedirectSwitchOverridesDecisionForCanApplyPreview) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kIgnoreLitePageRedirectOptimizationBlacklist);

  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(blacklisted_lpr_url());

  EXPECT_TRUE(decider.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle,
      PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       CanApplyPreviewPopulatesResourceLoadingHintsCache) {
  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  // Make sure resource loading hints not cached.
  std::vector<std::string> resource_loading_hints;
  EXPECT_FALSE(decider.GetResourceLoadingHints(resource_loading_hints_url(),
                                               &resource_loading_hints));
  EXPECT_TRUE(resource_loading_hints.empty());

  // Check if we can apply it and metadata is properly applied.
  PreviewsUserData data(/*page_id=*/1);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(resource_loading_hints_url());
  EXPECT_TRUE(decider.CanApplyPreview(&data, &navigation_handle,
                                      PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_EQ(123, data.data_savings_inflation_percent());

  // Make sure resource loading hints are validated and cached.
  EXPECT_TRUE(decider.GetResourceLoadingHints(resource_loading_hints_url(),
                                              &resource_loading_hints));
  EXPECT_EQ(2u, resource_loading_hints.size());
  EXPECT_EQ("resource1", resource_loading_hints[0]);
  EXPECT_EQ("resource2", resource_loading_hints[1]);
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       CanApplyPreviewWithUnknownDecisionReturnsFalse) {
  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(hint_not_loaded_url());

  EXPECT_FALSE(decider.CanApplyPreview(
      /*previews_data=*/nullptr, &navigation_handle, PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       MaybeLoadOptimizationHintsWithAtLeastOneNonFalseDecisionReturnsTrue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kLitePageServerPreviews,
       previews::features::kDeferAllScriptPreviews,
       previews::features::kNoScriptPreviews,
       previews::features::kResourceLoadingHints},
      {});

  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(hint_not_loaded_url());

  EXPECT_TRUE(decider.MaybeLoadOptimizationHints(&navigation_handle,
                                                 base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       MaybeLoadOptimizationHintsReturnsFalseIfNoClientSidePreviewsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kLitePageServerPreviews},
      {previews::features::kDeferAllScriptPreviews,
       previews::features::kNoScriptPreviews,
       previews::features::kResourceLoadingHints});

  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(hint_not_loaded_url());

  EXPECT_FALSE(decider.MaybeLoadOptimizationHints(&navigation_handle,
                                                  base::DoNothing()));
}

TEST_F(PreviewsOptimizationGuideDeciderTest,
       MaybeLoadOptimizationHintsWithAllFalseDecisionsReturnsFalse) {
  PreviewsOptimizationGuideDecider decider(optimization_guide_decider());
  SeedOptimizationGuideDeciderWithDefaultResponses();

  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://nohints.com"));

  EXPECT_FALSE(decider.MaybeLoadOptimizationHints(&navigation_handle,
                                                  base::DoNothing()));
}

}  // namespace previews
