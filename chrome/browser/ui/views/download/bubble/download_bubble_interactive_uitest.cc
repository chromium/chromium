// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace {

// This waits for the download bubble widget to be shown.
views::NamedWidgetShownWaiter CreateDownloadBubbleDialogWaiter() {
  return views::NamedWidgetShownWaiter{views::test::AnyWidgetTestPasskey{},
                                       DownloadToolbarButtonView::kBubbleName};
}

// Wait for the bubble to show up. `waiter` should be created before this
// call, and should be for the download bubble's widget name.
auto WaitForDownloadBubbleShow(views::NamedWidgetShownWaiter& waiter) {
  return base::BindLambdaForTesting(
      [&waiter]() { waiter.WaitIfNeededAndGet(); });
}

bool IsExclusiveAccessBubbleVisible(ExclusiveAccessBubbleViews* bubble) {
  bool is_hiding = bubble->animation_for_test()->IsClosing();
  return bubble->IsShowing() || (bubble->IsVisibleForTesting() && !is_hiding);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
auto WaitForImmersiveRevealEnded(ImmersiveModeTester& immersive_tester,
                                 int expected_tab_index) {
  return base::BindLambdaForTesting(
      [&immersive_tester, index = expected_tab_index] {
        immersive_tester.VerifyTabIndexAfterReveal(index);
      });
}
#endif

class DownloadBubbleInteractiveUiTest
    : public DownloadTestBase,
      public InteractiveBrowserTestApi,
      public testing::WithParamInterface<bool> {
 public:
  DownloadBubbleInteractiveUiTest() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    enabled.push_back(feature_engagement::kIPHDownloadToolbarButtonFeature);
    enabled.push_back(safe_browsing::kDownloadBubble);
    enabled.push_back(safe_browsing::kDownloadBubbleV2);

#if BUILDFLAG(IS_MAC)
    if (IsBrowserFullscreenModeImmersive()) {
      enabled.push_back(features::kImmersiveFullscreen);
    } else {
      disabled.push_back(features::kImmersiveFullscreen);
      fake_fullscreen_window_ =
          std::make_unique<ui::test::ScopedFakeNSWindowFullscreen>();
    }
#endif  // BUILDFLAG(IS_MAC)

    test_features_.InitAndEnableFeatures(enabled, disabled);
  }

  DownloadToolbarButtonView* download_toolbar_button() {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->download_button();
  }

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
    private_test_impl().DoTestSetUp();
    SetContextWidget(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());

    // Disable the auto-close timer and animation to prevent flakiness.
    download_toolbar_button()->DisableAutoCloseTimerForTesting();
    download_toolbar_button()->DisableDownloadStartedAnimationForTesting();

    ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetFeaturePromoController()));

#if BUILDFLAG(IS_MAC)
    if (!IsBrowserFullscreenModeImmersive()) {
      // Disable the fullscreen toolbar so the download bubble recognizes this
      // as fullscreen.
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kShowFullscreenToolbar, false);
    }
