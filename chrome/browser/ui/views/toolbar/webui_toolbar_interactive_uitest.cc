// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/metrics.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

// An observer for reload button tests that tracks completed and committed
// navigations.
class ReloadButtonTestNavigationObserver : public content::WebContentsObserver {
 public:
  explicit ReloadButtonTestNavigationObserver(
      content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ~ReloadButtonTestNavigationObserver() override = default;

  // WebContentsObserver implementation:

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ++num_started_navigations_;
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ++num_finished_navigations_;
    if (navigation_handle->HasCommitted()) {
      ++num_committed_navigations_;
    }
  }

  size_t num_started_navigations() const { return num_started_navigations_; }
  size_t num_finished_navigations() const { return num_finished_navigations_; }
  size_t num_committed_navigations() const {
    return num_committed_navigations_;
  }

 private:
  size_t num_started_navigations_ = 0;
  size_t num_finished_navigations_ = 0;
  size_t num_committed_navigations_ = 0;
};

// An HttpResponse subclass that returns a fixed plain text response, and can be
// managed by the test fixture. OnDemandHttpResponses should be created on the
// EmbeddedTestServer thread and then can be resumed by calling their
// TriggerResponse() method on the UI thread.
class OnDemandHttpResponse : public net::test_server::BasicHttpResponse {
 public:
  OnDemandHttpResponse() {
    set_content("Foo");
    set_content_type("text/plain");
  }

  ~OnDemandHttpResponse() override = default;

  // net::test_server::BasicHttpResponse implementation.
  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    response_delegate_ = delegate;
  }

  // Called on the UI thread. Posts a task over to `test_server_task_runner`,
  // which must be the EmbeddedTestServer's thread, to respond ot the request.
  // Takes a WeakPtr to the OnDemandHttpResponse() that should return the
  // response. Safely does nothing if the OnDemandHttpResponse has been
  // destroyed.
  static void TriggerResponse(
      scoped_refptr<base::SequencedTaskRunner> test_server_task_runner,
      base::WeakPtr<OnDemandHttpResponse> weak_ptr) {
    test_server_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&OnDemandHttpResponse::SendHeadersContentAndFinish,
                       std::move(weak_ptr)));
  }

  base::WeakPtr<OnDemandHttpResponse> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void SendHeadersContentAndFinish() {
    CHECK(response_delegate_);
    response_delegate_->SendHeadersContentAndFinish(code(), reason(),
                                                    BuildHeaders(), content());
  }

  base::WeakPtr<net::test_server::HttpResponseDelegate> response_delegate_;
  base::WeakPtrFactory<OnDemandHttpResponse> weak_ptr_factory_{this};
};

}  // namespace

