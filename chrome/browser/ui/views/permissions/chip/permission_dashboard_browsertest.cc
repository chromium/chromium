// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation_test_api.h"

class AnimationObserver : public PermissionChipInterface::Observer {
 public:
  explicit AnimationObserver(base::OnceClosure quit_closure)
      : animation_complete_callback_(std::move(quit_closure)) {}

  void OnAnimationEnded() {
    if (animation_complete_callback_) {
      std::move(animation_complete_callback_).Run();
    }
  }

  // PermissionChipInterface::Observer
  void OnChipVisibilityChanged(bool is_visible) override {}
  void OnExpandAnimationEnded() override { OnAnimationEnded(); }
  void OnCollapseAnimationEnded() override { OnAnimationEnded(); }

 private:
  base::OnceClosure animation_complete_callback_;
};

class PermissionDashboardBrowserTest : public InProcessBrowserTest {
 public:
  PermissionDashboardBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kLeftHandSideActivityIndicators);
  }

  PermissionDashboardBrowserTest(const PermissionDashboardBrowserTest&) =
      delete;
  PermissionDashboardBrowserTest& operator=(
      const PermissionDashboardBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.com", "/empty.html")));
  }

  void WaitForAnimationCompletion() {
    PermissionChipInterface* chip = indicator_chip();
    base::RunLoop run_loop;
    std::unique_ptr<AnimationObserver> observer =
        std::make_unique<AnimationObserver>(run_loop.QuitWhenIdleClosure());

    chip->AddObserver(observer.get());

    run_loop.Run();

    chip->RemoveObserver(observer.get());
  }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  LocationBar* location_bar() {
    return browser()->GetFeatures().location_bar();
  }

  PermissionDashboardController* dashboard_controller() {
    return location_bar()->GetPermissionDashboardController();
  }

  PermissionChipInterface* indicator_chip() {
    return dashboard_controller()->permission_dashboard()->GetIndicatorChip();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Some of these tests rely on animation being enabled. This forces
  // animation on even if it's turned off in the OS.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

// TODO(crbug.com/41492809): Test LHS indicators animation on macOS as well.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(PermissionDashboardBrowserTest,
                       DisplayLHSIndicatorTooltip) {
  PermissionChipInterface* chip = indicator_chip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  // 1. Test Camera
  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed});

  // Wait for the expand animation to finish.
  WaitForAnimationCompletion();

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_EQ(chip->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_CAMERA_ACCESSED));

  // 2. Test Microphone
  // Turn off Camera first
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);
  // Turn on Mic
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, true);

  WaitForAnimationCompletion();

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_EQ(chip->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED));

  // 3. Test Camera + Microphone
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  // Transitioning to both will not trigger animation if chip is already
  // visible.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return chip->GetTooltipText() ==
           l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED);
  }));

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_EQ(chip->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED));

  // 4. Test turn off Camera after Camera + Microphone were enabled
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  // Transitioning to both will not trigger animation if chip is already
  // visible.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return chip->GetTooltipText() ==
           l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED);
  }));

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_EQ(chip->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED));
}

