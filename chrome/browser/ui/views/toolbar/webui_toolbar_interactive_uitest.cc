// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_and_views_toolbar_interactive_uitest_base.h"
#include "chrome/browser/ui/views/toolbar/webui_reload_control.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/screen.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/metrics.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2Id);

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

class WebUIToolbarPixelInteractiveUiTest : public InteractiveBrowserTest {
 public:
  WebUIToolbarPixelInteractiveUiTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUISplitTabsButton, features::kWebUIBackForwardButton,
         features::kWebUIHomeButton, features::kWebUIPinnedToolbarActions,
         ::tabs::kHorizontalTabStripComboButton, features::kWebUILocationBar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUp() override {
    EnablePixelOutput();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

  void BasicPixelTest(Browser* browser, const std::string& screenshot_name) {
    ui::TrackedElement* element = nullptr;
    WebUIToolbarWebView* webui_toolbar_view = nullptr;
    views::WebView* web_view = nullptr;
    ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                       &webui_toolbar_view, &web_view,
                                       browser));

    // Assert that WebContents is not loading, as it affects the state of the
    // reload button.
    ASSERT_FALSE(web_view->GetWebContents()->IsLoading());
    // The WebView should be using the light color mode for regular windows,
    // and dark color mode for incognito windows.
    ASSERT_EQ(web_view->GetWidget()->GetColorMode(),
              browser->profile()->IsIncognitoProfile()
                  ? ui::ColorProviderKey::ColorMode::kDark
                  : ui::ColorProviderKey::ColorMode::kLight);

    // Pixel test
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kVerifyPixels)) {
      views::ViewSkiaGoldPixelDiff pixel_diff(
          "WebUIToolbarPixelInteractiveUiTest");
      EXPECT_TRUE(pixel_diff.CompareViewScreenshot(screenshot_name,
                                                   webui_toolbar_view));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarPixelInteractiveUiTest, Basic) {
  BasicPixelTest(browser(), "Basic");
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarPixelInteractiveUiTest, IncognitoBasic) {
  BasicPixelTest(CreateIncognitoBrowser(), "IncognitoBasic");
}

