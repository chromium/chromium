// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "components/permissions/permission_request_manager.h"
#include "components/remote_cocoa/browser/window.h"
#include "third_party/blink/public/common/features_generated.h"
#endif

namespace web_app {

namespace {

class WebAppInstallFlowBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppInstallFlowBrowserTest() = default;

  ~WebAppInstallFlowBrowserTest() override = default;

  void AdvanceToDoneAndAccept(views::Widget* widget) {
    ASSERT_TRUE(widget);
    AdvanceToProgressStep(widget);
    AdvanceToSuccessfulStep(widget);

    views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
    AcceptWidgetAndMoveForward(widget);
    destroyed_waiter.Wait();
  }

  // Waiter for the new install flow dialog. Waits for the progress bar to be
  // complete, and then moves the installation forward until it reaches the
  // "Success" view. Successfully accepts that dialog, which should result in an
  // app being installed.
  // The input here should be the "WebAppInstallerOptionsView", this will
  // CHECK-fail if any other view is passed as an input here.
  void ForwardThroughProgressViewAndAcceptDialog(views::Widget* widget) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
    AdvanceToSuccessfulStep(widget);

    // We should be in the "Successful" view step now. Accept it to finish
    // installation.
    AcceptWidgetAndMoveForward(widget);
    destroyed_waiter.Wait();
  }

  // Helper that moves the installation forward from the initial IntroView step
  // through the options view until it reaches kProgress.
  void AdvanceToProgressStep(views::Widget* widget) {
    // Accept step 1 (Intro).
    AcceptWidgetAndMoveForward(widget);

    // If on platform with options view, accept OptionsView too.
    if (ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            WebAppInstallFlowDialogDelegate::kOptionsViewId)) {
      AcceptWidgetAndMoveForward(widget);
    }
  }

  // Helper that moves the installation forward from the progress view step
  // (which starts in kProgress) until it reaches the final success view
  // (kSuccessful).
  void AdvanceToSuccessfulStep(views::Widget* widget) {
    // Wait for the progress view to complete.
    views::View* progress_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            WebAppInstallProgressView::kProgressBarId,
            views::ElementTrackerViews::GetContextForWidget(widget));
    EXPECT_NE(progress_view, nullptr);

    views::ProgressBar* install_progress_bar =
        views::AsViewClass<views::ProgressBar>(progress_view);
    EXPECT_NE(install_progress_bar, nullptr);

    ASSERT_TRUE(base::test::RunUntil([install_progress_bar]() -> bool {
      return (install_progress_bar->GetValue() >= 0.9);
    }));

    // Wait for all installs to be completed, if in progress.
    WebAppProvider::GetForWebApps(browser()->GetProfile())
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
  }

  void AcceptWidgetAndMoveForward(views::Widget* widget) {
    widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kWebAppInstallDialog};
};

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, SimpleInstallFlow) {
  base::UserActionTester action_tester;
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  // Wait for the omnibox icon to become visible.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
        .GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppSimpleDialogAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, FocusRestoredOnCancel) {
  if (features::IsWebUILocationBarEnabled()) {
    // TODO(crbug.com/545160323): Support focus restoration in WebUI location
    // bar.
    GTEST_SKIP() << "Focus restoration not tested on WebUI location bar";
  }
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));
  page_actions::PageActionTestAccessor icon(browser(), kActionInstallPwa);
  ASSERT_TRUE(base::test::RunUntil([&]() { return icon.GetVisible(); }));

  icon.RequestFocus();
  EXPECT_TRUE(icon.HasFocus());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  widget->CloseWithReason(views::Widget::ClosedReason::kCancelButtonClicked);
  ASSERT_TRUE(base::test::RunUntil([&]() { return icon.HasFocus(); }));
  EXPECT_TRUE(icon.HasFocus());
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DetailedInstallFlow) {
  base::UserActionTester action_tester;
  // Detailed install flow is triggered when screenshots are available.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_screenshots.json");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
        .GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDetailedDialogAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DiyInstallFlow) {
  base::UserActionTester action_tester;
  // Navigate to a page that is not installable.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/no_manifest_test_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  // Show the dialog.
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppDiyDialogAccepted"));
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest,
                       InstallFlowShowsIntentPickerOnClose) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
        .GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->AcceptDialog();

  if (ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          WebAppInstallFlowDialogDelegate::kOptionsViewId)) {
    dialog_delegate->AcceptDialog();
  }

  AdvanceToSuccessfulStep(widget);

  dialog_delegate->CancelDialog();
  ASSERT_TRUE(web_app::WaitForIntentPickerToShow(browser()));
  EXPECT_TRUE(web_app::GetIntentPickerButton(browser()).GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest,
                       DropOffStepLoggedOnCancelAtIntro) {
  base::HistogramTester histogram_tester;
  base::UserActionTester action_tester;
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
        .GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Cancel the dialog on the first step (Intro).
  widget->widget_delegate()->AsDialogDelegate()->CancelDialog();

  destroyed_waiter.Wait();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.InstallFlow.DropOffStep"),
      base::BucketsAre(base::Bucket(InstallDialogStep::kInstallDialog, 1)));
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
  histogram_tester.ExpectTotalCount("WebApp.InstallFlow.SuccessStepAction", 0);
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppSimpleDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest,
                       DropOffStepLoggedOnCancelAtSuccess) {
  base::HistogramTester histogram_tester;
  base::UserActionTester action_tester;
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
        .GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToProgressStep(widget);
  AdvanceToSuccessfulStep(widget);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Cancel dialog on success step.
  widget->widget_delegate()->AsDialogDelegate()->CancelDialog();

  destroyed_waiter.Wait();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.InstallFlow.DropOffStep"),
      base::BucketsAre(base::Bucket(InstallDialogStep::kSuccessful, 1)));
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
#if BUILDFLAG(IS_MAC)
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallFlow.SuccessStepAction",
      InstallFlowSuccessStepAction::kShowInFinderClicked, 1);
