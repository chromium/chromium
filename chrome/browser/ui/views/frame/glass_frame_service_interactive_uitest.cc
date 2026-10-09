// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_frame_service.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "media/base/media_switches.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

class FakeThemeService : public ThemeService {
 public:
  explicit FakeThemeService(Profile* profile)
      : ThemeService(profile, GetFakeThemeHelper()) {}
  bool UsingExtensionTheme() const override {
    return is_using_extension_theme_;
  }
  void set_using_extension_theme(bool value) {
    is_using_extension_theme_ = value;
    NotifyThemeChanged();
  }

 private:
  static const ThemeHelper& GetFakeThemeHelper() {
    static base::NoDestructor<ThemeHelper> helper;
    return *helper;
  }

  bool is_using_extension_theme_ = false;
};

}  // namespace

class GlassFrameServiceInteractiveTest : public InProcessBrowserTest {
 public:
  GlassFrameServiceInteractiveTest() {
#if BUILDFLAG(IS_MAC)
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlassFrame},
        /*disabled_features=*/{media::kUseSCContentSharingPicker});
#else
    scoped_feature_list_.InitAndEnableFeature(features::kGlassFrame);
#endif  // BUILDFLAG(IS_MAC)
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  ThemeServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
                        auto service = std::make_unique<FakeThemeService>(
                            static_cast<Profile*>(context));
                        service->Init();
                        return service;
                      }));
                }));
  }

  bool GlassFrameEligibilityMatchesTabStrip(BrowserWindowInterface* browser) {
    bool is_eligible =
        GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser);
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    TabStripRegionView* tab_strip_region_view = browser_view->tab_strip_view();

    if (auto* base_region =
            views::AsViewClass<BaseTabStripRegionView>(tab_strip_region_view)) {
      if (TabStripCollectionController* controller =
              base_region->GetTabStripCollectionController()) {
        return controller->IsGlassFrame() == is_eligible;
      }
    }

    if (TabStrip* tab_strip = views::AsViewClass<TabStrip>(
            tab_strip_region_view->GetTabStripView())) {
      return tab_strip->IsGlassFrame() == is_eligible;
    }

    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UserDataFactory::ScopedOverride glass_frame_service_override_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, SingleWindowEligible) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsActivationSwap) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
  BrowserWindowInterface* const browser3 =
      CreateBrowser(browser()->GetProfile());

  // Initially browser3 is active, so it should be eligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));

  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));

  // Activate window 1.
  browser1->GetWindow()->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1);
  }));

  // Now windows 2 and 3 shouldn't be eligible anymore.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));

  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ThreeWindowsCloseMiddle) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
  BrowserWindowInterface* const browser3 =
      CreateBrowser(browser()->GetProfile());

  // Initially browser3 is active and eligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser2));

  // Close window 2.
  CloseBrowserSynchronously(browser2);

  // Window 3 should still be eligible, and window 1 is ineligible.
  EXPECT_TRUE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser3));
  EXPECT_FALSE(
      GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser3));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, CallbackNotified) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  bool browser1_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser1);
  base::CallbackListSubscription sub1 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser1, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser1_eligible));

  EXPECT_TRUE(browser1_eligible);

  // Create a second browser, which becomes the active and eligible browser.
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());
  bool browser2_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser2);
  base::CallbackListSubscription sub2 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser2, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser2_eligible));

  // Wait for the new browser to be eligible. The callback should be notified.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser2_eligible; }));
  EXPECT_FALSE(browser1_eligible);

  // Activate window 1.
  browser1->GetWindow()->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] { return browser1_eligible; }));
  EXPECT_FALSE(browser2_eligible);
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));

  // Close window 1 (the currently active/eligible window).
  CloseBrowserSynchronously(browser1);

  // The remaining window (browser2) should become eligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser2_eligible; }));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser2));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       CallbackOnlyNotifiedOnEligibilityChange) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();
  BrowserWindowInterface* const browser2 =
      CreateBrowser(browser()->GetProfile());

  ASSERT_TRUE(base::test::RunUntil(
      [&] { return glass_frame_service->IsBrowserWindowEligible(browser2); }));
  ASSERT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));

  int browser1_notifications = 0;
  base::CallbackListSubscription sub1 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser1,
          base::BindRepeating([](int* count, bool is_eligible) { ++(*count); },
                              &browser1_notifications));

  int browser2_notifications = 0;
  base::CallbackListSubscription sub2 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser2,
          base::BindRepeating([](int* count, bool is_eligible) { ++(*count); },
                              &browser2_notifications));

  // Re-activating the already-eligible browser should not notify any callbacks.
  glass_frame_service->OnBrowserActivated(browser2);
  EXPECT_EQ(browser1_notifications, 0);
  EXPECT_EQ(browser2_notifications, 0);

  // Creating a third browser makes browser3 eligible and browser2 ineligible.
  // browser1 was already ineligible so its callback should not be notified.
  BrowserWindowInterface* const browser3 =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return glass_frame_service->IsBrowserWindowEligible(browser3); }));
  EXPECT_EQ(browser1_notifications, 0);
  EXPECT_EQ(browser2_notifications, 1);

  int browser3_notifications = 0;
  base::CallbackListSubscription sub3 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser3,
          base::BindRepeating([](int* count, bool is_eligible) { ++(*count); },
                              &browser3_notifications));

  // Closing an ineligible browser (browser2) should not notify remaining
  // browsers since their eligibility does not change.
  CloseBrowserSynchronously(browser2);
  EXPECT_EQ(browser1_notifications, 0);
  EXPECT_EQ(browser3_notifications, 0);

  // Closing the eligible browser (browser3) should make browser1 eligible and
  // notify browser1's callback once.
  CloseBrowserSynchronously(browser3);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return glass_frame_service->IsBrowserWindowEligible(browser1); }));
  EXPECT_EQ(browser1_notifications, 1);
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, LocalStatePref) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  PrefService* const local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kGlassFrameEnabled));

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));

  local_state->SetBoolean(prefs::kGlassFrameEnabled, false);
  EXPECT_FALSE(local_state->GetBoolean(prefs::kGlassFrameEnabled));
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));

  local_state->SetBoolean(prefs::kGlassFrameEnabled, true);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kGlassFrameEnabled));
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       SwitchTabOrientationPreservesGlassState) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  BrowserWindowInterface* const browser_window = browser();
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser_window);
  ASSERT_TRUE(controller);

  // Initially in horizontal tabs mode and eligible for glass frame.
  GlassFrameService* glass_frame_service = GlassFrameService::GetInstance();
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch to vertical tabs mode.
  controller->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch back to horizontal tabs mode.
  controller->SetVerticalTabsEnabled(false);
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_FALSE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       SwitchTabOrientationPreservesDisabledGlassState) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  PrefService* const local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  local_state->SetBoolean(prefs::kGlassFrameEnabled, false);

  BrowserWindowInterface* const browser_window = browser();
  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser_window);
  ASSERT_TRUE(controller);

  GlassFrameService* glass_frame_service = GlassFrameService::GetInstance();
  // Initially in horizontal tabs mode and not eligible for glass frame.
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch to vertical tabs mode while glass frame is disabled.
  controller->SetVerticalTabsEnabled(true);
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_TRUE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));

  // Switch back to horizontal tabs mode while glass frame is disabled.
  controller->SetVerticalTabsEnabled(false);
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser_window));
  EXPECT_FALSE(controller->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser_window));
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       GetInstanceDoesNotConstructService) {
  EXPECT_EQ(GlassFrameService::GetInstance(), nullptr);
}
#endif  // !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       WindowCaptureIneligible) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  ASSERT_TRUE(indicator);

  blink::mojom::StreamDevices devices;
  blink::MediaStreamDevice window_video_device(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, "window_device",
      "window_device");
  window_video_device.display_media_info =
      media::mojom::DisplayMediaInformation::New(
          media::mojom::DisplayCaptureSurfaceType::WINDOW,
          /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
          /*capture_handle=*/nullptr, /*initial_zoom_level=*/100);
  devices.video_device = window_video_device;

  // Start capturing a window from browser1's active WebContents. Glass frame
  // should become disallowed while window capture is active.
  std::unique_ptr<content::MediaStreamUI> stream_ui =
      indicator->RegisterMediaStream(
          browser1->GetTabStripModel()->GetActiveWebContents(), devices);
  stream_ui->OnStarted(base::DoNothing(),
                       content::MediaStreamUI::SourceCallback(),
                       /*label=*/std::string(), /*screen_capture_ids=*/{},
                       content::MediaStreamUI::StateChangeCallback());

  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));

  // Stop capturing the window. Browser1 should become eligible again.
  stream_ui.reset();
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, BatterySaverMode) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();

  BrowserWindowInterface* const browser1 = browser();

  bool browser1_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser1);
  base::CallbackListSubscription sub1 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser1, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser1_eligible));

  // Initially BSM is not active, so browser1 is eligible.
  EXPECT_TRUE(browser1_eligible);

  // Enable Battery Saver Mode.
  g_browser_process->local_state()->SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kEnabled));

  // Wait until BSM is active. GlassFrameService should report browser1 as
  // ineligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return !browser1_eligible; }));
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));

  // Disable Battery Saver Mode.
  g_browser_process->local_state()->SetInteger(
      performance_manager::user_tuning::prefs::kBatterySaverModeState,
      static_cast<int>(performance_manager::user_tuning::prefs::
                           BatterySaverModeState::kDisabled));

  // Wait until BSM is inactive and browser1 is eligible again.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser1_eligible; }));
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       DanglingBrowserWindowPointerOnActivation) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  // Allocate a browser pointer.
  BrowserWindowInterface* deleted_browser =
      CreateBrowser(browser()->GetProfile());

  // Register a callback referencing the browser pointer before it is destroyed.
  base::CallbackListSubscription sub =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          deleted_browser, base::BindRepeating([](bool is_eligible) {}));

  // Delete browser to simulate a destroyed window while subscription is active.
  CloseBrowserSynchronously(deleted_browser);

  // Trigger OnBrowserActivated to invoke callbacks_.Notify().
  glass_frame_service->OnBrowserActivated(browser1);
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       ExtensionThemeIneligible) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  BrowserWindowInterface* const browser1 = browser();

  bool browser1_eligible =
      glass_frame_service->IsBrowserWindowEligible(browser1);
  base::CallbackListSubscription sub1 =
      glass_frame_service->RegisterGlassFrameEligibilityChangedCallback(
          browser1, base::BindRepeating(
                        [](bool* out_eligible, bool is_eligible) {
                          *out_eligible = is_eligible;
                        },
                        &browser1_eligible));

  // Initially browser1 has default theme, so it is eligible.
  EXPECT_TRUE(browser1_eligible);

  FakeThemeService* const fake_theme_service = static_cast<FakeThemeService*>(
      ThemeServiceFactory::GetForProfile(browser1->GetProfile()));
  ASSERT_TRUE(fake_theme_service);

  // Enable extension theme.
  fake_theme_service->set_using_extension_theme(true);

  // GlassFrameService should report browser1 as ineligible.
  ASSERT_TRUE(base::test::RunUntil([&] { return !browser1_eligible; }));
  EXPECT_FALSE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));

  // Remove extension theme.
  fake_theme_service->set_using_extension_theme(false);

  // Browser1 should become eligible again.
  ASSERT_TRUE(base::test::RunUntil([&] { return browser1_eligible; }));
  EXPECT_TRUE(glass_frame_service->IsBrowserWindowEligible(browser1));
  EXPECT_TRUE(GlassFrameEligibilityMatchesTabStrip(browser1));
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest, DailyMetric) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  PrefService* const local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  ASSERT_TRUE(glass_frame_service);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kGlassFrameEnabled));

  base::HistogramTester histogram_tester;
  const auto advance_day_and_report = [&]() {
    const base::Time last_time = base::Time::Now() - base::Hours(25);
    local_state->SetInt64(prefs::kGlassFrameDailySample,
                          last_time.since_origin().InMicroseconds());
    // `metrics::DailyEvent` caches the last fired timestamp in memory and only
    // reads `prefs::kGlassFrameDailySample` on its first `CheckInterval()`
    // call. Resetting the reporter creates a new `DailyEvent` that reads the
    // updated pref and immediately runs `CheckInterval()`.
    glass_frame_service->ResetMetricsReporterForTesting();
  };

  // Since the daily sample was already recorded on startup, resetting the
  // reporter before a day has elapsed should not emit another sample.
  glass_frame_service->ResetMetricsReporterForTesting();
  histogram_tester.ExpectTotalCount("Browser.GlassFrame.Enabled.Daily", 0);
  histogram_tester.ExpectTotalCount("Browser.GlassFrame.IsDefault.Daily", 0);

  // Simulate a day elapsing while glass is enabled and in its default state.
  advance_day_and_report();
  histogram_tester.ExpectUniqueSample("Browser.GlassFrame.Enabled.Daily", true,
                                      1);
  histogram_tester.ExpectUniqueSample("Browser.GlassFrame.IsDefault.Daily",
                                      true, 1);

  // Disable the glass frame pref and simulate another day elapsing.
  local_state->SetBoolean(prefs::kGlassFrameEnabled, false);
  advance_day_and_report();
  histogram_tester.ExpectBucketCount("Browser.GlassFrame.Enabled.Daily", false,
                                     1);
  histogram_tester.ExpectTotalCount("Browser.GlassFrame.Enabled.Daily", 2);
  histogram_tester.ExpectBucketCount("Browser.GlassFrame.IsDefault.Daily",
                                     false, 1);
  histogram_tester.ExpectTotalCount("Browser.GlassFrame.IsDefault.Daily", 2);
}