// Tests for the old a new toolbar buttons. These tests unfortunately cannot
// exactly test behavior, since some behaviors depend on time passing, and
// browser tests can't mock out time, and because the WebUI logic is handled in
// a renderer, and so updated to/from the WebUI toolbar are always asynchronous.
class WebUIToolbarViewsInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebUIToolbarViewsInteractiveUiTest() {
    if (IsWebUIReloadButtonEnabled()) {
      // While the WebUI back and forward buttons are not currently tested by
      // these tests, they are needed to provide a spot on the toolbar for the
      // mouse to hover over that's not the reload button. See
      // MoveMouseOffOfReloadButton() for details.
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton,
#if BUILDFLAG(IS_MAC)
           // While it's not wrong to enable the WebUI back/forward button on
           // other platforms, it's currently only needed in these tests on Mac,
           // to work around a bug on that platform. See
           // MoveMouseOffOfReloadButton() for details.
           //
           // TODO(crbug.com/503006742): Remove this once the Mac bug if fixed,
           // or remove the above #if if we start testing the back/forward
           // button with these tests.
           features::kWebUIBackForwardButton,
#endif  // BUILDFLAG(IS_MAC)
           features::kSkipIPCChannelPausingForNonGuests,
           features::kWebUIInProcessResourceLoadingV2,
           features::kInitialWebUISyncNavStartToCommit},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {features::kInitialWebUI, features::kWebUIReloadButton,
               features::kWebUIBackForwardButton,
               features::kSkipIPCChannelPausingForNonGuests,
               features::kWebUIInProcessResourceLoadingV2,
               features::kInitialWebUISyncNavStartToCommit});
    }
  }

  ~WebUIToolbarViewsInteractiveUiTest() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  bool IsWebUIReloadButtonEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &WebUIToolbarViewsInteractiveUiTest::HandleDelayedRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

  // Invoked by the EmbeddedTestServer on the test server thread whenever a
  // request is observed. If the request is for kDelayedPath, sets up an
  // on-demand response, and passes over the information needed to respond to
  // the request back to the main thread.
  std::unique_ptr<net::test_server::HttpResponse> HandleDelayedRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() != kDelayedPath) {
      return nullptr;
    }

    auto response = std::make_unique<OnDemandHttpResponse>();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebUIToolbarViewsInteractiveUiTest::OnDemandResponseCreated,
            base::Unretained(this),
            base::SequencedTaskRunner::GetCurrentDefault(),
            response->GetWeakPtr()));

    return response;
  }

  // Called on the main thread when the EmbeddedTestServer sees a request for
  // `kDelayedPath`. Adds `response` to a queue of observed requests for the
  // URL. SendDelayedResponse() can be used to respond to the requests in FIFO
  // order.
  void OnDemandResponseCreated(
      scoped_refptr<base::SequencedTaskRunner> test_server_task_runner,
      base::WeakPtr<OnDemandHttpResponse> response) {
    test_server_task_runner_ = std::move(test_server_task_runner);
    pending_responses_.push_back(std::move(response));
    if (waiting_on_response_loop_) {
      waiting_on_response_loop_->Quit();
    }
  }

  // Waits until a request for `kDelayedPath` is observed, if one hasn't been
  // observed already, and then sends a response.
  void SendDelayedResponse() {
    CHECK(!waiting_on_response_loop_);
    if (pending_responses_.empty()) {
      // Have to allow nestable tasks to use this within a RunTestSequence()
      // call.
      waiting_on_response_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      waiting_on_response_loop_->Run();
      waiting_on_response_loop_.reset();
    }
    CHECK(!pending_responses_.empty());
    OnDemandHttpResponse::TriggerResponse(test_server_task_runner_,
                                          pending_responses_.front());
    pending_responses_.erase(pending_responses_.begin(),
                             pending_responses_.begin() + 1);
  }

  // Step that calls SendDelayedResponse(). Better than using Do() inline,
  // because it sets a useful step description for debugging.
  StepBuilder DoSendDelayedResponse() {
    StepBuilder step = Do(
        base::BindOnce(&WebUIToolbarViewsInteractiveUiTest::SendDelayedResponse,
                       base::Unretained(this)));
    SetStepDescription(step, "DoSendDelayedResponse()");
    return step;
  }

  // Waits for a load to stop. If no load is running, assumes load has already
  // completed, and does nothing. That lack of waiting is the primary reason
  // this may be preferred over WaitForWebContentsNavigation(), though if it's
  // called too soon, it could theoretically return before the load has even
  // started, so use with care.
  StepBuilder DoWaitForLoadStop() {
    StepBuilder step = Do(base::BindOnce(
        [](Browser* browser) {
          content::WaitForLoadStop(
              browser->tab_strip_model()->GetActiveWebContents());
        },
        base::Unretained(browser())));
    SetStepDescription(step, "DoWaitForLoadStop()");
    return step;
  }

  // Returns a URL that will hang until SendDelayedResponse() is invoked.
  GURL DelayedUrl() const {
    return embedded_test_server()->GetURL(kDelayedPath);
  }

  views::WebView* GetToolbarWebView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetWebUIToolbarViewForTesting()
        ->GetWebViewForTesting();
  }

  // Waits until the reload button is "ready" after a navigation completes -
  // that means the reload icon is displaying, and not in the double-click
  // timeout period. Note that since the reload button is showing, we also
  // know the button isn't disabled, and the enable timer isn't running, since
  // those only happen while showing the stop icon.
  MultiStep WaitForReloadButtonReady() {
    if (IsWebUIReloadButtonEnabled()) {
      return WaitForJsResultAt(kWebUIToolbarId, kReloadButtonDeepQuery,
                               "el => (!el.showStopIcon &&"
                               "  !el.doubleClickReloadIconTimer_.isRunning())",
                               true);
    }
    return PollUntil(
        base::BindRepeating(
            [](const ReloadButton* reload_button) {
              return reload_button->GetVisibleMode() ==
                         ReloadButton::Mode::kReload &&
                     !reload_button->GetDoubleClickTimerIsRunning();
            },
            base::Unretained(&GetNonWebUIReloadButton())),
        "Reload button ready");
  }

  // Waits for the reload button to show an enabled stop icon.
  MultiStep WaitForReloadButtonStopIcon() {
    if (IsWebUIReloadButtonEnabled()) {
      return WaitForJsResultAt(kWebUIToolbarId, kReloadButtonDeepQuery,
                               R"(el => (el.showStopIcon && !el.isDisabled))",
                               true);
    }
    return PollUntil(base::BindRepeating(
                         [](const ReloadButton* reload_button) {
                           return reload_button->GetVisibleMode() ==
                                      ReloadButton::Mode::kStop &&
                                  reload_button->GetEnabled();
                         },
                         base::Unretained(&GetNonWebUIReloadButton())),
                     "Reload button showing enabled stop icon");
  }

  // Called at the start of reload button tests. Instruments the initial tab
  // and moves the mouse off of the reload button. See
  // MoveMouseOffOfReloadButton() for why it's a good idea to move the cursor
  // off of the toolbar at the start of reload button tests.
  MultiStep SetUpReloadButtonTest() {
    return Steps(InstrumentTab(kTabId), InstrumentReloadButton(),
                 MoveMouseOffOfReloadButton());
  }

  MultiStep InstrumentReloadButton() {
    if (IsWebUIReloadButtonEnabled()) {
      return Steps(
          InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
          WaitForReloadButtonReady());
    }
    return WaitForReloadButtonReady();
  }

  // Moves mouse over the reload button and, if the WebUI reload button is
  // enabled, waits for the ":hover" state to be applied to the button, since
  // the WebUI implementation depends on that state, unlike the Views
  // implementation, which queries the current location of the cursor instead.
  MultiStep MoveMouseOverReloadButton() {
    if (IsWebUIReloadButtonEnabled()) {
      return Steps(MoveMouseTo(kWebUIToolbarId, kReloadButtonDeepQuery),
                   WaitForReloadHover(/*hover=*/true));
    }
    return Steps(MoveMouseTo(kReloadButtonElementId));
  }

  // Move cursor off of the reload button, and if using the WebUI reload
  // button, wait for the ":hover" state to be removed. This is useful because
  // hovering over the reload button affects reload button state (e.g.,
  // hovering when load stops will temporarily disable the button, which
  // affects tests). No wait is necessary with the views toolbar button,
  // because it checks the current location of the cursor, rather than relying
  // on a state that may take a little time to update.
  //
  // InstrumentReloadButton() must be called before this step is run, so it
  // can find the reload button to wait until it realizes the mouse is not
  // hovering over it.
  //
  // In theory, it doesn't actually matter where the cursor as moved, as long
  // as it's not on the reload but still on top of the browser window (to make
  // sure simulated events are propagated). However, to remove the ":hover"
  // state on Mac, the cursor needs to still be over the toolbar on that
  // platform.
  //
  // TODO(crbug.com/503006742): Remove the use of back/forward button on Mac
  // once this is fixed.
  MultiStep MoveMouseOffOfReloadButton() {
#if BUILDFLAG(IS_MAC)
    if (IsWebUIReloadButtonEnabled()) {
      return Steps(MoveMouseTo(kWebUIToolbarId, kBackForwardButtonDeepQuery),
                   WaitForReloadHover(/*hover=*/false));
    }
#endif  // BUILDFLAG(IS_MAC)
    return Steps(MoveMouseTo(kTabId));
  }

  // Waits for the reload button's CSS property to have / not have the
  // ":hover" property, depending on `hover`. InstrumentReloadButton() must be
  // called before this step is run. Step only makes sense when
  // IsWebUIReloadButtonEnabled() is true. The Views reload button makes
  // system calls to get the location of the cursor, so always gets the most
  // up-to-date position.
  MultiStep WaitForReloadHover(bool hover) {
    CHECK(IsWebUIReloadButtonEnabled());
    return WaitForJsResultAt(kWebUIToolbarId, kReloadButtonDeepQuery,
                             R"(el => el.renderRoot.querySelector(
                                'cr-icon-button')?.matches(':hover'))",
                             hover);
  }

  // Waits for the specified amount of time.
  StepBuilder DoWaitForTime(base::TimeDelta delay) {
    StepBuilder step = Do(base::BindOnce(
        [](base::TimeDelta delay) {
          // Have to allow nestable tasks to use this within a
          // RunTestSequence() call.
          base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, run_loop.QuitClosure(), delay);
          run_loop.Run();
        },
        delay));
    SetStepDescription(step, "DoWaitForTime()");
    return step;
  }

  ReloadControl& GetReloadControl() {
    BrowserView* const browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    ToolbarButtonProvider* provider = browser_view->toolbar_button_provider();
    return *provider->GetReloadButton();
  }

  WebUIReloadControl& GetWebUIReloadButton() {
    CHECK(IsWebUIReloadButtonEnabled());
    return static_cast<WebUIReloadControl&>(GetReloadControl());
  }

  ReloadButton& GetNonWebUIReloadButton() {
    CHECK(!IsWebUIReloadButtonEnabled());
    return static_cast<ReloadButton&>(GetReloadControl());
  }

  void SetReloadButtonDoubleClickInterval(base::TimeDelta interval) {
    // Could have added a virtual method to avoid the casting, but this avoids
    // including the methods in release builds.
    if (IsWebUIReloadButtonEnabled()) {
      GetWebUIReloadButton().set_double_click_interval_for_testing(interval);
    } else {
      GetNonWebUIReloadButton().set_double_click_timer_delay_for_testing(
          interval);
    }
  }

  // Checks that the reload button's mode is currently `expected_mode`.
  StepBuilder ExpectReloadButtonMode(ReloadControl::Mode expected_mode) {
    if (IsWebUIReloadButtonEnabled()) {
      return CheckJsResultAt(kWebUIToolbarId, kReloadButtonDeepQuery,
                             R"(el => el.showStopIcon)",
                             expected_mode == ReloadControl::Mode::kStop);
    } else {
      return Do(base::BindOnce(
          [](ReloadButton* reload_button, ReloadButton::Mode expected_mode) {
            EXPECT_EQ(reload_button->GetVisibleMode(), expected_mode);
          },
          base::Unretained(&GetNonWebUIReloadButton()), expected_mode));
    }
  }

  // Navigates to DelayedUrl() and triggers a response, waiting for the
  // navigation to complete. Can't use ui_test_utils::NavigateToURL() because
  // that starts the navigation and blocks until it completes, not letting us
  // call SendDelayedResponse() in the middle of the navigation.
  StepBuilder DoNavigateToDelayedUrl() {
    StepBuilder step = Do(base::BindOnce(
        [](WebUIToolbarViewsInteractiveUiTest* test) {
          NavigateParams params(test->browser(), test->DelayedUrl(),
                                ui::PAGE_TRANSITION_LINK);
          Navigate(&params);
          test->SendDelayedResponse();
          EXPECT_TRUE(
              content::WaitForLoadStop(params.navigated_or_inserted_contents));
        },
        base::Unretained(this)));
    SetStepDescription(step, "DoNavigateToDelayedUrl()");
    return step;
  }

  void SetStepDescription(StepBuilder& step, std::string_view description) {
    int count = ++step_with_description_counts_[std::string(description)];
    step.SetDescription(base::StringPrintf("%s, call %i", description, count));
  }

 private:
  static constexpr std::string_view kDelayedPath = "/delayed";

  const WebContentsInteractionTestUtil::DeepQuery kReloadButtonDeepQuery = {
      "toolbar-app", "reload-button"};
  const WebContentsInteractionTestUtil::DeepQuery kBackForwardButtonDeepQuery =
      {"toolbar-app", "back-forward-button"};

  // Number of steps with a particular description. Helps in debugging when
  // there are multiple identical steps, which is not uncommon in these tests.
  std::map<std::string, int> step_with_description_counts_;

  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::SequencedTaskRunner> test_server_task_runner_;

  std::unique_ptr<base::RunLoop> waiting_on_response_loop_;
  std::vector<base::WeakPtr<OnDemandHttpResponse>> pending_responses_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebUIToolbarViewsInteractiveUiTest,
                         testing::Bool());