// Tests for the old a new toolbar buttons. These tests unfortunately cannot
// exactly test behavior, since some behaviors depend on time passing, and
// browser tests can't mock out time, and because the WebUI logic is handled in
// a renderer, and so updated to/from the WebUI toolbar are always asynchronous.
class WebUIToolbarViewsInteractiveUiTest
    : public WebUIAndViewsToolbarInteractiveUiTestBase,
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
    WebUIAndViewsToolbarInteractiveUiTestBase::SetUpOnMainThread();

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

  // Called at the start of reload button tests. Instruments the initial tab and
  // moves the mouse off of the reload button. See MoveMouseOffOfReloadButton()
  // for why it's a good idea to move the cursor off of the toolbar at the start
  // of reload button tests.
  MultiStep SetUpReloadButtonTest() {
    return Steps(InstrumentToolbar(), MoveMouseOffOfReloadButton(),
                 WaitForReloadButtonReady());
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

  // Sets the double click interval for the reload button. May only be called
  // after InstrumentReloadButton() has been invoked.
  MultiStep SetReloadButtonDoubleClickInterval(
      base::TimeDelta double_click_interval) {
    if (IsWebUIReloadButtonEnabled()) {
      return Steps(
          Do(base::BindOnce(
              [](WebUIReloadControl* reload_control,
                 base::TimeDelta double_click_interval) {
                reload_control->SetDoubleClickIntervalForTesting(
                    double_click_interval);
              },
              base::Unretained(&GetWebUIReloadButton()),
              double_click_interval)),
          // Wait for the updated state to reach Javascript. Since mouse
          // messages are handled by JS directly, there's a chance of any
          // subsequent click event making it to the renderer before the new
          // interval, otherwise. Use milliseconds to avoid overflow, since
          // base::Values can only hold 32-bit ints.
          WaitForJsResultAt(
              WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
              R"(el => Number(el.state.doubleClickInterval.microseconds)/1000)",
              static_cast<int>(double_click_interval.InMilliseconds())));

    } else {
      return Steps(Do(base::BindOnce(
          [](ReloadButton* reload_button,
             base::TimeDelta double_click_interval) {
            reload_button->set_double_click_timer_delay_for_testing(
                double_click_interval);
          },
          base::Unretained(&GetNonWebUIReloadButton()),
          double_click_interval)));
    }
  }

  // Sets the mode switch interval for the reload button. May only be called
  // after InstrumentReloadButton() has been invoked.
  StepBuilder SetModeSwitchInterval(base::TimeDelta mode_switch_interval) {
    StepBuilder step;
    if (IsWebUIReloadButtonEnabled()) {
      // The WebUI reload button mode switch timer is handled entirely in
      // Javascript, so have to call into Javascript to set its duration.
      step = CheckJsResultAt(
          WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
          content::JsReplace(
              R"(el => el.modeSwitchIntervalMs_ = $1)",
              static_cast<int>(mode_switch_interval.InMilliseconds())),
          static_cast<int>(mode_switch_interval.InMilliseconds()));
    } else {
      step = Do(base::BindOnce(
          [](ReloadButton* reload_button,
             base::TimeDelta mode_switch_interval) {
            reload_button->set_mode_switch_timer_delay_for_testing(
                mode_switch_interval);
          },
          base::Unretained(&GetNonWebUIReloadButton()), mode_switch_interval));
    }
    SetStepDescription(step, "SetModeSwitchInterval()");
    return step;
  }

  // Checks that the reload button's mode is currently `expected_mode` and not
  // disabled.
  StepBuilder ExpectReloadButtonMode(ReloadControl::Mode expected_mode) {
    if (IsWebUIReloadButtonEnabled()) {
      return CheckJsResultAt(
          WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
          content::JsReplace(
              R"(el => (el.showStopIcon == $1 && !el.isDisabled))",
              expected_mode == ReloadControl::Mode::kStop),
          true);
    } else {
      return Do(base::BindOnce(
          [](ReloadButton* reload_button, ReloadButton::Mode expected_mode) {
            EXPECT_EQ(reload_button->GetVisibleMode(), expected_mode);
            EXPECT_TRUE(reload_button->GetEnabled());
          },
          base::Unretained(&GetNonWebUIReloadButton()), expected_mode));
    }
  }

  // Checks that the reload button is currently displaying the stop button and
  // is disabled.
  StepBuilder ExpectReloadButtonStopModeAndDisabled() {
    StepBuilder step;
    if (IsWebUIReloadButtonEnabled()) {
      step =
          CheckJsResultAt(WebUIToolbarId(), WebUIReloadButtonDeepQuery(),
                          R"(el => (el.showStopIcon && el.isDisabled))", true);
    } else {
      step = Do(base::BindOnce(
          [](ReloadButton* reload_button) {
            EXPECT_EQ(reload_button->GetVisibleMode(),
                      ReloadControl::Mode::kStop);
            EXPECT_FALSE(reload_button->GetEnabled());
          },
          base::Unretained(&GetNonWebUIReloadButton())));
    }
    SetStepDescription(step, "SetModeSwitchInterval()");
    return step;
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

  // Triggers a reload without a button press. For the purposes of these tests,
  // the important thing is that it's a load not triggered by clicking on the
  // reload button.
  StepBuilder DoStartReloadWithoutClick() {
    StepBuilder step = Do(base::BindOnce(
        [](Browser* browser) {
          browser->tab_strip_model()
              ->GetActiveWebContents()
              ->GetController()
              .Reload(content::ReloadType::NORMAL, /*check_for_repost=*/false);
        },
        base::Unretained(browser())));
    SetStepDescription(step, "DoStartReloadWithoutClick()");
    return step;
  }

  void SetStepDescription(StepBuilder& step, std::string_view description) {
    int count = ++step_with_description_counts_[std::string(description)];
    step.SetDescription(base::StringPrintf("%s, call %i", description, count));
  }

 private:
  static constexpr std::string_view kDelayedPath = "/delayed";

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

  RunTestSequence(SetUpReloadButtonTest(), NavigateWebContents(TabId(), url),
                  WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
                  ClickMouse(), WaitForWebContentsNavigation(TabId(), url));

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

  // Simulate having shift pressed for some of the loads, which should not make
  // a difference to the logic under test.
  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      // Set the double click interval to be long enough to avoid any chance of
      // it passing during the test.
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
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

  // Simulate having shift pressed for some of the loads, which should not make
  // a difference to the logic under test.
  RunTestSequence(SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
                  // Set the double click interval to be long enough to avoid
                  // any chance of it passing during the test.
                  SetReloadButtonDoubleClickInterval(base::Hours(1)),
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

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(TabId(), url),
      // Set a short double click interval.
      SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      WaitForWebContentsNavigation(TabId(), url),
      // Make sure the reload button is ready before trying to load again, to
      // avoid any races. This is not able to check that the exact interval is
      // respected, unfortunately. Also note that this waits until the icon is
      // no longer disabled, as may happen on commit if the cursor is hovering
      // over the button.
      WaitForReloadButtonReady(), ClickMouse(),
      WaitForWebContentsNavigation(TabId(), url));

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Make sure the reload button can eventually be clicked again. This test
// waits through the reload button being disabled. It uses the delayed URL, to
// avoid raciness around when the reload icon switches to the stop icon.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickAgainAfterReloadInterval2) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
                  // Set a short double click interval.
                  SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
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

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(TabId(), url),
      // Set the double click interval to be long enough to avoid any chance of
      // it passing during the test.
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
      // Click reload button for initial tab, and wait for navigation to
      // start. Waiting for navigation start prevents racily creating a new
      // tab before navigating the old one starts.
      ClickMouse(), WaitForWebContentsNavigation(TabId(), url),
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

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), NavigateWebContents(TabId(), url),
      // Set the double click interval to be long enough to avoid any chance of
      // it passing during the test.
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
      AddInstrumentedTab(kTab2Id, url),
      // Wait for the reload button to be updated to reflect the completed
      // navigation.
      WaitForReloadButtonReady(),
      // Press reload button for the new tab, and wait for it to complete.
      MoveMouseOverReloadButton(), ClickMouse(),
      WaitForWebContentsNavigation(kTab2Id, url),
      // Switch back to the original tab, and press the reload button again.
      // The double-click delay should not apply, due to the tab switch.
      SelectTab(kTabStripElementId, 0), WaitForReloadButtonReady(),
      ClickMouse(), WaitForWebContentsNavigation(TabId(), url));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test how the reload button changes when the mouse never hovers over the icon