#else
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallFlow.SuccessStepAction",
      InstallFlowSuccessStepAction::kCloseClicked, 1);
#endif
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppSimpleDialogClosed"));
}

// Regression test for crbug.com/570729142. When a site calls prompt() on its
// BeforeInstallPromptEvent and the user then clicks "Learn more" in the install
// dialog, the install must be treated as dismissed: the userChoice promise
// resolves with "dismissed" and a new beforeinstallprompt event is dispatched
// so the site can prompt again later.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest,
                       LearnMoreResolvesUserChoiceAsDismissed) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);
  ASSERT_NE(manager, nullptr);

  // Load a page that stashes the beforeinstallprompt event, and wait for the
  // renderer to reply to that event.
  {
    base::RunLoop run_loop;
    manager->SetBannerPromptReplyCallback(run_loop.QuitClosure());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_https_test_server().GetURL(
                       "/banners/manifest_test_page.html?action=stash_event")));
    run_loop.Run();
  }
  ASSERT_EQ(webapps::AppBannerManager::State::PENDING_PROMPT,
            manager->state_for_testing());
  ASSERT_TRUE(manager->IsPromptAvailableForTesting());

  // Have the page call prompt() on the stashed event, which shows the dialog.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  ASSERT_TRUE(content::ExecJs(web_contents, "callStashedPrompt();"));
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  // prompt() consumes the event; the site may not call prompt() again until it
  // receives a new beforeinstallprompt event.
  EXPECT_FALSE(manager->IsPromptAvailableForTesting());

  views::View* learn_more_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          WebAppInstallFlowDialogDelegate::kLearnMoreButtonId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(learn_more_view, nullptr);
  views::Button* learn_more_button =
      views::AsViewClass<views::Button>(learn_more_view);
  ASSERT_NE(learn_more_button, nullptr);

  base::RunLoop prompt_reply_run_loop;
  manager->SetBannerPromptReplyCallback(prompt_reply_run_loop.QuitClosure());
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  // Use a key event, which is not subject to input event activation
  // protection on a freshly shown dialog.
  views::test::ButtonTestApi(learn_more_button)
      .NotifyClick(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                                ui::EF_NONE));

  // The help page opens in a new foreground tab and the dialog closes.
  content::WebContents* help_contents = tab_added_waiter.Wait();
  ASSERT_NE(help_contents, nullptr);
  EXPECT_EQ(GURL(chrome::kInstallDialogFlowLearnMoreURL),
            help_contents->GetVisibleURL());
  destroyed_waiter.Wait();

  // The site's userChoice promise resolves as dismissed...
  const std::u16string expected_title = u"Got userChoice: dismissed";
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // ...and a new beforeinstallprompt event is dispatched so that the site can
  // prompt again.
  prompt_reply_run_loop.Run();
  EXPECT_EQ(webapps::AppBannerManager::State::PENDING_PROMPT,
            manager->state_for_testing());
  EXPECT_TRUE(manager->IsPromptAvailableForTesting());
}