IN_PROC_BROWSER_TEST_F(GlassFrameServiceInteractiveTest,
                       HasMultipleOpenProfiles) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  GlassFrameService* const glass_frame_service =
      GlassFrameService::GetInstance();
  ASSERT_TRUE(glass_frame_service);

  int multiple_profiles_notifications = 0;
  base::CallbackListSubscription sub =
      glass_frame_service->RegisterMultipleOpenProfilesChangedCallback(
          base::BindRepeating([](int* count) { (*count)++; },
                              &multiple_profiles_notifications));

  // Initially only a single profile is loaded.
  EXPECT_FALSE(glass_frame_service->HasMultipleOpenProfiles());

  // Opening another normal window with the same profile should not count as
  // multiple profiles or notify the callback.
  BrowserWindowInterface* const same_profile_browser =
      CreateBrowser(browser()->GetProfile());
  EXPECT_FALSE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 0);
  CloseBrowserSynchronously(same_profile_browser);
  EXPECT_EQ(multiple_profiles_notifications, 0);

  // Opening a popup window should not notify the callback.
  Profile* const otr_profile =
      browser()->GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  BrowserWindowInterface* const popup_browser =
      CreateBrowserForPopup(otr_profile);
  EXPECT_FALSE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 0);
  CloseBrowserSynchronously(popup_browser);
  EXPECT_EQ(multiple_profiles_notifications, 0);

  // Creating a second profile and opening a normal browser window should report
  // multiple open profiles and notify once.
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  const base::FilePath new_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& second_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  BrowserWindowInterface* const second_profile_browser_1 =
      CreateBrowser(&second_profile);
  EXPECT_TRUE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 1);

  // Opening a second window in the second profile should keep multiple open
  // profiles true without notifying again.
  BrowserWindowInterface* const second_profile_browser_2 =
      CreateBrowser(&second_profile);
  EXPECT_TRUE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 1);

  // Closing one of the second profile windows keeps multiple open profiles true
  // without notifying.
  CloseBrowserSynchronously(second_profile_browser_2);
  EXPECT_TRUE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 1);

  // Closing the remaining second profile window returns to a single open
  // profile and notifies once more.
  CloseBrowserSynchronously(second_profile_browser_1);
  EXPECT_FALSE(glass_frame_service->HasMultipleOpenProfiles());
  EXPECT_EQ(multiple_profiles_notifications, 2);
}