// when a reload is triggered by some other mechanism.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonNotClickedMouseNeverOverButton) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
      SetModeSwitchInterval(base::Hours(1)), WaitForReloadButtonReady(),
      // Trigger a reload without the mouse.
      DoStartReloadWithoutClick(),
      // We should soon start showing the stop icon, and should
      // continue showing it until the page finishes loading.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      WaitForReloadButtonStopIcon(),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, we should start showing the
      // reload button again.
      WaitForReloadButtonReady());

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test how the reload button changes when the mouse hovers over the icon when a
// reload is triggered by some other mechanism. This test uses a long mode
// switch interval, so checks that the button is disabled, as expected.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonNotClickedMouseHoverOverButton1) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
      SetModeSwitchInterval(base::Hours(1)), WaitForReloadButtonReady(),
      MoveMouseOverReloadButton(),
      // Trigger a reload without the mouse.
      DoStartReloadWithoutClick(),
      // We should soon start showing the stop icon, and should show it until
      // the load completes.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kStop),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, we should continue showing the stop
      // button, but it should be disabled. Check that clicking the button does
      // nothing.
      WaitForReloadButtonDisabledStopIcon(), ClickMouse(),
      // Wait to make sure the ClickMouse call didn't trigger a load.
      DoWaitForTime(base::Milliseconds(100)),
      // Button should still be disabled.
      ExpectReloadButtonStopModeAndDisabled());

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test how the reload button changes when the mouse hovers over the icon when a
// reload is triggered by some other mechanism. This test uses a short mode
// switch interval, and checks that after the button is re-enabled, clicking on
// it works.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonNotClickedMouseHoverOverButton2) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      SetReloadButtonDoubleClickInterval(base::Hours(1)),
      SetModeSwitchInterval(base::Milliseconds(100)),
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
      // Trigger a reload without the mouse.
      DoStartReloadWithoutClick(),
      // We should soon start showing the stop icon, and should show it until
      // the load completes.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kStop),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, the button should be temporarily disabled.
      // Rather than try to observe it when it's disabled (which could be racy),
      // wait for it to be enabled.
      WaitForReloadButtonReady(),
      // Trigger a reload by pressing the button, and make sure it works.
      ClickMouse(), DoSendDelayedResponse(), DoWaitForLoadStop());

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Test how the reload button changes when the mouse is clicked to trigger a
// reload, but the cursor is moved off of the button before the load completes.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonIconMouseMovedOffOfButton) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      // This interval will be run into, though the test uses
      // WaitForReloadButtonStopIcon() to wait for it to pass, rather than
      // trying to observe this state, to avoid any races.
      SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
      // This interval should never be appled in this test, since the cursor is
      // moved off the button before the load completes.
      SetModeSwitchInterval(base::Hours(1)),
      // Trigger a reload with the mouse.
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      // We should show the stop icon once the double-click interval passes.
      // Can't really check the interval in this test, since time is not mocked
      // out.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      WaitForReloadButtonStopIcon(),
      // Move mouse off the reload button before the load completes. As a
      // result, the button should never be disabled.
      MoveMouseOffOfReloadButton(),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, we should start showing the reload button
      // again.
      WaitForReloadButtonReady());

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test that the stop icon is shown when the cursor hovers over the reload
// button after clicking on it. This test uses a long mode switch interval, and
// checks that the button is disabled on load complete.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickedMouseHoverOverButton1) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
      SetModeSwitchInterval(base::Hours(1)),
      // Click the mouse while it's over the reload button, which should trigger
      // a reload.
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      // We should soon start showing the stop icon, and should show it until
      // the load completes.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kStop),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, we should continue showing the stop
      // button, but it should be disabled. Check that clicking the button does
      // nothing.
      WaitForReloadButtonDisabledStopIcon(), ClickMouse(),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift),
      ClickMouse(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonStopModeAndDisabled(),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift),
      ClickMouse(),
      ClickMouse(ui_controls::LEFT, /*release=*/true, ui_controls::kShift));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

