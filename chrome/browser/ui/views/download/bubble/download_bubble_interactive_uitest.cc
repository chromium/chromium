// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#endif

namespace {

#if !BUILDFLAG(IS_MAC)
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
#endif

class DownloadBubbleInteractiveUiTest : public DownloadTestBase,
                                        public InteractiveBrowserTestApi {
 public:
  DownloadBubbleInteractiveUiTest() {
    test_features_.InitAndEnableFeatures(
        {
          feature_engagement::kIPHDownloadToolbarButtonFeature,
              safe_browsing::kDownloadBubble, safe_browsing::kDownloadBubbleV2
#if BUILDFLAG(IS_MAC)
              ,
              features::kImmersiveFullscreen
#endif  // BUILDFLAG(IS_MAC)
        },
        {});
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

#if !BUILDFLAG(IS_MAC)
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
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
  auto ToggleFullscreen() {
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  auto IsInImmersiveFullscreen() {
    return [&]() {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      return browser_view->GetWidget()->IsFullscreen() &&
             browser_view->immersive_mode_controller()->IsEnabled();
    };
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)

  bool IsPartialViewEnabled() {
    return download::IsDownloadBubblePartialViewEnabled(browser()->profile());
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ToolbarIconAndBubbleDetailsShownAfterDownload) {
  RunTestSequence(Do(DownloadTestFile()),
                  WaitForShow(kToolbarDownloadButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
                  // Hide the bubble so it's not showing while tearing down the
                  // test browser (which causes a crash on Mac).
                  Do(ChangeBubbleVisibility(false)));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleInteractedWith_NoIPHShown) {
  RunTestSequence(Do(ChangeButtonVisibility(true)),
                  WaitForShow(kToolbarDownloadButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  // Press the button to register an interaction (which should
                  // suppress the IPH) which opens the main view.
                  PressButton(kToolbarDownloadButtonElementId),
                  // Close the main view.
                  Do(ChangeBubbleVisibility(false)),
                  // Now download a file to show the partial view, if enabled.
                  Do(DownloadTestFile()),
                  Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
                  // Hide the partial view, if enabled. No IPH is shown.
                  Do(ChangeBubbleVisibility(false)),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  Check(DownloadBubblePromoIsActive(false)));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DownloadBubbleShownAfterDownload_IPHShown) {
  RunTestSequence(Do(DownloadTestFile()),
                  WaitForShow(kToolbarDownloadButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
                  // Hide the partial view, if enabled. The IPH should be shown.
                  Do(ChangeBubbleVisibility(false)),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  Check(DownloadBubblePromoIsActive(IsPartialViewEnabled())));
}

// This test is only for ChromeOS and Mac where we have immersive fullscreen.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ToolbarIconShownAfterImmersiveFullscreenDownload) {
  RunTestSequence(
      Do(ToggleFullscreen()),
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      // This cannot be enabled yet for ChromeOS because it would be flaky, due
      // to the delay between server and client agreeing on immersive state.
      // TODO(crbug.com/1448281): Enable this check for ChromeOS.
      Check(IsInImmersiveFullscreen()),
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
      // No download toolbar icon should be present before the download.
      EnsureNotPresent(kToolbarDownloadButtonElementId),
      // Download a file to make the partial bubble show up, if enabled.
      Do(DownloadTestFile()),
      // This step is fine and won't be flaky on ChromeOS, because waiting for
      // the element to show includes waiting for the server to notify us that
      // we are in immersive mode.
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Hide the bubble, if enabled, so it's not showing while tearing down the
      // test browser (which causes a crash on Mac).
      // TODO(chlily): Rewrite this test to interact with the UI instead of
      // hiding the bubble artificially, to properly test user journeys.
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kToolbarDownloadButtonElementId));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)

// This test is only for Lacros, where tab fullscreen is non-immersive, and
// other platforms, where fullscreen is not immersive.
// TODO(chlily): Add test coverage for Mac.
#if !BUILDFLAG(IS_MAC)
// Test that downloading a file in tab fullscreen (not browser fullscreen)
// results in an exclusive access bubble, and the partial view, if enabled, is
// displayed after the tab exits fullscreen.
IN_PROC_BROWSER_TEST_F(
    DownloadBubbleInteractiveUiTest,
    ExclusiveAccessBubbleShownForTabFullscreenDownloadThenPartialView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

  // Grab the fullscreen accelerator, which is used to exit fullscreen in the
  // test. For some reason, exiting tab fullscreen via JavaScript doesn't work
  // (times out).
  ui::Accelerator fullscreen_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_FULLSCREEN, &fullscreen_accelerator);

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
      // Now exit fullscreen, and the partial view, if enabled, should be shown.
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator),
      FlushEvents(),
      If([&]() { return IsPartialViewEnabled(); },
         Steps(Do(WaitForDownloadBubbleShow(dialog_waiter)),
               Check(DownloadBubbleIsShowingDetails(true),
                     "Download bubble is showing details after exiting "
                     "fullscreen"))),
      // TODO(chlily): Rewrite this test to interact with the UI instead of
      // hiding the bubble artificially, to properly test user journeys.
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kToolbarDownloadButtonElementId));
}
#endif

}  // namespace