#if BUILDFLAG(IS_MAC)
class GlassFrameServiceOptInPromoInteractiveTest
    : public InteractiveFeaturePromoTestMixin<
          GlassFrameServiceInteractiveTest> {
 public:
  GlassFrameServiceOptInPromoInteractiveTest()
      : InteractiveFeaturePromoTestMixin(
            InteractiveFeaturePromoTestApi::UseDefaultTrackerAllowingPromos(
                {feature_engagement::kIPHGlassFrameOptInFeature})) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlassFrame,
        {{features::kGlassFrameEnabledByDefault.name, "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceOptInPromoInteractiveTest,
                       ShowsOptInPromoOnStartup) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTabContents);
  RunTestSequence(WaitForPromo(feature_engagement::kIPHGlassFrameOptInFeature),
                  PressDefaultPromoButton(),
                  InstrumentTab(kSettingsTabContents, 1),
                  WaitForWebContentsReady(
                      kSettingsTabContents,
                      chrome::GetSettingsUrl(chrome::kAppearanceSubPage)),
                  InAnyContext(WaitForShow(kTabStylingSettingElementId)));
}
#endif  // BUILDFLAG(IS_MAC)

class GlassFrameServiceOptOutIphInteractiveTest
    : public InteractiveFeaturePromoTestMixin<
          GlassFrameServiceInteractiveTest> {
 public:
  GlassFrameServiceOptOutIphInteractiveTest()
      : InteractiveFeaturePromoTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHGlassFrameOptOutFeature})) {}
};

IN_PROC_BROWSER_TEST_F(GlassFrameServiceOptOutIphInteractiveTest,
                       OptOutIphShowsOnStartup) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP();
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTabContents);
  RunTestSequence(WaitForPromo(feature_engagement::kIPHGlassFrameOptOutFeature),
                  PressNonDefaultPromoButton(),
                  InstrumentTab(kSettingsTabContents, 1),
                  WaitForWebContentsReady(
                      kSettingsTabContents,
                      chrome::GetSettingsUrl(chrome::kAppearanceSubPage)),
                  InAnyContext(WaitForShow(kTabStylingSettingElementId)));
}
