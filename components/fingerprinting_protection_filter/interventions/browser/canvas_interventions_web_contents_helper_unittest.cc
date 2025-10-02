// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/interventions/browser/canvas_interventions_web_contents_helper.h"

#include <string_view>

#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<TestParam> {
 public:
  CanvasInterventionsWebContentsHelperLauncher()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    privacy_sandbox::tracking_protection::RegisterProfilePrefs(
        prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, /*is_off_the_record=*/!GetParam().run_in_regular_mode,
        /*store_last_modified=*/false,
        /*restore_session=*/false,
        /*should_record_metrics=*/false);
    management_service_ = std::make_unique<policy::ManagementService>(
        std::vector<std::unique_ptr<policy::ManagementStatusProvider>>());
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            &prefs_,
            /*host_content_settings_map=*/host_content_settings_map_.get(),
            /*management_service=*/management_service_.get(),
            /*is_incognito=*/!GetParam().run_in_regular_mode);
  }

  void TearDown() override {
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
    RenderViewHostTestHarness::TearDown();
  }

  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_.get();
  }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<policy::ManagementService> management_service_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CanvasInterventionsWebContentsHelperLauncher,
                         testing::ConvertGenerator<TestParam::TupleT>(
                             Combine(Bool(), Bool(), Bool())));

TEST_P(CanvasInterventionsWebContentsHelperLauncher,
       InterventionsNavigationPropagatesCanvasInterventionsFeature) {
  WebContents* web_contents = RenderViewHostTestHarness::web_contents();
  TestParam param = GetParam();
  fingerprinting_protection_interventions::
      CanvasInterventionsWebContentsHelper::CreateForWebContents(
          web_contents, tracking_protection_settings(),
          !param.run_in_regular_mode);

  EXPECT_NE(nullptr, CanvasInterventionsWebContentsHelper::FromWebContents(
                         RenderViewHostTestHarness::web_contents()));

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
