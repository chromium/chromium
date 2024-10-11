// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/chromeos/test_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands.h"
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

// TODO(chlily): Deduplicate this helper class into a test utils file.
class TestDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    GetDownloadIdReceiverCallback().Run(download::DownloadItem::kInvalidId + 1);
  }
  ~TestDownloadManagerDelegate() override {}

  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      download::DownloadTargetCallback* callback) override {
    auto set_dangerous = [](download::DownloadTargetCallback callback,
                            download::DownloadTargetInfo target_info) {
      target_info.danger_type = download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL;
      std::move(callback).Run(std::move(target_info));
    };

    download::DownloadTargetCallback dangerous_callback =
        base::BindOnce(set_dangerous, std::move(*callback));
    bool run = ChromeDownloadManagerDelegate::DetermineDownloadTarget(
        item, &dangerous_callback);
    // ChromeDownloadManagerDelegate::DetermineDownloadTarget() needs to run the
    // |callback|.
    DCHECK(run);
    DCHECK(!dangerous_callback);
    return true;
  }
};

class DownloadBubbleInteractiveUiTest
    : public InteractiveFeaturePromoTestT<DownloadTestBase> {
 public:
  DownloadBubbleInteractiveUiTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHDownloadEsbPromoFeature})) {
#if BUILDFLAG(IS_MAC)
    test_features_.InitWithFeatures({features::kImmersiveFullscreen}, {});
#endif  // BUILDFLAG(IS_MAC)
  }

  DownloadToolbarButtonView* download_toolbar_button() {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->toolbar()->download_button();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InteractiveFeaturePromoTestT::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTestT::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Disable the auto-close timer and animation to prevent flakiness.
    download_toolbar_button()->DisableAutoCloseTimerForTesting();
    download_toolbar_button()->DisableDownloadStartedAnimationForTesting();
  }

  auto DownloadBubbleIsShowingDetails(bool showing) {
    return base::BindOnce(
        [](DownloadToolbarButtonView* download_toolbar_button, bool showing) {
          return showing == download_toolbar_button->IsShowingDetails();
        },
        download_toolbar_button(), showing);
  }

  // Whether the download bubble's widget is showing and active.
  auto DownloadBubbleIsActive(bool active) {
    return base::BindOnce(
        [](DownloadToolbarButtonView* download_toolbar_button, bool active) {
          if (!download_toolbar_button->IsShowingDetails() ||
              !download_toolbar_button->bubble_contents_for_testing()
                   ->GetWidget()) {
            return false;
          }
          return active ==
                 download_toolbar_button->bubble_contents_for_testing()
                     ->GetWidget()
                     ->IsActive();
        },
        download_toolbar_button(), active);
  }

  auto DownloadBubblePromoIsActive(bool active, const base::Feature& feature) {
    return base::BindOnce(
        [](DownloadToolbarButtonView* download_toolbar_button, Browser* browser,
           bool active, const base::Feature& feature) {
          return active == BrowserView::GetBrowserViewForBrowser(browser)
                               ->GetFeaturePromoControllerForTesting()
                               ->IsPromoActive(feature);
        },
        download_toolbar_button(), browser(), active, std::cref(feature));
  }

  auto ChangeButtonVisibility(bool visible) {
    return base::BindOnce(
        [](DownloadToolbarButtonView* download_toolbar_button, bool visible) {
          if (visible) {
            download_toolbar_button->Show();
          } else {
            download_toolbar_button->Hide();
          }
        },
        download_toolbar_button(), visible);
  }

  auto ChangeBubbleVisibility(bool visible) {
    return base::BindOnce(
        [](DownloadToolbarButtonView* download_toolbar_button, bool visible) {
          if (visible) {
            download_toolbar_button->ShowDetails();
          } else {
            download_toolbar_button->HideDetails();
          }
        },
        download_toolbar_button(), visible);
  }

  auto DownloadTestFile() {
    GURL url = embedded_test_server()->GetURL(
        base::StrCat({"/", DownloadTestBase::kDownloadTest1Path}));
    return base::BindLambdaForTesting(
        [this, url]() { DownloadAndWait(browser(), url); });
  }

  auto DownloadDangerousTestFile() {
    // Set up the fake delegate that forces the download to be malicious.
    std::unique_ptr<TestDownloadManagerDelegate> test_delegate(
        new TestDownloadManagerDelegate(browser()->profile()));
    DownloadCoreServiceFactory::GetForBrowserContext(browser()->profile())
        ->SetDownloadManagerDelegateForTesting(std::move(test_delegate));
    GURL url = embedded_test_server()->GetURL(
        DownloadTestBase::kDangerousMockFilePath);

    return base::BindLambdaForTesting([this, url]() {
      std::unique_ptr<content::DownloadTestObserver> waiter{
          DangerousDownloadWaiter(
              browser(), /*num_downloads=*/1,
              content::DownloadTestObserver::DangerousDownloadAction::
                  ON_DANGEROUS_DOWNLOAD_QUIT)};
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
      waiter->WaitForFinished();
    });
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
  auto EnterImmersiveFullscreen() {
    return [&]() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      ChromeOSBrowserUITest::EnterImmersiveFullscreenMode(browser());
#else  // BUILDFLAG(IS_MAC)
      ui_test_utils::ToggleFullscreenModeAndWait(browser());
#endif
    };
  }

  auto IsInImmersiveFullscreen() {
    return [&]() {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      return browser_view->GetWidget()->IsFullscreen() &&
             browser_view->immersive_mode_controller()->IsEnabled();
    };
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)

  bool IsPartialViewEnabled() {
    return download::IsDownloadBubblePartialViewEnabled(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList test_features_;

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
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
                       DownloadBubbleMainView) {
  RunTestSequence(Do(ChangeButtonVisibility(true)),
                  WaitForShow(kToolbarDownloadButtonElementId),
                  Check(DownloadBubbleIsShowingDetails(false)),
                  // Press the button to open the main view.
                  PressButton(kToolbarDownloadButtonElementId),
                  // Close the main view.
                  Do(ChangeBubbleVisibility(false)),
                  // Now download a file to show the partial view, if enabled.
                  Do(DownloadTestFile()),
                  Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
                  // Hide the partial view, if enabled.
                  Do(ChangeBubbleVisibility(false)),
                  Check(DownloadBubbleIsShowingDetails(false)));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DangerousDownloadShowsEsbIphPromo_WhenAutomaticClose) {
  RunTestSequence(
      Do(DownloadDangerousTestFile()),
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Hide the partial view, if enabled. The IPH should be shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false)),
      If([&]() { return IsPartialViewEnabled(); },
         Steps(InAnyContext(WaitForShow(user_education::HelpBubbleView::
                                            kHelpBubbleElementIdForTesting)),
               Check(DownloadBubblePromoIsActive(
                   IsPartialViewEnabled(),
                   feature_engagement::kIPHDownloadEsbPromoFeature)))));
}

IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       DangerousDownloadShowsEsbIphPromo_WhenUserClicksAway) {
  RunTestSequence(
      Do(DownloadDangerousTestFile()),
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Click outside (at the center point of the browser) to close the bubble.
      MoveMouseTo(kBrowserViewElementId), ClickMouse(),
      EnsureNotPresent(kToolbarDownloadBubbleElementId),
      Check(DownloadBubbleIsShowingDetails(false),
            "Bubble is closed after clicking outside of it."),
      If([&]() { return IsPartialViewEnabled(); },
         Steps(InAnyContext(WaitForShow(user_education::HelpBubbleView::
                                            kHelpBubbleElementIdForTesting)),
               Check(DownloadBubblePromoIsActive(
                   IsPartialViewEnabled(),
                   feature_engagement::kIPHDownloadEsbPromoFeature)))));
}

IN_PROC_BROWSER_TEST_F(
    DownloadBubbleInteractiveUiTest,
    DangerousDownloadDoesNotShowEsbIphPromo_WhenSafeBrowsingDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);
  RunTestSequence(
      Do(DownloadDangerousTestFile()),
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Hide the partial view, if enabled. The IPH should not be shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false)),
      Check(DownloadBubblePromoIsActive(
          false, feature_engagement::kIPHDownloadEsbPromoFeature)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadBubbleInteractiveUiTest,
    DangerousDownloadDoesNotShowEsbIphPromo_WhenEnhancedSafeBrowsingEnabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                               true);
  RunTestSequence(
      Do(DownloadDangerousTestFile()),
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Hide the partial view, if enabled. The IPH should not be shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false)),
      Check(DownloadBubblePromoIsActive(
          false, feature_engagement::kIPHDownloadEsbPromoFeature)));
}