// Test that the reload button exists, and clicking on it will cause the page
// to be reloaded.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest, ReloadButton) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(SetUpReloadButtonTest(), NavigateWebContents(kTabId, url),
                  WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
                  ClickMouse(), WaitForWebContentsNavigation(kTabId, url));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test that multiple reload clicks while in the double-click period, before a
// page has finished loading (or even committed) are ignored. Simulates a
// bunch of clicks at once, then pauses briefly, and then simulates more. Also
// checks that only the reload button is shown during this process, as it only
// changes after the reload interval has passed.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonMultipleClicksBeforeLoadStopIgnored) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Set the double click interval to be long enough to avoid any chance of it
  // passing during the test.
  SetReloadButtonDoubleClickInterval(base::Hours(1));

  // Simulate having shift pressed for some of the loads, which should not
  // make a difference to the logic under test.
  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
      ExpectReloadButtonMode(ReloadControl::Mode::kReload), ClickMouse(),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift),
      ClickMouse(), ExpectReloadButtonMode(ReloadControl::Mode::kReload),
      DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kReload),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift),
      ClickMouse(),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift),
      ExpectReloadButtonMode(ReloadControl::Mode::kReload),
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      ExpectReloadButtonMode(ReloadControl::Mode::kReload));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test that the load completing cancels the double-click cooldown timer, and
// allows the reload button to be pressed again.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonMultipleClicksAfterLoadStopIgnored) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Set the double click interval to be long enough to avoid any chance of it
  // passing during the test.
  SetReloadButtonDoubleClickInterval(base::Hours(1));

  // Simulate having shift pressed for some of the loads, which should not
  // make a difference to the logic under test.
  RunTestSequence(SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
                  WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
                  ClickMouse(), DoSendDelayedResponse(), DoWaitForLoadStop(),
                  // The stop button should never be shown.
                  ExpectReloadButtonMode(ReloadControl::Mode::kReload),
                  // Wait until the reload timer has stopped, and click it
                  // again, which should trigger a new load.
                  WaitForReloadButtonReady(), ClickMouse(),
                  DoSendDelayedResponse(), DoWaitForLoadStop());

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Make sure the reload button can eventually be clicked again.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickAgainAfterReloadInterval) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set a short double click interval.
  SetReloadButtonDoubleClickInterval(base::Milliseconds(100));

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(kTabId, url),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      WaitForWebContentsNavigation(kTabId, url),
      // Make sure the reload button is ready before trying to load again, to
      // avoid any races. This is not able to check that the exact interval is
      // respected, unfortunately. Also note that this waits until the icon is
      // no longer disabled, as may happen on commit if the cursor is hovering
      // over the button.
      WaitForReloadButtonReady(), ClickMouse(),
      WaitForWebContentsNavigation(kTabId, url));

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Make sure the reload button can eventually be clicked again. This test
// waits through the reload button being disabled. It uses the delayed URL, to
// avoid raciness around when the reload icon switches to the stop icon.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickAgainAfterReloadInterval2) {
  // Set a short double click interval.
  SetReloadButtonDoubleClickInterval(base::Milliseconds(100));

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
                  WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
                  ClickMouse(), WaitForReloadButtonStopIcon(),
                  DoSendDelayedResponse(), DoWaitForLoadStop(),
                  // Make sure the reload button is ready before trying to
                  // load again, to avoid any races. This is not able to check
                  // that the exact interval is respected, unfortunately.
                  WaitForReloadButtonReady(), ClickMouse(),
                  DoSendDelayedResponse(), DoWaitForLoadStop());

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Test that creating a new tab resets the interval required between pressing
// the reload button, which is pressed when the new tab is focused.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonNewTabResetsReloadInterval) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set the double click interval to be long enough to avoid any chance of it
  // passing during the test.
  SetReloadButtonDoubleClickInterval(base::Hours(1));

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(kTabId, url),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
      // Click reload button for initial tab, and wait for navigation to
      // start. Waiting for navigation start prevents racily creating a new
      // tab before navigating the old one starts.
      ClickMouse(), WaitForWebContentsNavigation(kTabId, url),
      MoveMouseOffOfReloadButton(),
      // Move mouse off of the reload button to avoid the reload button
      // potentially becoming disabled on load complete for the initial load
      // of the new tab.
      AddInstrumentedTab(kTab2Id, url),
      // Wait for the reload button to be updated to reflect the completed
      // navigation.
      WaitForReloadButtonReady(),
      // Press reload button for the new tab, and wait for it to complete.
      MoveMouseOverReloadButton(), ClickMouse(),
      WaitForWebContentsNavigation(kTab2Id, url));
}

// Test that switching to a new tab resets the interval required between
// pressing the reload button, which is pressed when the new tab is focused.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonSwitchingTabResetsReloadInterval) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  // Set the double click interval to be long enough to avoid any chance of it
  // passing during the test.
  SetReloadButtonDoubleClickInterval(base::Hours(1));

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(kTabId, url),
      WaitForReloadButtonReady(), AddInstrumentedTab(kTab2Id, url),
      // Wait for the reload button to be updated to reflect the completed
      // navigation.
      WaitForReloadButtonReady(),
      // Press reload button for the new tab, and wait for it to complete.
      MoveMouseOverReloadButton(), ClickMouse(),
      WaitForWebContentsNavigation(kTab2Id, url),
      // Switch back to the original tab, and press the reload button again.
      // The double-click delay should not apply, due to the tab switch.
      SelectTab(kTabStripElementId, 0), WaitForReloadButtonReady(),
      ClickMouse(), WaitForWebContentsNavigation(kTabId, url));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}