// Test that after showing the stop icon when the reload button is pressed, the
// reload button eventually starts showing the reload icon and is enabled. To
// avoid any races, this test does not check that the reload button is disabled,
// it just waits until it's enabled again before triggering another load.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickedMouseHoverOverButton2) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
      SetModeSwitchInterval(base::Milliseconds(100)),
      // Click the mouse while it's over the reload button, which should trigger
      // a reload.
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      // We should soon start showing the stop icon, and should show it until
      // the load completes.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kStop),
      // Complete the request.
      DoSendDelayedResponse(), DoWaitForLoadStop(),
      // Once the request completes, the button should be temporarily disabled.
      // Rather than try to observe it when it's disabled (which could be racy),
      // wait for it to be enabled.
      WaitForReloadButtonReady(),
      // Trigger a reload by pressing the button, and make sure it works.
      ClickMouse(), DoSendDelayedResponse(), DoWaitForLoadStop());

  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 3u);
}

// Test that when the stop icon is pressed, after pressing the reload icon, that
// the load is stopped, and a disabled stop icon is never shown.
IN_PROC_BROWSER_TEST_P(WebUIToolbarViewsInteractiveUiTest,
                       ReloadButtonClickedThenStopClicked) {
  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(
      SetUpReloadButtonTest(), DoNavigateToDelayedUrl(),
      // The reload interval should be hit in this test, but not the mode switch
      // interval.
      SetReloadButtonDoubleClickInterval(base::Milliseconds(100)),
      SetModeSwitchInterval(base::Hours(1)),
      // Click the mouse while it's over the reload button, which should trigger
      // a reload.
      WaitForReloadButtonReady(), MoveMouseOverReloadButton(), ClickMouse(),
      // We should soon start showing the stop icon, and should show it until
      // the load completes.
      WaitForReloadButtonStopIcon(), DoWaitForTime(base::Milliseconds(100)),
      ExpectReloadButtonMode(ReloadControl::Mode::kStop),
      // Click the stop button, and wait for the load to stop.
      ClickMouse(), DoWaitForLoadStop(),
      // We should show an enabled reload button.
      WaitForReloadButtonReady(),
      // Try to send a delayed response to the request - this shouldn't do
      // anything other than remove the queued HttpResponse, so the next request
      // can be responded to.
      DoSendDelayedResponse(),
      // Trigger another reload by pressing the button, and make sure it works.
      ClickMouse(), DoSendDelayedResponse(), DoWaitForLoadStop());

  // The stop button press should have resulted in an uncommitted navigation.
  EXPECT_EQ(observer.num_started_navigations(), 3u);
  EXPECT_EQ(observer.num_finished_navigations(), 3u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}

#if BUILDFLAG(IS_MAC)
// Regression test for the GlassFrame click-through bug: NSGlassEffectView
// was intercepting clicks on the WebUI reload button.
class WebUIToolbarGlassFrameInteractiveUiTest
    : public WebUIToolbarViewsInteractiveUiTest {
 public:
  WebUIToolbarGlassFrameInteractiveUiTest() {
    additional_features_.InitAndEnableFeature(features::kGlassFrame);
  }

  void SetUp() override {
    if (!features::IsGlassFrameEnabled()) {
      GTEST_SKIP() << "GlassFrame requires macOS 26.0+";
    }
    WebUIToolbarViewsInteractiveUiTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList additional_features_;
};

// Only the WebUI reload button path is affected.
INSTANTIATE_TEST_SUITE_P(All,
                         WebUIToolbarGlassFrameInteractiveUiTest,
                         testing::Values(true));

IN_PROC_BROWSER_TEST_P(WebUIToolbarGlassFrameInteractiveUiTest, ReloadButton) {
  const GURL url = embedded_test_server()->GetURL("/title1.html");

  ReloadButtonTestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  RunTestSequence(SetUpReloadButtonTest(), NavigateWebContents(TabId(), url),
                  WaitForReloadButtonReady(), MoveMouseOverReloadButton(),
                  ClickMouse(), WaitForWebContentsNavigation(TabId(), url));

  EXPECT_EQ(observer.num_started_navigations(), 2u);
  EXPECT_EQ(observer.num_finished_navigations(), 2u);
  EXPECT_EQ(observer.num_committed_navigations(), 2u);
}
#endif  // BUILDFLAG(IS_MAC)

#if defined(USE_AURA)
class TestDragDropClient : public aura::client::DragDropClient {
 public:
  explicit TestDragDropClient(aura::Window* root_window)
      : root_window_(root_window) {
    client_ = aura::client::GetDragDropClient(root_window_);
    aura::client::SetDragDropClient(root_window_, this);
  }
  ~TestDragDropClient() override {
    for (auto& observer : observers_) {
      observer.OnDragDropClientDestroying();
    }
    aura::client::SetDragDropClient(root_window_, client_);
  }

  // aura::client::DragDropClient:
  ui::mojom::DragOperation StartDragAndDrop(
      std::unique_ptr<ui::OSExchangeData> data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& screen_location,
      int allowed_operations,
      ui::mojom::DragEventSource source) override {
    drag_triggered_ = true;
    auto urls =
        data->GetURLs(ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
    if (!urls.empty()) {
      dragged_url_ = urls[0].url;
    }
    for (auto& observer : observers_) {
      observer.OnDragStarted();
    }
    return ui::mojom::DragOperation::kCopy;
  }
  void DragCancel() override {}
  bool IsDragDropInProgress() override { return false; }
  void AddObserver(aura::client::DragDropClientObserver* observer) override {
    observers_.AddObserver(observer);
    if (drag_triggered_) {
      observer->OnDragStarted();
    }
  }
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
    observers_.RemoveObserver(observer);
  }
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override {}
#endif  // BUILDFLAG(IS_LINUX)

  bool drag_triggered() const { return drag_triggered_; }
  const GURL& dragged_url() const { return dragged_url_; }

 private:
  bool drag_triggered_ = false;
  GURL dragged_url_;
  raw_ptr<aura::Window> root_window_;
  raw_ptr<aura::client::DragDropClient> client_;
  base::ObserverList<aura::client::DragDropClientObserver>::Unchecked
      observers_;
};
#endif  // defined(USE_AURA)

class WebUIToolbarViewsLocationBarInteractiveUiTest
    : public WebUIAndViewsToolbarInteractiveUiTestBase {
 public:
  WebUIToolbarViewsLocationBarInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIBackForwardButton,
         features::kWebUIReloadButton, features::kWebUIHomeButton,
         features::kWebUISplitTabsButton, features::kWebUILocationBar},
        {});
  }

  ~WebUIToolbarViewsLocationBarInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    WebUIAndViewsToolbarInteractiveUiTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Wait for the toolbar to load.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

 protected:
  const WebContentsInteractionTestUtil::DeepQuery kLocationIconDeepQuery = {
      "toolbar-app", "#location-bar", "location-icon"};

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragLocationIcon DragLocationIcon
#else
#define MAYBE_DragLocationIcon DISABLED_DragLocationIcon
#endif
IN_PROC_BROWSER_TEST_F(WebUIToolbarViewsLocationBarInteractiveUiTest,
                       MAYBE_DragLocationIcon) {
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");

  std::unique_ptr<TestDragDropClient> drag_drop_client;

  RunTestSequence(
      // Setup and navigate to a page.
      WaitForToolbarLoaded(), NavigateWebContents(TabId(), initial_url),

      // Wait until the icon is actually clickable.
      WaitForJsResultAt(WebUIToolbarId(), kLocationIconDeepQuery,
                        "el => el.clickable"),

      // Setup interception and trigger drag.
      Do(base::BindLambdaForTesting([&]() {
        auto* root_window = BrowserView::GetBrowserViewForBrowser(browser())
                                ->GetWidget()
                                ->GetNativeWindow()
                                ->GetRootWindow();
        drag_drop_client = std::make_unique<TestDragDropClient>(root_window);
      })),

      // Move to icon and perform drag gesture.
      MoveMouseTo(WebUIToolbarId(), kLocationIconDeepQuery),
      DragMouseTo(base::BindOnce([]() -> gfx::Point {
        return display::Screen::Get()->GetCursorScreenPoint() +
               gfx::Vector2d(0, 20);
      })),

      // Verify that drag was triggered with correct data.
      PollUntil(base::BindLambdaForTesting([&]() {
                  return drag_drop_client->drag_triggered() &&
                         drag_drop_client->dragged_url() == initial_url;
                }),
                "Drag was triggered with correct URL"),

      // Cleanup.
      Do(base::BindLambdaForTesting([&]() { drag_drop_client.reset(); })));
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
}