#if BUILDFLAG(IS_MAC)
// Provisional duration to keep the grace period active during test setup.
constexpr base::TimeDelta kAppShimGracePeriodForTesting = base::Seconds(200);

// A deliberately reduced dialog size used to verify recovery from small bounds.
constexpr gfx::Size kSmallDialogSize(448, 200);
class WebAppInstallFlowStartupBrowserTest
    : public WebAppInstallFlowBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    WebAppInstallFlowBrowserTest::SetUpOnMainThread();
    // Keep the app-shim grace period long enough that tests can set up dialog
    // state before it expires. The override lives for the whole test.
    grace_period_override_ =
        WebAppInstallFlowDialogDelegate::SetAppShimGracePeriodForTesting(
            kAppShimGracePeriodForTesting);
  }

  // Installs the test app and waits for the native app window to appear.
  // Returns the WebContents of the PWA window, or nullptr.
  content::WebContents* InstallAndLaunchPwa() {
    BrowserWindowInterface* app_browser = InstallWebAppFromPageGetBrowser(
        browser(), embedded_https_test_server().GetURL(
                       "/banners/manifest_test_page.html"));
    if (!app_browser) {
      return nullptr;
    }

    content::WebContents* app_contents =
        app_browser->GetTabStripModel()->GetActiveWebContents();
    if (!content::WaitForLoadStop(app_contents)) {
      return nullptr;
    }

    views::Widget* app_widget =
        BrowserView::GetBrowserViewForBrowser(app_browser)->GetWidget();
    views::test::WidgetVisibleWaiter(app_widget).Wait();
    return app_contents;
  }

  // Fires navigator.install() from `app_contents` and waits for the install
  // flow dialog to appear. Returns a WeakPtr to the dialog widget, or null if
  // the dialog never showed.
  base::WeakPtr<views::Widget> FireInstallAndWaitForFlowDialog(
      content::WebContents* app_contents) {
    // Installing from a manifest URL prompts for permission.
    permissions::PermissionRequestManager::FromWebContents(app_contents)
        ->set_auto_response_for_test(permissions::PermissionRequestManager::
                                         AutoResponseType::ACCEPT_ALL);

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebAppInstallFlowDialog");
    // Fire and forget so we can inspect the dialog.
    EXPECT_TRUE(content::ExecJs(
        app_contents,
        content::JsReplace(
            "navigator.install({manifest: $1, manifestId: $2})"
            ".catch(error => { window.installError = error.name; });",
            embedded_https_test_server()
                .GetURL("/banners/manifest_with_id.json")
                .spec(),
            embedded_https_test_server().GetURL("/some_id").spec()),
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    views::Widget* widget = waiter.WaitIfNeededAndGet();
    if (!widget) {
      return nullptr;
    }
    views::test::WidgetVisibleWaiter(widget).Wait();
    return widget->GetWeakPtr();
  }

  // Waits until the install flow dialog's origin label is fully laid out and
  // visible. Returns false if the dialog is destroyed while waiting.
  bool WaitForFlowDialogLaidOut() {
    const bool completed = base::test::RunUntil([this]() {
      if (!widget_) {
        return true;
      }
      // Force layout when we poll, so we can accurately test clipping.
      widget_->LayoutRootViewIfNecessary();
      auto* origin =
          views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
              kSimpleInstallDialogAppInfoLabel,
              views::ElementTrackerViews::GetContextForWidget(widget_.get()));
      return origin && origin->IsDrawn() && !origin->size().IsEmpty() &&
             origin->GetVisibleBounds() == origin->GetLocalBounds();
    });
    return completed && widget_;
  }

  base::OneShotTimer& GetAppShimGraceTimer() {
    return WebAppInstallFlowDialogDelegate::GetAppShimGraceTimerForTesting(
        widget_.get());
  }

  views::View* GetOriginView() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        kSimpleInstallDialogAppInfoLabel,
        views::ElementTrackerViews::GetContextForWidget(widget_.get()));
  }

  bool IsDialogViewVisible(ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, views::ElementTrackerViews::GetContextForWidget(widget_.get()),
        /*require_visible=*/true);
  }

  base::WeakPtr<views::Widget> widget_;

 private:
  base::test::ScopedFeatureList web_install_feature_list_{
      blink::features::kWebAppInstallation};
  std::optional<base::AutoReset<base::TimeDelta>> grace_period_override_;
};