#endif
  }

  void TearDownOnMainThread() override {
    SetContextWidget(nullptr);
    private_test_impl().DoTestTearDown();
    DownloadTestBase::TearDownOnMainThread();
  }

  auto DownloadBubbleIsShowingDetails(bool showing) {
    return base::BindLambdaForTesting([&, showing = showing]() {
      return showing == download_toolbar_button()->IsShowingDetails();
    });
  }

  auto DownloadBubblePromoIsActive(bool active) {
    return base::BindLambdaForTesting([&, active = active]() {
      return active ==
             BrowserView::GetBrowserViewForBrowser(browser())
                 ->GetFeaturePromoController()
                 ->IsPromoActive(
                     feature_engagement::kIPHDownloadToolbarButtonFeature);
    });
  }

  auto ChangeButtonVisibility(bool visible) {
    return base::BindLambdaForTesting([&, visible = visible]() {
      if (visible) {
        download_toolbar_button()->Show();
      } else {
        download_toolbar_button()->Hide();
      }
    });
  }

  auto ChangeBubbleVisibility(bool visible) {
    return base::BindLambdaForTesting([&, visible = visible]() {
      if (visible) {
        download_toolbar_button()->ShowDetails();
      } else {
        download_toolbar_button()->HideDetails();
      }
    });
  }

  auto DownloadTestFile() {
    GURL url = embedded_test_server()->GetURL(
        base::StrCat({"/", DownloadTestBase::kDownloadTest1Path}));
    return base::BindLambdaForTesting(
        [this, url]() { DownloadAndWait(browser(), url); });
  }

  // Check for whether the exclusive access bubble is shown ("Press Esc to
  // exit fullscreen" or other similar message).
  auto IsExclusiveAccessBubbleDisplayed(bool displayed) {
    return base::BindLambdaForTesting([&, displayed = displayed]() {
      ExclusiveAccessBubbleViews* bubble =
          BrowserView::GetBrowserViewForBrowser(browser())
              ->exclusive_access_bubble();
      return displayed ==
             (bubble ? IsExclusiveAccessBubbleVisible(bubble) : false);
    });
  }

  // Whether the exclusive access bubble, if any, is a download notification.
  auto IsExclusiveAccessBubbleForDownload(bool for_download) {
    return base::BindLambdaForTesting([&, for_download = for_download]() {
      ExclusiveAccessBubbleViews* bubble =
          BrowserView::GetBrowserViewForBrowser(browser())
              ->exclusive_access_bubble();
      return for_download ==
             (bubble ? ExclusiveAccessTest::IsBubbleDownloadNotification(bubble)
                     : false);
    });
  }

  // Toggles browser fullscreen and wait for the fullscreen notification.
  // Browser fullscreen is immersive on ChromeOS and on Mac with the feature
  // enabled.
  auto ToggleBrowserFullscreenAndWait() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    chromeos::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerChromeos*>(
            browser_view->immersive_mode_controller())
            ->controller())
        .SetupForTest();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    return [&]() {
      FullscreenNotificationObserver waiter(browser());
      chrome::ToggleFullscreenMode(browser());
      waiter.Wait();
    };
  }

  // Toggles browser fullscreen without blocking on the notification.
  auto ToggleBrowserFullscreen() {
    return [&]() { chrome::ToggleFullscreenMode(browser()); };
  }

  auto IsInImmersiveFullscreen() {
    return [&]() {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      return browser_view->GetWidget()->IsFullscreen() &&
             browser_view->immersive_mode_controller()->IsEnabled();
    };
  }

  // On Lacros, browser fullscreen mode is immersive but tab fullscreen mode is
  // not. On Mac, fullscreen mode can either be immersive or non-immersive.
  // Other platforms do not have immersive.
  bool IsBrowserFullscreenModeImmersive() { return GetParam(); }
  bool IsTabFullscreenModeImmersive() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // ChromeOS uses immersive mode for browser fullscreen only.
    return false;
#else
    // On other platforms, including Mac, tab fullscreen matches browser
    // fullscreen.
    return IsBrowserFullscreenModeImmersive();
#endif
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;

#if BUILDFLAG(IS_MAC)
  // On Mac, entering into the system fullscreen mode can tickle crashes in
  // the WindowServer (c.f. https://crbug.com/828031), so provide a fake for
  // testing.
  std::unique_ptr<ui::test::ScopedFakeNSWindowFullscreen>
      fake_fullscreen_window_;
#endif
};

// The param value represents whether browser fullscreen is immersive.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    DownloadBubbleInteractiveUiTest,
#if BUILDFLAG(IS_MAC)
    // On Mac, immersive mode is toggled by a base::Feature.
    testing::Bool()
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // On ChromeOS, immersive mode is always on for browser fullscreen. (This
    // test is only built for Lacros because the download bubble is not
    // available on Ash ChromeOS.)
    testing::Values(true)
#else
    // Other platforms do not have immersive mode.
    testing::Values(false)
#endif
);