class WebUIToolbarFocusInteractiveUiTestBase
    : public WebUIAndViewsToolbarInteractiveUiTestBase {
 public:
  void SetUpOnMainThread() override {
    WebUIAndViewsToolbarInteractiveUiTestBase::SetUpOnMainThread();
    // Enable/pin home and split-tabs buttons so they can be focused.
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton,
                                                 true);
    // Wait for the toolbar to load.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

 protected:
  views::View* GetViewForIdentifier(Browser* browser,
                                    ui::ElementIdentifier el_id) {
    auto* element_tracker_views = views::ElementTrackerViews::GetInstance();
    ui::ElementContext context = BrowserElements::From(browser)->GetContext();
    return element_tracker_views->GetFirstMatchingView(el_id, context);
  }

  auto ExpectFocusedView(ui::ElementIdentifier element_id) {
    return Steps(PollUntil(
        base::BindRepeating(
            [](WebUIToolbarFocusInteractiveUiTestBase* test,
               ui::ElementIdentifier el_id) {
              views::View* expected_view =
                  test->GetViewForIdentifier(test->browser(), el_id);
              views::View* focused_view =
                  BrowserView::GetBrowserViewForBrowser(test->browser())
                      ->GetFocusManager()
                      ->GetFocusedView();
              return expected_view && focused_view &&
                     (focused_view == expected_view ||
                      expected_view->Contains(focused_view));
            },
            base::Unretained(this), element_id),
        "Wait for expected view to gain focus"));
  }

  auto ExpectFocusedWebUIElement(const std::string& expected_id) {
    return Steps(WaitForJsResultAt(
        WebUIToolbarId(),
        WebContentsInteractionTestUtil::DeepQuery({"toolbar-app"}),
        R"JS(
        el => {
          let active = el.shadowRoot.activeElement;
          while (active && active.shadowRoot &&
              active.shadowRoot.activeElement) {
            active = active.shadowRoot.activeElement;
          }
          if (!active) return '';
          let curr = active;
          while (curr && curr !== el) {
            if (curr.id && curr.id !== 'container' &&
                curr.id !== 'textInput' && curr.id !== 'button') {
              return curr.id;
            }
            if (curr.id === 'container') {
              return 'location-icon-container';
            }
            if (curr.id === 'textInput') {
              return 'omnibox-text-input';
            }
            let parent = curr.parentElement || curr.parentNode;
            if (parent && parent.host) {
              curr = parent.host;
            } else {
              curr = parent;
            }
          }
          return active.id || active.tagName;
        }
        )JS",
        expected_id));
  }
};