// Bounds recovery before the grace period expires prevents dialog dismissal.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       RecoversFromSmallBoundsBeforeGraceExpires) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  EXPECT_EQ(kAppShimGracePeriodForTesting,
            GetAppShimGraceTimer().GetCurrentDelay());

  const base::TimeTicks deadline = GetAppShimGraceTimer().desired_run_time();
  const gfx::Rect safe_bounds = widget_->GetWindowBoundsInScreen();

  // Simulate a late, stale bounds update from the app shim.
  widget_->SetSize(kSmallDialogSize);
  ASSERT_TRUE(widget_);
  ASSERT_EQ(kSmallDialogSize, widget_->GetSize());

  // The grace period allows the undersized dialog to stay open, but it must not
  // advance past the intro view or extend the original deadline.
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));
  EXPECT_EQ(deadline, GetAppShimGraceTimer().desired_run_time());

  // Restore safe bounds before the grace period expires.
  widget_->SetBounds(safe_bounds);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !widget_ || widget_->GetWindowBoundsInScreen() == safe_bounds;
  }));
  ASSERT_TRUE(widget_);
  EXPECT_EQ(deadline, GetAppShimGraceTimer().desired_run_time());

  // End the grace period. The dialog should remain open.
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  GetAppShimGraceTimer().FireNow();
  ASSERT_TRUE(widget_);

  // With safe bounds restored, advancing is allowed.
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));

  // Clean up the dialog.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destroyed.Wait();
}

// A pending resize is applied before checking layout when grace expires.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       FlushesPendingResizeWhenGraceExpires) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  ASSERT_TRUE(widget_->is_autosized());

  // Increase the preferred height enough so the dialog is temporarily
  // undersized (the resize task is posted asynchronously).
  const gfx::Size initial_size = widget_->GetSize();
  gfx::Size preferred_size = widget_->GetContentsView()->GetPreferredSize();
  preferred_size.set_height(initial_size.height() + 100);
  widget_->GetContentsView()->SetPreferredSize(preferred_size);
  ASSERT_EQ(initial_size, widget_->GetSize());
  ASSERT_TRUE(IsWidgetCurrentSizeSmallerThanPreferredSize(
      widget_.get(), GetMaxAllowedShrinkage(InstallDialogType::kSimple)));

  // Expire the grace period before the posted resize task runs. The dialog
  // should resize synchronously and remain open.
  GetAppShimGraceTimer().FireNow();
  ASSERT_TRUE(widget_);
  ASSERT_FALSE(widget_->IsClosed());
  EXPECT_FALSE(GetAppShimGraceTimer().IsRunning());
  EXPECT_GT(widget_->GetSize().height(), initial_size.height());
  EXPECT_FALSE(IsWidgetCurrentSizeSmallerThanPreferredSize(
      widget_.get(), GetMaxAllowedShrinkage(InstallDialogType::kSimple)));

  // Wait for queued tasks and verify the intro is still visible.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_FALSE(widget_->IsClosed());
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));

  // Clean up the dialog.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destroyed.Wait();
}

