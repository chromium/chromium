// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/interventions_web_contents_helper.h"

#include <string_view>

#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {
namespace {

using ::content::RenderViewHostTestHarness;
using ::content::WebContents;

static constexpr std::string_view kTestUrl = "https://site.test/";

class CanvasInterventionsWebContentsHelperTest
    : public RenderViewHostTestHarness {
 public:
  CanvasInterventionsWebContentsHelperTest() = default;

  void SetUp() override { RenderViewHostTestHarness::SetUp(); }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    ResetFeatureState();
  }

  void ResetFeatureState() { scoped_feature_list_.Reset(); }

  void SetFeatureFlags(bool is_canvas_interventions_feature_enabled,
                       bool enable_in_regular_mode) {
    if (is_canvas_interventions_feature_enabled) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {
              {fingerprinting_protection_interventions::features::kCanvasNoise,
               {{"enable_in_regular_mode",
                 base::ToString(enable_in_regular_mode)}}},
          },
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              fingerprinting_protection_interventions::features::kCanvasNoise});
    }
  }

  bool GetRuntimeFeatureFlagValue(
      content::NavigationHandle* navigation_handle) {
    auto feature_overrides =
        navigation_handle->GetMutableRuntimeFeatureStateContext();
    return feature_overrides.IsCanvasInterventionsEnabled();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CanvasInterventionsWebContentsHelperTest, CreateForWebContents) {
  InterventionsWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), /*is_incognito=*/true);
  EXPECT_NE(nullptr, InterventionsWebContentsHelper::FromWebContents(
                         RenderViewHostTestHarness::web_contents()));
}

class CanvasInterventionsWebContentsHelperLauncher
    : public CanvasInterventionsWebContentsHelperTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  CanvasInterventionsWebContentsHelperLauncher() {
    std::tie(enable_canvas_interventions_, feature_enabled_in_regular_mode_,
             run_in_regular_mode_) = GetParam();
  }

 protected:
  bool enable_canvas_interventions_;
  bool feature_enabled_in_regular_mode_;
  bool run_in_regular_mode_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasInterventionsWebContentsHelperLauncher,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(CanvasInterventionsWebContentsHelperLauncher,
       InterventionsNavigationPropagatesCanvasInterventionsFeature) {
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  InterventionsWebContentsHelper::CreateForWebContents(
      web_contents, /*is_incognito=*/!run_in_regular_mode_);
  SetFeatureFlags(enable_canvas_interventions_,
                  feature_enabled_in_regular_mode_);

  std::unique_ptr<content::NavigationSimulator> nav_sim_handle =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kTestUrl),
                                                           web_contents);
  nav_sim_handle->Start();

  // RuntimeFeature is not updated on the NavigationRequest yet.
  EXPECT_FALSE(
      GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));

  // RuntimeFeature should now be updated after ReadyToCommit.
  nav_sim_handle->ReadyToCommit();

  if (run_in_regular_mode_) {
    EXPECT_EQ(
        enable_canvas_interventions_ && feature_enabled_in_regular_mode_,
        GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));
  } else {
    EXPECT_EQ(
        enable_canvas_interventions_,
        GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));
  }
}

}  // namespace
}  // namespace fingerprinting_protection_interventions