IN_PROC_BROWSER_TEST_P(DownloadBubbleInteractiveUiTest,
                       ToolbarIconAndBubbleDetailsShownAfterDownload) {
  RunTestSequence(
      Do(DownloadTestFile()), WaitForShow(kDownloadToolbarButtonElementId),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after downloading file"),
      // Hide the bubble so it's not showing while tearing down the
      // test browser (which causes a crash on Mac).
      Do(ChangeBubbleVisibility(false)));
}

IN_PROC_BROWSER_TEST_P(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleInteractedWith_NoIPHShown) {
  RunTestSequence(
      Do(ChangeButtonVisibility(true)),
      WaitForShow(kDownloadToolbarButtonElementId),
      Check(DownloadBubbleIsShowingDetails(false),
            "Download bubble is not showing details before downloading file"),
      // Press the button to register an interaction (which should
      // suppress the IPH) which opens the main view.
      PressButton(kDownloadToolbarButtonElementId),
      // Close the main view.
      Do(ChangeBubbleVisibility(false)),
      // Now download a file to show the partial view.
      Do(DownloadTestFile()),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after downloading file"),
      // Hide the partial view. No IPH is shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false),
            "Download bubble is not showing details after hiding partial view"),
      Check(DownloadBubblePromoIsActive(false),
            "Download bubble promo is not active after hiding partial view"));
}

IN_PROC_BROWSER_TEST_P(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleShownAfterDownload_IPHShown) {
  RunTestSequence(
      Do(DownloadTestFile()), WaitForShow(kDownloadToolbarButtonElementId),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after downloading file"),
      // Hide the partial view. The IPH should be shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false),
            "Download bubble is not showing details after hiding partial view"),
      Check(DownloadBubblePromoIsActive(true),
            "Download bubble promo is active after hiding partial view"));
}

IN_PROC_BROWSER_TEST_P(
    DownloadBubbleInteractiveUiTest,
    DownloadBubbleDetailsShownAfterImmersiveFullscreenDownload) {
  // This test is only for immersive fullscreen.
  if (!IsBrowserFullscreenModeImmersive()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto immersive_tester = std::make_unique<ImmersiveModeTester>(browser());
#endif

  RunTestSequence(
      Do(ToggleBrowserFullscreenAndWait()),
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      Do(WaitForImmersiveRevealEnded(*immersive_tester,
                                     /*expected_tab_index=*/0)),
      // Delete the immersive tester to avoid issues with the next reveal,
      // triggered by the download.
      Do(base::BindLambdaForTesting([&]() { immersive_tester.reset(); })),
#endif
      Check(IsInImmersiveFullscreen(), "Immersive fullscreen is active"),
      // No download toolbar icon should be present before the download.
      EnsureNotPresent(kDownloadToolbarButtonElementId),
      // Download a file to make the partial bubble show up.
      Do(DownloadTestFile()), WaitForShow(kDownloadToolbarButtonElementId),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after downloading file"),
      // No exclusive access bubble is shown for immersive.
      Check(IsExclusiveAccessBubbleDisplayed(false),
            "Exclusive access bubble is not displayed"),
      // Hide the bubble so it's not showing while tearing down the test browser
      // (which causes a crash on Mac).
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kDownloadToolbarButtonElementId));
}