// The grace period must not allow installation from an undersized options view.
// Once safe bounds are restored, installation can proceed.
IN_PROC_BROWSER_TEST_F(
    WebAppInstallFlowStartupBrowserTest,
    BlocksInstallWhileInstallerOptionsUndersizedDuringGrace) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // Advance to installer options and wait for the dialog to reach safe bounds.
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !widget_ || !IsWidgetCurrentSizeSmallerThanPreferredSize(
                           widget_.get(),
                           GetMaxAllowedShrinkage(InstallDialogType::kSimple));
  }));
  ASSERT_TRUE(widget_);

  // Save the safe bounds and the grace period's deadline, and observe
  // installation attempts.
  const gfx::Rect safe_bounds = widget_->GetWindowBoundsInScreen();
  const base::TimeTicks deadline = GetAppShimGraceTimer().desired_run_time();
  base::UserActionTester action_tester;
  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  // Shrink the options dialog while size-based dismissal is still deferred.
  widget_->SetSize(kSmallDialogSize);
  ASSERT_TRUE(widget_);
  ASSERT_EQ(kSmallDialogSize, widget_->GetSize());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  ASSERT_TRUE(IsWidgetCurrentSizeSmallerThanPreferredSize(
      widget_.get(), GetMaxAllowedShrinkage(InstallDialogType::kSimple)));

  // Accepting undersized options must not start installation, advance to
  // progress, or extend the grace period's original deadline.
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));
  EXPECT_FALSE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kProgressViewId));
  EXPECT_EQ(0, action_tester.GetActionCount("WebAppSimpleDialogAccepted"));
  EXPECT_EQ(deadline, GetAppShimGraceTimer().desired_run_time());

  // Restore safe bounds before the grace period expires, without changing its
  // deadline.
  widget_->SetBounds(safe_bounds);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !widget_ || (widget_->GetWindowBoundsInScreen() == safe_bounds &&
                        !IsWidgetCurrentSizeSmallerThanPreferredSize(
                            widget_.get(), GetMaxAllowedShrinkage(
                                               InstallDialogType::kSimple)));
  }));
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  EXPECT_EQ(deadline, GetAppShimGraceTimer().desired_run_time());

  // With safe bounds restored, installation is allowed during the grace period.
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));
  EXPECT_EQ(1, action_tester.GetActionCount("WebAppSimpleDialogAccepted"));
  EXPECT_FALSE(install_observer.Wait().empty());
  ASSERT_TRUE(widget_);

  // Wait for the installed app's success view.
  AdvanceToSuccessfulStep(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kSuccessfulViewId));
  EXPECT_EQ(2u, provider().registrar_unsafe().GetAppIds().size());

  // Clean up the dialog.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destroyed.Wait();
}

// Hiding the origin during the grace period blocks advancement without
// disabling the button. Restoring the origin allows advancement again.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       BlocksAdvanceWhileOriginIsClipped) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // Hide the origin to exercise the visibility guard independently of size.
  views::View* origin = GetOriginView();
  ASSERT_NE(origin, nullptr);
  origin->SetVisible(false);

  // The enabled button must not advance past the intro view while the origin
  // is invisible, even during the grace period.
  auto* dialog = widget_->widget_delegate()->AsDialogDelegate();
  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));
  EXPECT_FALSE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));

  // Restore the origin. With safe layout restored, advancing is allowed.
  origin->SetVisible(true);
  widget_->LayoutRootViewIfNecessary();
  EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_FALSE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));

  // Clean up the dialog.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destroyed.Wait();
}

// The grace period must not allow advancement when the app title is invisible,
// even if the origin remains visible.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       BlocksAdvanceWhileTitleIsClipped) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // Hide the title to exercise its visibility guard independently of size.
  views::View* title =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kSimpleInstallDialogAppTitle,
          views::ElementTrackerViews::GetContextForWidget(widget_.get()));
  ASSERT_NE(title, nullptr);
  title->SetVisible(false);

  // The enabled button must not advance past the intro view while the title
  // is invisible, even during the grace period.
  EXPECT_TRUE(
      widget_->widget_delegate()->AsDialogDelegate()->IsDialogButtonEnabled(
          ui::mojom::DialogButton::kOk));
  AcceptWidgetAndMoveForward(widget_.get());
  ASSERT_TRUE(widget_);
  EXPECT_TRUE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kIntroViewId));
  EXPECT_FALSE(
      IsDialogViewVisible(WebAppInstallFlowDialogDelegate::kOptionsViewId));

  // Clean up the dialog.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destroyed.Wait();
}

// An invisible origin when the grace period expires causes dialog dismissal.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       ClosesIfStillClippedWhenGraceExpires) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // Keep the origin invisible until the grace period expires.
  views::View* origin = GetOriginView();
  ASSERT_NE(origin, nullptr);
  origin->SetVisible(false);

  // End the grace period. The unsafe dialog should be dismissed.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  GetAppShimGraceTimer().FireNow();
  destroyed.Wait();
}