// Test focus traversal of with only WebUI navigation controls.
class WebUIToolbarFocusMinimalInteractiveUiTest
    : public WebUIToolbarFocusInteractiveUiTestBase {
 public:
  WebUIToolbarFocusMinimalInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIBackForwardButton,
         features::kWebUIReloadButton, features::kWebUIHomeButton,
         features::kWebUISplitTabsButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {features::kWebUILocationBar, features::kWebUIPinnedToolbarActions});
  }
  ~WebUIToolbarFocusMinimalInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarFocusMinimalInteractiveUiTest,
                       KeyboardNavigation) {
  // Navigate to a URL so that back button is enabled.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Move mouse off of toolbar. Having the mouse over the reload button when a
  // page finishes loading may temporarily disable the reload button, making it
  // no longer focusable, which will cause walking through focusable elements to
  // skip over it, and the test will then fail.
  RunTestSequence(MoveMouseTo(ToolbarView::kToolbarElementId,
                              base::BindOnce([](ui::TrackedElement* el) {
                                return el->GetScreenBounds().bottom_center() +
                                       gfx::Vector2d(0, 1);
                              })));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  // Navigate back once so forward is enabled too.
  content::TestNavigationObserver back_nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->command_controller()->ExecuteCommand(IDC_BACK);
  back_nav_observer.Wait();

  RunTestSequence(
      // 1. Wait for toolbar to load.
      WaitForToolbarLoaded(),

      // Wait for Lit rendering to complete (until Back button is rendered in
      // shadow DOM).
      WaitForElementVisible(WebUIToolbarId(),
                            DeepQuery({"toolbar-app", "#back"})),

      // 2. Focus the toolbar using the browser command (Alt+Shift+T).
      Do(base::BindLambdaForTesting([this]() {
        browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);
      })),
      ExpectFocusedWebUIElement("back"),

      // 3. ArrowRight -> Forward (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("forward"),

      // 4. ArrowRight -> Reload (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("reload"),

      // 5. ArrowRight -> Home (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("home"),

      // 6. ArrowRight -> SplitTabs (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("split-tabs"),

      // 7. ArrowRight -> LocationBar (Views!).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedView(kLocationBarElementId),

  // Focus is now in the C++ Omnibox. Escaping it via simulated Tab keys is
  // highly unstable in headless environments, so we programmatically set
  // pane focus on the Profile (Avatar) button instead.
#if BUILDFLAG(IS_CHROMEOS)
      // ChromeOS has no avatar button, so use app menu instead.
      Do(base::BindLambdaForTesting([this]() {
        auto* view =
            GetViewForIdentifier(browser(), kToolbarAppMenuButtonElementId);
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->SetPaneFocus(view);
      })),
      ExpectFocusedView(kToolbarAppMenuButtonElementId),
#else
      Do(base::BindLambdaForTesting([this]() {
        auto* view =
            GetViewForIdentifier(browser(), kToolbarAvatarButtonElementId);
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->SetPaneFocus(view);
      })),
      ExpectFocusedView(kToolbarAvatarButtonElementId),

      // 10. ArrowRight -> Chrome Menu (Views).
      SendKeyPress(kToolbarAvatarButtonElementId, ui::VKEY_RIGHT),
      ExpectFocusedView(kToolbarAppMenuButtonElementId),
#endif  // BUILDFLAG(IS_CHROMEOS)

      // 11. ArrowRight -> Back (WebUI, wrap-around!).
      SendKeyPress(kToolbarAppMenuButtonElementId, ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("back"),

      // 12. Now test backward navigation: ArrowLeft on Back -> Chrome Menu
      // (Views, wrap-around!).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_LEFT),
      ExpectFocusedView(kToolbarAppMenuButtonElementId),

#if !BUILDFLAG(IS_CHROMEOS)
      // Skip for ChromeOS which has no profile button.

      // 13. ArrowLeft -> Profile (Views).
      SendKeyPress(kToolbarAppMenuButtonElementId, ui::VKEY_LEFT),
      ExpectFocusedView(kToolbarAvatarButtonElementId),
#endif  // BUILDFLAG(IS_CHROMEOS)

      // 14. Test Home and End keys.
      // Focus WebUI WebView in C++ to ensure it has active pane focus before
      // key injection.
      Do(base::BindLambdaForTesting([this]() {
        auto* view = BrowserView::GetBrowserViewForBrowser(browser())
                         ->toolbar()
                         ->GetWebUIToolbarViewForTesting();
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->SetPaneFocus(view);
      })),
      ExpectFocusedView(kWebUIToolbarElementIdentifier),

      // Focus Reload button via JS.
      ExecuteJsAt(WebUIToolbarId(),
                  {"toolbar-app", "#reload", "cr-icon-button"},
                  "el => el.focus()"),
      ExpectFocusedWebUIElement("reload"),

      // Dispatch Home keydown event via JS to focus Back button.
      ExecuteJsAt(WebUIToolbarId(),
                  {"toolbar-app", "#reload", "cr-icon-button"},
                  "el => {"
                  "  const event = new KeyboardEvent('keydown', {key: 'Home', "
                  "bubbles: true, composed: true});"
                  "  "
                  "el.dispatchEvent(event);"
                  "}"),
      ExpectFocusedWebUIElement("back"),

      // Press End -> focuses Chrome Menu.
      SendKeyPress(WebUIToolbarId(), ui::VKEY_END),
      ExpectFocusedView(kToolbarAppMenuButtonElementId));
}

