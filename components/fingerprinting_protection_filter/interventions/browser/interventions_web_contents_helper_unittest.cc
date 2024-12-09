// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/interventions_web_contents_helper.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {
class WebContents;
}  // namespace content

namespace fingerprinting_protection_interventions {
namespace {

using ::content::RenderViewHostTestHarness;
using ::content::WebContents;

static constexpr std::string_view kGoogleUrl = "https://google.com/";

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

  void SetFeatureFlag(bool is_canvas_interventions_feature_enabled) {
    if (is_canvas_interventions_feature_enabled) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {blink::features::kCanvasInterventions},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{blink::features::kCanvasInterventions});
    }
  }

  // Same as above, but will modify the blink::RuntimeFeature value for the
  // given NavigationHandle instead of the base::Feature value.
  void SetRuntimeFeatureFlag(bool is_canvas_interventions_feature_enabled,
                             content::NavigationHandle* navigation_handle) {
    navigation_handle->GetMutableRuntimeFeatureStateContext()
        .SetCanvasInterventionsEnabled(is_canvas_interventions_feature_enabled);
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
      RenderViewHostTestHarness::web_contents());
  EXPECT_NE(nullptr, InterventionsWebContentsHelper::FromWebContents(
                         RenderViewHostTestHarness::web_contents()));
}

TEST_F(CanvasInterventionsWebContentsHelperTest,
       InterventionsNavigationPropagtesCanvasInterventionsFeature) {
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  InterventionsWebContentsHelper::CreateForWebContents(web_contents);
  SetFeatureFlag(/*is_canvas_interventions_feature_enabled=*/true);

  std::unique_ptr<content::NavigationSimulator> nav_sim_handle =
      content::NavigationSimulator::CreateBrowserInitiated(GURL(kGoogleUrl),
                                                           web_contents);
  nav_sim_handle->Start();
  auto* nav_handle = nav_sim_handle->GetNavigationHandle();

  // RuntimeFeature is not updated on the NavigationRequest yet.
  EXPECT_FALSE(
      GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));

  // RuntimeFeature should now be updated after ReadyToCommit.
  nav_sim_handle->ReadyToCommit();

  EXPECT_TRUE(
      GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));
  EXPECT_EQ(fingerprinting_protection_interventions::features::
                IsCanvasInterventionsFeatureEnabled(),
            GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));

  nav_sim_handle->Commit();

  // Reload navigation with the same web contents.
  nav_sim_handle = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kGoogleUrl), web_contents);
  nav_sim_handle->Start();
  nav_handle = nav_sim_handle->GetNavigationHandle();

  // RuntimeFeature is not updated on the NavigationRequest yet.
  SetRuntimeFeatureFlag(/*is_canvas_interventions_feature_enabled=*/true,
                        nav_handle);
  EXPECT_TRUE(GetRuntimeFeatureFlagValue(nav_handle));

  // Now set the browser-wide feature flag to false.
  ResetFeatureState();
  SetFeatureFlag(/*is_canvas_interventions_feature_enabled=*/false);

  // RuntimeFeature should now be updated after ReadyToCommit.
  nav_sim_handle->ReadyToCommit();

  EXPECT_FALSE(GetRuntimeFeatureFlagValue(nav_handle));
  EXPECT_EQ(fingerprinting_protection_interventions::features::
                IsCanvasInterventionsFeatureEnabled(),
            GetRuntimeFeatureFlagValue(nav_handle));
}

}  // namespace
}  // namespace fingerprinting_protection_interventions