// This test verifies:
// 1. Camera activity indicator chip is shown in verbose form after
// `PageSpecificContentSettings` updates camera usage.
// 2. The chip's verbose state collapses after 4 seconds.
// 3. The chip disappears after `PageSpecificContentSettings` resets camera
// usage.
IN_PROC_BROWSER_TEST_F(PermissionDashboardBrowserTest,
                       DisplayLHSIndicatorForCamera) {
  PermissionChipInterface* chip = indicator_chip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed});

  // Wait for the expand animation to finish.
  WaitForAnimationCompletion();

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_TRUE(dashboard_controller()->is_verbose());
  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));

  EXPECT_TRUE(
      dashboard_controller()->get_collapse_timer_for_testing().IsRunning());
  EXPECT_FALSE(chip->IsAnimating());
  // Trigger collapse timer to fire and wait for the collapse animation to
  // finish.
  dashboard_controller()->get_collapse_timer_for_testing().FireNow();
  EXPECT_TRUE(base::test::RunUntil([&]() { return chip->IsAnimating(); }));

  WaitForAnimationCompletion();

  EXPECT_FALSE(chip->IsAnimating());

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_FALSE(
      dashboard_controller()->get_collapse_timer_for_testing().IsRunning());

  EXPECT_FALSE(dashboard_controller()->is_verbose());

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return pscs->get_indicators_hiding_delay_timer_for_testing().contains(
        ContentSettingsType::MEDIASTREAM_CAMERA);
  }));

  ASSERT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing()
                  [ContentSettingsType::MEDIASTREAM_CAMERA]
                      .IsRunning());

  pscs->get_indicators_hiding_delay_timer_for_testing()
      [ContentSettingsType::MEDIASTREAM_CAMERA]
          .FireNow();
  EXPECT_FALSE(chip->GetVisible());
  EXPECT_FALSE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
}

// This test verifies:
// 1. Camera & Mic activity indicator chip is shown.
// 2. The chip disappears after `PageSpecificContentSettings` resets camera &
// microphone usage.
IN_PROC_BROWSER_TEST_F(PermissionDashboardBrowserTest,
                       DisplayLHSIndicatorForCameraMic) {
  PermissionChipInterface* chip = indicator_chip();

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

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_TRUE(
      dashboard_controller()->get_collapse_timer_for_testing().IsRunning());

  EXPECT_TRUE(dashboard_controller()->is_verbose());
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
  EXPECT_FALSE(chip->GetVisible());
}

// This test verifies:
// 1. Camera activity indicator chip is shown.
// 2. After the Camera indicator collapsed, Microphone usage will not trigger
// expand animation because there is only one indicator for both camera and mic.
// 3. The chip does not disappears after `PageSpecificContentSettings` resets
// camera.
// 4. The chip disappears after `PageSpecificContentSettings` resets microphone
// usage.
IN_PROC_BROWSER_TEST_F(PermissionDashboardBrowserTest,
                       DisplayLHSIndicatorForCameraAndThenMic) {
  PermissionChipInterface* chip = indicator_chip();

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      GURL("http://a.com"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed});

  // After the camera is accessed, wait for the animation to finish, so that all
  // timers and UI states are properly initialized.
  WaitForAnimationCompletion();

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_TRUE(
      dashboard_controller()->get_collapse_timer_for_testing().IsRunning());

  EXPECT_TRUE(dashboard_controller()->is_verbose());
  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_FALSE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  // Trigger collapse timer to fire and wait for the collapse animation to
  // finish.
  dashboard_controller()->get_collapse_timer_for_testing().FireNow();
  EXPECT_TRUE(base::test::RunUntil([&]() { return chip->IsAnimating(); }));

  WaitForAnimationCompletion();

  EXPECT_FALSE(dashboard_controller()->is_verbose());

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, true);

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_FALSE(chip->IsAnimating());
  // The indicator stays collapsed.
  EXPECT_FALSE(dashboard_controller()->is_verbose());

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().HasAll(
      {content_settings::PageSpecificContentSettings::kCameraAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneAccessed}));

  EXPECT_TRUE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  EXPECT_TRUE(chip->GetVisible());
  EXPECT_FALSE(chip->IsAnimating());
  // The indicator stays collapsed.
  EXPECT_FALSE(dashboard_controller()->is_verbose());

  EXPECT_FALSE(
      pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_TRUE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_MIC, false);
  ASSERT_TRUE(pscs->get_indicators_hiding_delay_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  pscs->get_indicators_hiding_delay_timer_for_testing()
      [ContentSettingsType::MEDIASTREAM_MIC]
          .FireNow();

  EXPECT_FALSE(chip->GetVisible());
}
#endif