// Test that downloading a file in browser fullscreen in non-immersive mode
// results in an exclusive access bubble, and the partial view displayed after
// the browser exits fullscreen.
IN_PROC_BROWSER_TEST_P(
    DownloadBubbleInteractiveUiTest,
    ExclusiveAccessBubbleShownForBrowserFullscreenDownloadThenPartialView) {
  // This test is only for non-immersive fullscreen.
  if (IsBrowserFullscreenModeImmersive()) {
    return;
  }

  views::NamedWidgetShownWaiter dialog_waiter =
      CreateDownloadBubbleDialogWaiter();

  RunTestSequence(
      InParallel(
          Do(ToggleBrowserFullscreen()),
          InAnyContext(WaitForShow(kExclusiveAccessBubbleViewElementId))),
      // The exclusive access bubble should notify about the fullscreen change.
      Check(IsExclusiveAccessBubbleDisplayed(true),
            "Exclusive access bubble is displayed upon entering fullscreen"),
      Check(IsExclusiveAccessBubbleForDownload(false),
            "Exclusive access bubble is not for a download"),
      // Download a file to make the exclusive access bubble appear again.
      Do(DownloadTestFile()),
      // The exclusive access bubble should be displayed and should be for a
      // download.
      InAnyContext(WaitForShow(kExclusiveAccessBubbleViewElementId)),
      Check(IsExclusiveAccessBubbleDisplayed(true),
            "Exclusive access bubble is displayed after starting a download"),
      Check(IsExclusiveAccessBubbleForDownload(true),
            "Exclusive access bubble is for a download"),
      FlushEvents(),
      // Now exit fullscreen, and the partial view should be shown.
      Do(ToggleBrowserFullscreen()), FlushEvents(),
      Do(WaitForDownloadBubbleShow(dialog_waiter)),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after exiting fullscreen"),
      // Hide the bubble so it's not showing while tearing down the test browser
      // (which causes a crash on Mac).
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kDownloadToolbarButtonElementId));
}

// Test that downloading a file in tab fullscreen (not browser fullscreen)
// results in an exclusive access bubble, and the partial view displayed after
// the tab exits fullscreen.
IN_PROC_BROWSER_TEST_P(
    DownloadBubbleInteractiveUiTest,
    ExclusiveAccessBubbleShownForTabFullscreenDownloadThenPartialView) {
  // This test is only for non-immersive fullscreen.
  // TODO(chlily): Add test coverage for immersive tab fullscreen on Mac.
  if (IsTabFullscreenModeImmersive()) {
    return;
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

  // Grab the fullscreen accelerator, which is used to exit fullscreen in the
  // test. For some reason, exiting tab fullscreen via JavaScript doesn't work
  // (times out).
  ui::Accelerator fullscreen_accelerator;
#if BUILDFLAG(IS_MAC)
  // Mac uses Esc to exit tab fullscreen, not the normal fullscreen accelerator.
  fullscreen_accelerator = ui::Accelerator{ui::VKEY_ESCAPE, ui::EF_NONE};
#else
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_FULLSCREEN, &fullscreen_accelerator);
#endif

  views::NamedWidgetShownWaiter dialog_waiter =
      CreateDownloadBubbleDialogWaiter();

  RunTestSequenceInContext(
      browser()->window()->GetElementContext(),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId,
                          embedded_test_server()->GetURL("/empty.html")),
      // Enter tab fullscreen.
      InParallel(
          ExecuteJs(kWebContentsElementId,
                    "() => document.documentElement.requestFullscreen()"),
          InAnyContext(WaitForShow(kExclusiveAccessBubbleViewElementId))),
      // The exclusive access bubble should notify about the fullscreen change.
      Check(IsExclusiveAccessBubbleDisplayed(true),
            "Exclusive access bubble is displayed upon entering fullscreen"),
      Check(IsExclusiveAccessBubbleForDownload(false),
            "Exclusive access bubble is not for a download"),
      // Download a file to make the exclusive access bubble appear again.
      Do(DownloadTestFile()),
      // The exclusive access bubble should be displayed and should be for a
      // download.
      InAnyContext(WaitForShow(kExclusiveAccessBubbleViewElementId)),
      Check(IsExclusiveAccessBubbleDisplayed(true),
            "Exclusive access bubble is displayed after starting a download"),
      Check(IsExclusiveAccessBubbleForDownload(true),
            "Exclusive access bubble is for a download"),
      FlushEvents(),
      // Now exit fullscreen, and the partial view should be shown.
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator),
      FlushEvents(), Do(WaitForDownloadBubbleShow(dialog_waiter)),
      Check(DownloadBubbleIsShowingDetails(true),
            "Download bubble is showing details after exiting fullscreen"),
      // Hide the bubble so it's not showing while tearing down the test browser
      // (which causes a crash on Mac).
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kDownloadToolbarButtonElementId));
}

}  // namespace