// Placeholder 1x1 bounds when the grace period expires cause dialog dismissal.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       ClosesIfStillOneByOneWhenGraceExpires) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // Simulate the app shim's async kWindowSizeDeterminedLater bounds update.
  widget_->SetBounds(gfx::Rect(0, 0, 1, 1));
  ASSERT_TRUE(widget_);
  ASSERT_EQ(gfx::Size(1, 1), widget_->GetSize());

  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  ASSERT_TRUE(widget_);

  // Expire the grace period and verify that the dialog is autoclosed.
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());
  GetAppShimGraceTimer().FireNow();
  destroyed.Wait();
}

// After the grace period expires, undersized bounds queue a close task.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       ClosesIfUndersizedAfterGrace) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // End the grace period while the dialog is safely sized.
  GetAppShimGraceTimer().FireNow();
  ASSERT_TRUE(widget_);

  // After the grace period, undersized bounds must post the normal close task.
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->SetSize(kSmallDialogSize);
  ASSERT_TRUE(widget_);
  ASSERT_EQ(kSmallDialogSize, widget_->GetSize());

  // Wait for the posted close task to destroy the dialog.
  destroyed.Wait();
}

// Recovery after the grace period cannot rescue a dialog whose unsafe bounds
// have already caused a close task to be posted.
IN_PROC_BROWSER_TEST_F(WebAppInstallFlowStartupBrowserTest,
                       ClosesAfterGraceEvenIfBoundsRecover) {
  content::WebContents* app_contents = InstallAndLaunchPwa();
  ASSERT_TRUE(app_contents);
  ASSERT_TRUE(
      remote_cocoa::IsWindowRemote(app_contents->GetTopLevelNativeWindow()));

  // Execute `navigator.install` from the PWA.
  widget_ = FireInstallAndWaitForFlowDialog(app_contents);
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(remote_cocoa::IsWindowRemote(widget_->GetNativeWindow()));

  // Wait for the intro layout to settle and verify the grace period is active.
  ASSERT_TRUE(WaitForFlowDialogLaidOut());
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(GetAppShimGraceTimer().IsRunning());

  // End the grace period while the dialog is safely sized.
  GetAppShimGraceTimer().FireNow();
  ASSERT_TRUE(widget_);

  // Save the safe bounds, then shrink the dialog to trigger a close task.
  const gfx::Rect safe_bounds = widget_->GetWindowBoundsInScreen();
  views::test::WidgetDestroyedWaiter destroyed(widget_.get());
  widget_->SetSize(kSmallDialogSize);
  ASSERT_TRUE(widget_);
  ASSERT_EQ(kSmallDialogSize, widget_->GetSize());

  // Restoring safe bounds must not cancel an already-posted close task.
  widget_->SetBounds(safe_bounds);
  ASSERT_TRUE(widget_);
  ASSERT_EQ(safe_bounds, widget_->GetWindowBoundsInScreen());

  // The dialog should still be destroyed by the posted close task.
  destroyed.Wait();
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

enum class CheckboxOptions { kNeither, kShortcutOnly, kTaskbarOnly, kBoth };

std::string CheckboxOptionsToString(CheckboxOptions options) {
  switch (options) {
    case CheckboxOptions::kNeither:
      return "Neither";
    case CheckboxOptions::kShortcutOnly:
      return "CreateShortcutOnly";
    case CheckboxOptions::kTaskbarOnly:
      return "PinTaskbarOnly";
    case CheckboxOptions::kBoth:
      return "Both";
  }
  NOTREACHED();
}

class WebAppInstallFlowOptionsViewTest
    : public WebAppInstallFlowBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<InstallDialogType, CheckboxOptions>> {
 public:
  WebAppInstallFlowOptionsViewTest() {
    win_pin_feature_list_.InitAndEnableFeature(
        features::kWebAppInstallDialogWinPin);
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->GetProfile());
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  GURL GetCurrentAppUrlForFlow() {
    InstallDialogType flow_type = std::get<InstallDialogType>(GetParam());
    switch (flow_type) {
      case InstallDialogType::kSimple:
        return embedded_https_test_server().GetURL(
            "/banners/manifest_test_page.html");
      case InstallDialogType::kDetailed:
        return embedded_https_test_server().GetURL(
            "/banners/"
            "manifest_test_page.html?manifest=manifest_with_screenshots.json");
      case InstallDialogType::kDiy:
        return embedded_https_test_server().GetURL(
            "/banners/no_manifest_test_page.html");
    }
    NOTREACHED();
  }

  // Title is obtained from the manifest.json used in the corresponding url
  // returned by `GetCurrentAppUrlForFlow()`.
  std::string GetCurrentAppTitleForFlow() {
    InstallDialogType flow_type = std::get<InstallDialogType>(GetParam());
    switch (flow_type) {
      case InstallDialogType::kSimple:
        return "Manifest test app";
      case InstallDialogType::kDetailed:
        return "PWA Bottom Sheet";
      case InstallDialogType::kDiy:
        return "Web app banner test page";
    }
    NOTREACHED();
  }

 private:
  base::test::ScopedFeatureList win_pin_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppInstallFlowOptionsViewTest, OptionsParameters) {
  base::ScopedAllowBlockingForTesting allow_blocking_for_files;
  const auto& [flow_type, options] = GetParam();

  bool create_shortcut = false;
  bool pin_to_taskbar = false;

  switch (options) {
    case CheckboxOptions::kNeither:
      create_shortcut = false;
      pin_to_taskbar = false;
      break;
    case CheckboxOptions::kShortcutOnly:
      create_shortcut = true;
      pin_to_taskbar = false;
      break;
    case CheckboxOptions::kTaskbarOnly:
      create_shortcut = false;
      pin_to_taskbar = true;
      break;
    case CheckboxOptions::kBoth:
      create_shortcut = true;
      pin_to_taskbar = true;
      break;
  }

  // Navigation and wait logic depends on flow type
  if (flow_type == InstallDialogType::kDiy) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetCurrentAppUrlForFlow()));
  } else {
    ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(),
                                                    GetCurrentAppUrlForFlow()));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return page_actions::PageActionTestAccessor(browser(), kActionInstallPwa)
          .GetVisible();
    }));
  }

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  AcceptWidgetAndMoveForward(widget);

  // Use Element identifiers to find checkboxes and set their state.
  views::View* shortcut_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          web_app::WebAppInstallOptionsView::kCreateShortcutCheckboxId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(shortcut_view, nullptr);
  auto* shortcut_checkbox = views::AsViewClass<views::Checkbox>(shortcut_view);
  ASSERT_NE(shortcut_checkbox, nullptr);
  shortcut_checkbox->SetChecked(create_shortcut);

  views::View* taskbar_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          web_app::WebAppInstallOptionsView::kPinToTaskbarCheckboxId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(taskbar_view, nullptr);
  auto* taskbar_checkbox = views::AsViewClass<views::Checkbox>(taskbar_view);
  ASSERT_NE(taskbar_checkbox, nullptr);
  taskbar_checkbox->SetChecked(pin_to_taskbar);
  AcceptWidgetAndMoveForward(widget);
  ForwardThroughProgressViewAndAcceptDialog(widget);

  const webapps::AppId app_id = install_observer.Wait();

  // Verify OS hooks based on test parameters.
  // We cannot use "IsShortcutCreated()" here, since that always returns true,
  // as a shortcut is always created on the "Start Menu".
  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().desktop(), app_id,
          GetCurrentAppTitleForFlow());
  EXPECT_EQ(base::PathExists(desktop_shortcut_path), create_shortcut);
  EXPECT_EQ(os_integration_override().IsAppPinnedToTaskbar(app_id),
            pin_to_taskbar);

  test::UninstallWebApp(profile(), app_id);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppInstallFlowOptionsViewTest,
    testing::Combine(testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy),
                     testing::Values(CheckboxOptions::kNeither,
                                     CheckboxOptions::kShortcutOnly,
                                     CheckboxOptions::kTaskbarOnly,
                                     CheckboxOptions::kBoth)),
    [](const ::testing::TestParamInfo<
        std::tuple<InstallDialogType, CheckboxOptions>>& test_info) {
      return base::StrCat(
          {base::ToString(std::get<InstallDialogType>(test_info.param)), "_",
           CheckboxOptionsToString(
               std::get<CheckboxOptions>(test_info.param))});
    });

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace web_app
