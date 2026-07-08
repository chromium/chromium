// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace send_tab_to_self {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7757185";
constexpr char kToastDismissedHistogram[] =
    "Toast.SendTabToSelfTabsOpenedInBackground.Dismissed";
constexpr char kLocalCacheGuid[] = "local_cache_guid";

class SendTabToSelfToolbarIconControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              send_tab_to_self::StubSendTabToSelfSyncService>();
        }));
  }

  SendTabToSelfToolbarIconController* controller() {
    return static_cast<SendTabToSelfToolbarIconController*>(
        SendTabToSelfClientServiceFactory::GetForProfile(
            browser()->GetProfile())
            ->GetReceivingUiHandler());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  auto SimulateReceivingNewEntries() {
    return Do([this]() {
      FakeSendTabToSelfModel* model =
          static_cast<StubSendTabToSelfSyncService*>(
              SendTabToSelfSyncServiceFactory::GetForProfile(
                  browser()->GetProfile()))
              ->GetFakeSendTabToSelfModel();
      model->SetLocalCacheGuid(kLocalCacheGuid);
      GURL url_1("https://www.example-a.com");
      GURL url_2("https://www.example-b.com");
      model->AddEntriesRemotely({{url_1, "a site", kLocalCacheGuid},
                                 {url_2, "b site", kLocalCacheGuid}});
    });
  }

  auto StopToastTimer() {
    return Do([this]() {
      browser()
          ->browser_window_features()
          ->toast_controller()
          ->GetToastCloseTimerForTesting()
          ->Stop();
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_{kSendTabToSelfAutoOpen};
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerInteractiveUiTest,
                       AutoOpenNewEntriesInForegroundIfActive) {
  RunTestSequence(
      InstrumentTab(kFirstTab, 0), SimulateReceivingNewEntries(),
      // Check that two new tabs have been opened and the first one is active.
      PollUntil([this]() { return browser()->tab_strip_model()->count() == 3; },
                "polling until all the tabs are opened"),
      PollUntil(
          [this]() {
            return browser()->tab_strip_model()->active_index() == 1;
          },
          "polling until the new tab is active"),

      // Check that the toast is shown.
      WaitForShow(toasts::ToastView::kToastViewId),
      EnsureNotPresent(toasts::ToastView::kToastCloseButton),
      EnsureNotPresent(toasts::ToastView::kToastActionButton),

      // Stop the toast timer to prevent the toast from disappearing before
      // screenshot is taken.
      StopToastTimer(),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Screenshot(toasts::ToastView::kToastViewId,
                 /*screenshot_name=*/"send_tab_to_self_foreground_toast",
                 /*baseline_cl=*/kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarIconControllerInteractiveUiTest,
                       AutoOpenPendingEntriesAsBackgroundTabsOnActivation) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "Linux window activation issues on Wayland";
  }
#endif
  base::HistogramTester histogram_tester;

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kIncognitoTab);
  const ui::ElementContext kOriginalContext =
      browser_view()->GetElementContext();

  RunTestSequence(
      InstrumentTab(kFirstTab, 0),
      // Ensure web contents is focused instead of the omnibox so the toast
      // isn't hidden by omnibox focus changes when the window is reactivated.
      FocusWebContents(kFirstTab),

      // Create an incognito browser and remove the current browser from focus.
      InstrumentNextTab(kIncognitoTab, AnyBrowser()),
      Do([this]() { CreateIncognitoBrowser(); }),
      InAnyContext(WaitForShow(kIncognitoTab)),
      InSameContextAs(kIncognitoTab, ActivateSurface(kBrowserViewElementId)),

      // Switch context to the original browser and verify that it is inactive.
      InContext(kOriginalContext,
                CheckViewProperty(kBrowserViewElementId, &BrowserView::IsActive,
                                  false)),

      // Add new entries while the browser is in the background.
      SimulateReceivingNewEntries(),

      // Activate the browser and check that the entries are opened in the
      // background.
      InContext(kOriginalContext, ActivateSurface(kBrowserViewElementId)),
      // Check that two new tabs have been opened in the background.
      PollUntil([this]() { return browser()->tab_strip_model()->count() == 3; },
                "polling until all the tabs are opened"),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),

      // Check that the toast is shown.
      WaitForShow(toasts::ToastView::kToastViewId),
      EnsurePresent(toasts::ToastView::kToastCloseButton),
      EnsurePresent(toasts::ToastView::kToastActionButton),

      // Stop the toast timer to prevent the toast from disappearing before
      // screenshot is taken.
      StopToastTimer(),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Screenshot(toasts::ToastView::kToastViewId,
                 /*screenshot_name=*/"send_tab_to_self_background_toast",
                 /*baseline_cl=*/kScreenshotBaselineCL),

      // Clicking on the toast action button should switch to the latest tabs
      // opened in the background.
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForHide(toasts::ToastView::kToastViewId), Do([&]() {
        histogram_tester.ExpectUniqueSample(
            kToastDismissedHistogram, toasts::ToastCloseReason::kActionButton,
            1);
      }),
      PollUntil(
          [this]() {
            return browser()->tab_strip_model()->active_index() == 1;
          },
          "polling until the latest tab is active"));
}

}  // namespace

}  // namespace send_tab_to_self