IN_PROC_BROWSER_TEST_F(
    DownloadBubbleInteractiveUiTest,
    DangerousDownloadDoesNotShowEsbIphPromo_WhenSafeBrowsingSetByPolicy) {
  policy::PolicyMap policy;
  policy.Set(
      policy::key::kSafeBrowsingProtectionLevel, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(safe_browsing::SafeBrowsingPolicyHandler::
                                       ProtectionLevel::kStandardProtection)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policy);

  EXPECT_TRUE(safe_browsing::SafeBrowsingPolicyHandler::
                  IsSafeBrowsingProtectionLevelSetByPolicy(
                      browser()->profile()->GetPrefs()));
  RunTestSequence(
      Do(DownloadDangerousTestFile()),
      WaitForShow(kToolbarDownloadButtonElementId),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled())),
      // Hide the partial view, if enabled. The IPH should not be shown.
      Do(ChangeBubbleVisibility(false)),
      Check(DownloadBubbleIsShowingDetails(false)),
      Check(DownloadBubblePromoIsActive(
          false, feature_engagement::kIPHDownloadEsbPromoFeature)));
}
#endif

// This test is only for ChromeOS and Mac where we have immersive fullscreen.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ToolbarIconShownAfterImmersiveFullscreenDownload) {
  RunTestSequence(
      Do(EnterImmersiveFullscreen()), Check(IsInImmersiveFullscreen()),
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

      // Now exit fullscreen, and the partial view, if enabled, should be shown.
      SendAccelerator(kBrowserViewElementId, fullscreen_accelerator),

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

// Tests that the partial view does not steal focus from the web contents, and
// that the partial view is still closable when clicking outside of it, and that
// the main view is focused when shown.
IN_PROC_BROWSER_TEST_F(DownloadBubbleInteractiveUiTest,
                       ClosePartialBubbleOnClick) {
  RunTestSequence(
      // Download a test file so that the partial view shows up.
      Do(DownloadTestFile()),
      InAnyContext(WaitForShow(kToolbarDownloadButtonElementId)),
      Check(DownloadBubbleIsShowingDetails(IsPartialViewEnabled()),
            "Partial view shows after download, if enabled."),
      If([&] { return IsPartialViewEnabled(); },
         // The bubble, if enabled, should be shown as inactive to avoid
         // stealing focus from the page.
         Steps(Check(DownloadBubbleIsActive(false),
                     "Partial view, if enabled, is inactive."))),
      // Click outside (at the center point of the browser) to close the bubble.
      MoveMouseTo(kBrowserViewElementId), ClickMouse(),
      EnsureNotPresent(kToolbarDownloadBubbleElementId),
      Check(DownloadBubbleIsShowingDetails(false),
            "Bubble is closed after clicking outside of it."),
      // Click on the toolbar button to show the main view, which should always
      // have focus.
      PressButton(kToolbarDownloadButtonElementId),
      WaitForShow(kToolbarDownloadBubbleElementId),
      Check(DownloadBubbleIsShowingDetails(true),
            "Main view is shown after clicking button."),
      // The main view widget should be active.
      Check(DownloadBubbleIsActive(true), "Main view is active."),
      // Hide the bubble so it's not showing while tearing down the
      // test browser (which causes a crash on Mac).
      Do(ChangeBubbleVisibility(false)), Do(ChangeButtonVisibility(false)),
      WaitForHide(kToolbarDownloadButtonElementId));
}

}  // namespace