// Test focus traversal of all WebUI controls.
class WebUIToolbarFocusFullInteractiveUiTest
    : public WebUIToolbarFocusInteractiveUiTestBase {
 public:
  WebUIToolbarFocusFullInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIBackForwardButton,
         features::kWebUIReloadButton, features::kWebUIHomeButton,
         features::kWebUISplitTabsButton, features::kWebUILocationBar,
         features::kWebUIPinnedToolbarActions,
         ::tabs::kHorizontalTabStripComboButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }
  ~WebUIToolbarFocusFullInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarFocusFullInteractiveUiTest,
                       KeyboardNavigation) {
  // Navigate to a URL so that back/forward buttons are enabled.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Move mouse off of toolbar. Having the mouse over the reload button when a
  // page finishes loading may temporarily disable the reload button, making it
  // no longer focusable, which will cause walking through focusable elements to
  // skip over it, and the test will then fail.
  RunTestSequence(MoveMouseTo(ToolbarView::kToolbarElementId,
                              base::BindOnce([](ui::TrackedElement* el) {
                                return el->GetScreenBounds().bottom_center() +
                                       gfx::Vector2d(0, 1);
                              })));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  // Navigate back once so forward is enabled too.
  content::TestNavigationObserver back_nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->command_controller()->ExecuteCommand(IDC_BACK);
  back_nav_observer.Wait();

  RunTestSequence(
      // 1. Wait for toolbar to load.
      WaitForToolbarLoaded(),

      // Pin an action so that WebUIPinnedToolbarActions has at least one action
      // and is visible!
      Do(base::BindLambdaForTesting([this]() {
        PinnedToolbarActionsModel::Get(browser()->profile())
            ->UpdatePinnedState(kActionCopyUrl, true);
      })),

      // Wait for Lit rendering to complete (until Back button is rendered in
      // shadow DOM).
      WaitForElementVisible(WebUIToolbarId(),
                            DeepQuery({"toolbar-app", "#back"})),

      // 2. Focus the toolbar using the browser command (Alt+Shift+T).
      Do(base::BindLambdaForTesting([this]() {
        browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);
      })),
      ExpectFocusedWebUIElement("back"),

      // 3. ArrowRight to SplitTabs.
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("forward"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("reload"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("home"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("split-tabs"),

      // 4. ArrowRight -> LocationIcon (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("location-icon-container"),

      // 5. ArrowRight -> Omnibox input (WebUI).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("omnibox-text-input"),

      // 6. ArrowRight inside Omnibox should NOT shift focus.
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("omnibox-text-input"),

      // 7. Focus PinnedActions (WebUI) using JS to avoid keyboard tab flakiness
      // in test environments.
      ExecuteJsAt(WebUIToolbarId(),
                  {"toolbar-app", "#pinnedToolbarActions",
                   "pinned-toolbar-action", "cr-icon-button"},
                  "el => el.focus()"),
      ExpectFocusedWebUIElement("pinnedToolbarActions"),

#if BUILDFLAG(IS_CHROMEOS)
      // ChromeOS has no profile button, so skip to Chrome menu.

      // 8. ArrowRight -> Chrome Menu (Views).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedView(kToolbarAppMenuButtonElementId),
#else
      // 8. ArrowRight -> Profile (Views).
      // Since Extensions is hidden in the test profile, focus goes directly to
      // Profile.
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedView(kToolbarAvatarButtonElementId),

      // 10. ArrowRight -> Chrome Menu (Views).
      SendKeyPress(kToolbarAvatarButtonElementId, ui::VKEY_RIGHT),
      ExpectFocusedView(kToolbarAppMenuButtonElementId),
#endif  // BUILDFLAG(IS_CHROMEOS)

      // 11. ArrowRight -> Back (WebUI, wrap-around!).
      SendKeyPress(kToolbarAppMenuButtonElementId, ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("back"),

      // 12. Test Home and End keys.
      // Focus Reload button via JS.
      ExecuteJsAt(WebUIToolbarId(),
                  {"toolbar-app", "#reload", "cr-icon-button"},
                  "el => el.focus()"),
      ExpectFocusedWebUIElement("reload"),

      // Dispatch Home keydown event via JS to focus Back button.
      ExecuteJsAt(WebUIToolbarId(),
                  {"toolbar-app", "#reload", "cr-icon-button"},
                  "el => {"
                  "  const event = new KeyboardEvent('keydown', {key: 'Home', "
                  "bubbles: true, composed: true});"
                  "  "
                  "el.dispatchEvent(event);"
                  "}"),
      ExpectFocusedWebUIElement("back"),

      // Press End -> focuses last pane element (Chrome Menu).
      SendKeyPress(WebUIToolbarId(), ui::VKEY_END),
      ExpectFocusedView(kToolbarAppMenuButtonElementId));
}

class WebUIToolbarFocusFullRtlInteractiveUiTest
    : public WebUIToolbarFocusFullInteractiveUiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIToolbarFocusFullInteractiveUiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                    switches::kForceDirectionRTL);
  }
};

