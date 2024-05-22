// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"

class AnimationObserver : public PermissionChipView::Observer {
 public:
  explicit AnimationObserver(base::OnceClosure quit_closure)
      : animation_complete_callback_(std::move(quit_closure)) {}

  void OnAnimationEnded() { std::move(animation_complete_callback_).Run(); }

  // PermissionChipView::Observer
  void OnChipVisibilityChanged(bool is_visible) override {}
  void OnExpandAnimationEnded() override { OnAnimationEnded(); }
  void OnCollapseAnimationEnded() override { OnAnimationEnded(); }

 private:
  base::OnceClosure animation_complete_callback_;
};

class PermissionDashboardUnitTest : public TestWithBrowserView {
 public:
  PermissionDashboardUnitTest()
      : TestWithBrowserView(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                            base::test::TaskEnvironment::MainThreadType::UI),
        animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kLeftHandSideActivityIndicators);
  }

  PermissionDashboardUnitTest(const PermissionDashboardUnitTest&) = delete;
  PermissionDashboardUnitTest& operator=(const PermissionDashboardUnitTest&) =
      delete;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser(), GURL("http://a.com"));
  }

  void WaitForAnimationCompletion() {
    PermissionDashboardView* dashboard_view =
        location_bar_view()
            ->permission_dashboard_controller()
            ->permission_dashboard_view();
    base::RunLoop run_loop;
    std::unique_ptr<AnimationObserver> observer =
        std::make_unique<AnimationObserver>(run_loop.QuitWhenIdleClosure());

    dashboard_view->GetIndicatorChip()->AddObserver(observer.get());

    run_loop.Run();

    dashboard_view->GetIndicatorChip()->RemoveObserver(observer.get());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  LocationBarView* location_bar_view() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

  PermissionDashboardController* dashboard_controller() {
    return location_bar_view()->permission_dashboard_controller();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Some of these tests rely on animation being enabled. This forces
  // animation on even if it's turned off in the OS.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

// TODO(crbug.com/41492809): Test LHS indicators animation on macOS as well.
#if !BUILDFLAG(IS_MAC)
// This test verifies:
// 1. Camera activity indicator chip is shown in verbose form after
// `PageSpecificContentSettings` updates camera usage.
// 2. The chip's verbose state collapses after 4 seconds.
// 3. The chip disappears after `PageSpecificContentSettings` resets camera
// usage.
TEST_F(PermissionDashboardUnitTest, DisplayLHSIndicatorForCamera) {
  PermissionDashboardController* dashboard_controller =
      location_bar_view()->permission_dashboard_controller();

  PermissionChipView* indicator_chip =
      dashboard_controller->permission_dashboard_view()->GetIndicatorChip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed});

  // Wait for the expand animation to finish.
  WaitForAnimationCompletion();

  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_TRUE(dashboard_controller->is_verbose());
  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));

  EXPECT_TRUE(
      dashboard_controller->get_collapse_timer_for_testing().IsRunning());
  EXPECT_FALSE(indicator_chip->is_animating());
  // Wait longer than 4 seconds for collapse timer to fire and the collapse
  // animation to finish.
  task_environment()->AdvanceClock(base::Milliseconds(4100));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(indicator_chip->is_animating());

  WaitForAnimationCompletion();

  EXPECT_FALSE(indicator_chip->is_animating());

  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(
      dashboard_controller->get_collapse_timer_for_testing().IsRunning());

  EXPECT_FALSE(dashboard_controller->is_verbose());

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing()
                  [ContentSettingsType::MEDIASTREAM_CAMERA]
                      .IsRunning());

  pscs->get_indicators_hiding_delay_timer_for_testing()
      [ContentSettingsType::MEDIASTREAM_CAMERA]
          .FireNow();
  EXPECT_FALSE(indicator_chip->GetVisible());
  EXPECT_FALSE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
}

// This test verifies:
// 1. Camera & Mic activity indicator chip is shown.
// 2. The chip disappears after `PageSpecificContentSettings` resets camera &
// microphone usage.
TEST_F(PermissionDashboardUnitTest, DisplayLHSIndicatorForCameraMic) {
  PermissionDashboardController* dashboard_controller =
      location_bar_view()->permission_dashboard_controller();

  PermissionChipView* indicator_chip =
      dashboard_controller->permission_dashboard_view()->GetIndicatorChip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneAccessed});

  // Wait for the expand animation to finish.
  WaitForAnimationCompletion();

  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_TRUE(
      dashboard_controller->get_collapse_timer_for_testing().IsRunning());

  EXPECT_TRUE(dashboard_controller->is_verbose());
  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  // Because indicator is displayed for both camera and mic, disabling only one
  // does not trigger a delay timer.
  EXPECT_FALSE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, false);
  ASSERT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  pscs->get_indicators_hiding_delay_timer_for_testing()
      [ContentSettingsType::MEDIASTREAM_MIC]
          .FireNow();

  // Wait for the collapse animation to finish.
  WaitForAnimationCompletion();
  EXPECT_FALSE(indicator_chip->GetVisible());
}

// This test verifies:
// 1. Camera activity indicator chip is shown.
// 2. After the Camera indicator collapsed, Microphone usage will not trigger
// expand animation because there is only one indicator for both camera and mic.
// 3. The chip does not disappears after `PageSpecificContentSettings` resets
// camera.
// 4. The chip disappears after `PageSpecificContentSettings` resets microphone
// usage.
TEST_F(PermissionDashboardUnitTest, DisplayLHSIndicatorForCameraAndThenMic) {
  PermissionDashboardController* dashboard_controller =
      location_bar_view()->permission_dashboard_controller();

  PermissionChipView* indicator_chip =
      dashboard_controller->permission_dashboard_view()->GetIndicatorChip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed});

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(
      dashboard_controller->get_collapse_timer_for_testing().IsRunning());

  WaitForAnimationCompletion();

  EXPECT_TRUE(dashboard_controller->is_verbose());
  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  // Wait longer than 4 seconds for collapse timer to fire and the collapse
  // animation to finish.
  task_environment()->AdvanceClock(base::Milliseconds(4100));
  base::RunLoop().RunUntilIdle();

  WaitForAnimationCompletion();

  EXPECT_FALSE(dashboard_controller->is_verbose());

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, true);

  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(indicator_chip->is_animating());
  // The indicator stays collapsed.
  EXPECT_FALSE(dashboard_controller->is_verbose());

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().HasAll(
      {content_settings::PageSpecificContentSettings::kCameraAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneAccessed}));

  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(indicator_chip->is_animating());
  // The indicator stays collapsed.
  EXPECT_FALSE(dashboard_controller->is_verbose());

  EXPECT_FALSE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, false);
  ASSERT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  pscs->get_indicators_hiding_delay_timer_for_testing()
      [ContentSettingsType::MEDIASTREAM_MIC]
          .FireNow();

  EXPECT_FALSE(indicator_chip->GetVisible());
}
#endif
