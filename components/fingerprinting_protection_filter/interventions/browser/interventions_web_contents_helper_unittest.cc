// Copyright 2025 The Chromium Authors
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

using ::testing::Bool;
using ::testing::Combine;
using ::testing::ConvertGenerator;

using CanvasInterventionsWebContentsHelperTest = RenderViewHostTestHarness;

TEST_F(CanvasInterventionsWebContentsHelperTest, CreateForWebContents) {
  InterventionsWebContentsHelper::CreateForWebContents(
      RenderViewHostTestHarness::web_contents(), /*is_incognito=*/true);
  EXPECT_NE(nullptr, InterventionsWebContentsHelper::FromWebContents(
                         RenderViewHostTestHarness::web_contents()));
}

struct TestParam {
  using TupleT = std::tuple<bool, bool, bool>;
  explicit TestParam(TupleT params)
      : enable_block_canvas_readback(std::get<0>(params)),
        feature_enabled_in_regular_mode(std::get<1>(params)),
        run_in_regular_mode(std::get<2>(params)) {}

  const bool enable_block_canvas_readback;
  const bool feature_enabled_in_regular_mode;
  const bool run_in_regular_mode;
};

class BlockReadbackFeatureFlag {
 public:
  BlockReadbackFeatureFlag(bool is_block_readback_feature_enabled,
                           bool enable_in_regular_mode) {
    if (is_block_readback_feature_enabled) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {
              {fingerprinting_protection_interventions::features::
                   kBlockCanvasReadback,
               {{"enable_in_regular_mode",
                 base::ToString(enable_in_regular_mode)}}},
          },
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{fingerprinting_protection_interventions::
                                     features::kBlockCanvasReadback});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

bool GetRuntimeFeatureFlagValue(content::NavigationHandle* navigation_handle) {
  return navigation_handle->GetMutableRuntimeFeatureStateContext()
      .IsBlockCanvasReadbackEnabled();
}

class CanvasInterventionsWebContentsHelperLauncher
    : public CanvasInterventionsWebContentsHelperTest,
      public testing::WithParamInterface<TestParam> {};

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasInterventionsWebContentsHelperLauncher,
                         testing::ConvertGenerator<TestParam::TupleT>(
                             Combine(Bool(), Bool(), Bool())));

TEST_P(CanvasInterventionsWebContentsHelperLauncher,
       InterventionsNavigationPropagatesCanvasInterventionsFeature) {
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  TestParam param = GetParam();
  InterventionsWebContentsHelper::CreateForWebContents(
      web_contents, /*is_incognito=*/!param.run_in_regular_mode);
  BlockReadbackFeatureFlag block_readback_feature_flag(
      param.enable_block_canvas_readback,
      param.feature_enabled_in_regular_mode);

  std::unique_ptr<content::NavigationSimulator> nav_sim_handle =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://site.test/"), web_contents);
  nav_sim_handle->Start();

  // RuntimeFeature is not updated on the NavigationRequest yet.
  ASSERT_FALSE(
      GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));

  // RuntimeFeature should now be updated after ReadyToCommit.
  nav_sim_handle->ReadyToCommit();

  if (param.run_in_regular_mode) {
    EXPECT_EQ(
        param.enable_block_canvas_readback &&
            param.feature_enabled_in_regular_mode,
        GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));
  } else {
    EXPECT_EQ(
        param.enable_block_canvas_readback,
        GetRuntimeFeatureFlagValue(nav_sim_handle->GetNavigationHandle()));
  }
}

}  // namespace
}  // namespace fingerprinting_protection_interventions