// In RTL mode, right arrow should move to previous control, not next one.
// (and correspondingly for left arrow)
IN_PROC_BROWSER_TEST_F(WebUIToolbarFocusFullRtlInteractiveUiTest,
                       KeyboardNavigation) {
  // Navigate to a URL so that back/forward buttons are enabled.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Move mouse off of toolbar. Having the mouse over the reload button when a
  // page finishes loading may temporarily disable the reload button, making it
  // no longer focusable, which will cause walking through focusable elements to
  // skip over it, and the test will then fail.
  RunTestSequence(MoveMouseTo(ToolbarView::kToolbarElementId,
                              base::BindOnce([](ui::TrackedElement* el) {
                                return el->GetScreenBounds().bottom_center() +
                                       gfx::Vector2d(0, 3);
                              })));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  // Navigate back once so forward is enabled too.
  content::TestNavigationObserver back_nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  browser()->command_controller()->ExecuteCommand(IDC_BACK);
  back_nav_observer.Wait();

  RunTestSequence(
      // 1. Wait for toolbar to load.
      WaitForToolbarLoaded(),

      // Wait for Lit rendering to complete (until Back button is rendered in
      // shadow DOM).
      WaitForElementVisible(WebUIToolbarId(),
                            DeepQuery({"toolbar-app", "#back"})),

      // 2. Focus the toolbar using the browser command (Alt+Shift+T).
      Do(base::BindLambdaForTesting([this]() {
        browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);
      })),
      ExpectFocusedWebUIElement("back"),

      // 3. ArrowLeft to advance all the way to split-tabs button
      SendKeyPress(WebUIToolbarId(), ui::VKEY_LEFT),
      ExpectFocusedWebUIElement("forward"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_LEFT),
      ExpectFocusedWebUIElement("reload"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_LEFT),
      ExpectFocusedWebUIElement("home"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_LEFT),
      ExpectFocusedWebUIElement("split-tabs"),

      // Now ArrowRight should move back.
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("home"),
      SendKeyPress(WebUIToolbarId(), ui::VKEY_RIGHT),
      ExpectFocusedWebUIElement("reload"));
}
