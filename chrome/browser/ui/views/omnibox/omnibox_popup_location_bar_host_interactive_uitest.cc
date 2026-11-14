// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

namespace {

// Defines `sb` (<omnibox-popup-searchbox>), `wrapper` (the unbounded
// #inputWrapper),
// `crInput` and `input` (the native <input>) for scripts run in the popup
// WebContents. Scripts passed through `content::JsReplace` must not use
// `${...}` template literals (JsReplace strips `$` + next char).
constexpr char kPrelude[] = R"(
  const sb = document.querySelector('omnibox-full-app').shadowRoot
                 .querySelector('omnibox-popup-searchbox');
  const wrapper = sb.shadowRoot.querySelector('#inputWrapper');
  const crInput = sb.shadowRoot.querySelector('#input');
  const input = crInput.inputElement;
)";

constexpr char kDropdownOpen[] =
    "sb.dropdownIsVisible && wrapper.matches(':unbounded')";
constexpr char kDropdownClosed[] =
    "!sb.dropdownIsVisible && !wrapper.matches(':unbounded')";
// The native <input> is the focused element of a focused document.
constexpr char kInputFocused[] =
    "document.hasFocus() && sb.shadowRoot.activeElement === crInput && "
    "crInput.shadowRoot.activeElement === input";

// Whether `views::Widget::Deactivate()` can deactivate the browser window.
// `NativeWidgetMac::Deactivate()` is not implemented, and Wayland doesn't let
// a client deactivate its own window.
bool CanDeactivateWidget() {
#if BUILDFLAG(IS_MAC)
  return false;
#elif BUILDFLAG(IS_OZONE)
  return !ui::OzonePlatform::RunningOnWaylandForTest();
#else
  return true;
#endif
}

// Counts the autocomplete queries started while alive.
class AutocompleteStartCounter : public AutocompleteController::Observer {
 public:
  explicit AutocompleteStartCounter(AutocompleteController* controller) {
    observation_.Observe(controller);
  }

  int count() const { return count_; }

  // AutocompleteController::Observer:
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override {
    ++count_;
  }

 private:
  int count_ = 0;
  base::ScopedObservation<AutocompleteController,
                          AutocompleteController::Observer>
      observation_{this};
};

// Records omnibox focus state and popup state changes while alive.
class OmniboxStateRecorder : public OmniboxTabHelper::Observer {
 public:
  OmniboxStateRecorder(content::WebContents* tab,
                       OmniboxPopupStateManager* popup_state_manager) {
    observation_.Observe(OmniboxTabHelper::FromWebContents(tab));
    subscription_ = popup_state_manager->AddPopupStateChangedCallback(
        base::BindLambdaForTesting(
            [this](OmniboxPopupState old_state, OmniboxPopupState new_state) {
              popup_states_.push_back(new_state);
            }));
  }

  const std::vector<OmniboxFocusState>& focus_states() const {
    return focus_states_;
  }
  const std::vector<OmniboxPopupState>& popup_states() const {
    return popup_states_;
  }

  // OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override {
    focus_states_.push_back(state);
  }
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

 private:
  std::vector<OmniboxFocusState> focus_states_;
  std::vector<OmniboxPopupState> popup_states_;
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      observation_{this};
  base::CallbackListSubscription subscription_;
};

}  // namespace

class OmniboxPopupLocationBarHostTest : public InProcessBrowserTest {
 public:
  OmniboxPopupLocationBarHostTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxFullPopup,
          {{"Omnibox_HostUnboundedInLocationBar", "true"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::internal::kWebUIOmniboxAimPopup, {}},
         {omnibox::kOmniboxFullWebUIEvictOnHide, {}},
         {omnibox::kOmniboxFullWebUIDetachWebContentsOnHide, {}},
         {omnibox::kOmniboxWebUIPopupMarkAsHidden, {}}},
        {omnibox::kOmniboxAimPopupRequireEligibility,
         // Avoid macOS menu bar NSStatusItem setup stalling
         // PreRunTestOnMainThread.
         omnibox::kOmniboxEverywhere,
         // These tests exercise the popup hosted in the Views
         // LocationBarView, which the WebUI toolbar and WebUI
         // location bar replace.
         features::kWebUILocationBar, features::kWebUIToolbar});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Widgets must close before the browser process tears down.
    other_widget_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }
  LocationBarView* location_bar() {
    return browser_view()->toolbar()->location_bar_view();
  }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxController* controller() {
    return location_bar()->GetOmniboxController();
  }
  OmniboxEditModel* edit_model() { return controller()->edit_model(); }
  OmniboxPopupState popup_state() {
    return controller()->popup_state_manager()->popup_state();
  }
  OmniboxPopupViewFullWebUI* popup_view() {
    return static_cast<OmniboxPopupViewFullWebUI*>(
        location_bar()->GetOmniboxPopupView());
  }
  OmniboxFullPopupWebUIContent* web_view() {
    return static_cast<OmniboxFullPopupWebUIContent*>(
        popup_view()->popup_host()->GetWebUIContent());
  }
  // The popup's WebContents, even while it is detached from `web_view()`.
  content::WebContents* popup_contents() {
    return web_view() ? web_view()->GetWrappedWebContents() : nullptr;
  }
  content::WebContents* active_tab() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  void NavigateTo(const std::string& path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(path)));
  }

  // Runs `body` (with `kPrelude` in scope) as an async function in the popup.
  content::EvalJsResult Eval(const std::string& body) {
    return content::EvalJs(
        popup_contents(),
        "(async () => {" + std::string(kPrelude) + body + "})()");
  }

  // Waits up to `TestTimeouts::action_timeout()` for the JS `condition` to
  // hold.
  [[nodiscard]] bool WaitForJs(const std::string& condition) {
    const std::string timeout_ms =
        base::NumberToString(TestTimeouts::action_timeout().InMilliseconds());
    const content::EvalJsResult result =
        Eval("const deadline = Date.now() + " + timeout_ms + "; while (!(" +
             condition +
             ")) {"
             "  if (Date.now() > deadline) { return false; }"
             "  await new Promise(r => setTimeout(r, 10));"
             "} return true;");
    if (result != true) {
      ADD_FAILURE() << "Failed waiting for: " << condition << ": " << result;
      return false;
    }
    return true;
  }

  [[nodiscard]] bool WaitForInputValue(const std::string& value) {
    return WaitForJs(content::JsReplace("input.value === $1", value));
  }

  // Focuses the omnibox and waits until the embedded WebUI is bound, its input
  // is focused and it shows the native omnibox text.
  content::WebContents* FocusAndWaitForPopup() {
    chrome::FocusLocationBar(browser());
    EXPECT_TRUE(base::test::RunUntil(
        [&] { return popup_view()->IsOpen() && popup_contents(); }));
    content::WebContents* contents = popup_contents();
    if (!contents) {
      ADD_FAILURE() << "No popup WebContents";
      return nullptr;
    }
    EXPECT_TRUE(content::WaitForLoadStop(contents));
    EXPECT_TRUE(base::test::RunUntil([&] {
      return static_cast<OmniboxPopupView*>(popup_view())
          ->IsPopupHandlerReady();
    }));
    EXPECT_EQ(true, content::EvalJs(contents, R"((async () => {
      await customElements.whenDefined('omnibox-full-app');
      await customElements.whenDefined('omnibox-popup-searchbox');
      const ready = () => {
        const app = document.querySelector('omnibox-full-app');
        const sb = app && app.shadowRoot &&
            app.shadowRoot.querySelector('omnibox-popup-searchbox');
        return sb && sb.shadowRoot && sb.shadowRoot.querySelector('#input');
      };
      while (!ready()) {
        await new Promise(r => setTimeout(r, 10));
      }
      return true;
    })())"));
    EXPECT_TRUE(WaitForJs(content::JsReplace(
        "sb.shadowRoot.activeElement === crInput && input.value === $1",
        base::UTF16ToUTF8(omnibox_view()->GetText()))));
    return contents;
  }

  // Types `text` into the WebUI input and waits for the dropdown and for the
  // edit model to receive the text.
  void TypeAndWaitForDropdown(const std::string& text) {
    // Window deactivation (see `ActivateBrowserWindow()`) can dismiss the
    // dropdown, so this types again until the dropdown opens.
    const std::string script = std::string(R"(
      input.focus();
      input.value = $1;
      input.setSelectionRange(input.value.length, input.value.length);
      input.dispatchEvent(new InputEvent('input', {
        data: $1, inputType: 'insertText', bubbles: true, composed: true}));
      const start = Date.now();
      while (!()") + kDropdownOpen +
                               R"()) {
        if (Date.now() - start > 3000) {
          return false;
        }
        await new Promise(r => setTimeout(r, 10));
      }
      return true;)";
    bool open = false;
    for (int i = 0; i < 3 && !open; ++i) {
      ASSERT_TRUE(ActivateBrowserWindow());
      open = Eval(content::JsReplace(script, text)) == true;
    }
    ASSERT_TRUE(open) << "The dropdown didn't open for: " << text;
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return edit_model()->user_text() == base::UTF8ToUTF16(text); }));
    // Wait for async autocomplete providers to finish so a late result cannot
    // reopen the dropdown or reset selection while the test acts.
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return controller()->autocomplete_controller()->done(); }));
    ASSERT_EQ(true, Eval("return true;"));
  }

  // Sends one Escape key event to the popup page. Unlike `PressKey()`, this
  // lets a test act between the keydown and the keyup.
  void SendEscapeKeyEvent(blink::WebInputEvent::Type type) {
    input::NativeWebKeyboardEvent event(
        type, blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
    event.windows_key_code = ui::VKEY_ESCAPE;
    event.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ESCAPE);
    event.dom_code = static_cast<int>(ui::DomCode::ESCAPE);
    event.dom_key = ui::DomKey::ESCAPE;
    popup_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->ForwardKeyboardEvent(event);
  }

  // Presses Escape as a user does: the keyup comes after the unbounded window,
  // which consumes the keydown, requested a dismissal.
  void PressEscape() {
    ASSERT_EQ(true, Eval(R"(
      window.escapeDismissals = 0;
      wrapper.addEventListener('beforetoggle', e => {
        if (e.cancelable) {
          ++window.escapeDismissals;
        }
      }, {once: true});
      return true;)"));
    SendEscapeKeyEvent(blink::WebInputEvent::Type::kRawKeyDown);
    ASSERT_TRUE(WaitForJs("window.escapeDismissals > 0"));
    SendEscapeKeyEvent(blink::WebInputEvent::Type::kKeyUp);
  }

  // Sends a trusted key press to the popup WebContents.
  void PressKey(ui::DomKey key,
                ui::DomCode code,
                ui::KeyboardCode key_code,
                bool shift = false) {
    content::SimulateKeyPress(popup_contents(), key, code, key_code,
                              /*control=*/false, shift, /*alt=*/false,
                              /*command=*/false);
  }

  [[nodiscard]] bool WaitForPopupClosed() {
    return base::test::RunUntil([&] { return !popup_view()->IsOpen(); });
  }

  // Opens `path` from the omnibox, which closes the popup, then waits for the
  // hidden content to detach its WebContents.
  void NavigateFromOmniboxAndWaitForDetach(const std::string& path) {
    const GURL target = embedded_test_server()->GetURL(path);
    TypeAndWaitForDropdown(target.spec());
    content::TestNavigationObserver observer(active_tab());
    PressKey(ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN);
    observer.Wait();
    ASSERT_EQ(target, active_tab()->GetLastCommittedURL());
    ASSERT_TRUE(WaitForPopupClosed());
    ASSERT_TRUE(
        base::test::RunUntil([&] { return !web_view()->web_contents(); }));
  }

  // Waits until the browser window is active. These tests need an active
  // window, which is why they're interactive_ui_tests (run one at a time), but
  // the window can still lose activation, e.g. after `Widget::Deactivate()`.
  // While the browser widget is inactive,
  // `views::FocusManager::SetFocusedView()` only stores the view and activates
  // the widget, so a views focus change the test makes (such as focusing the
  // omnibox WebView) doesn't happen until the window is active again. Call this
  // right before a views focus change, with no message loop pump in between: a
  // deactivation can then only be observed after the focus change ran.
  [[nodiscard]] bool ActivateBrowserWindow() {
    views::Widget* widget = browser_view()->GetWidget();
    // `BringBrowserWindowToFront()` returns on the activation, after which the
    // same batch of X events may already have deactivated the window again.
    for (int i = 0; i < 5 && !widget->IsActive(); ++i) {
      if (!ui_test_utils::BringBrowserWindowToFront(browser())) {
        return false;
      }
    }
    return widget->IsActive();
  }

  // Takes activation from the browser window by showing and activating an
  // in-process top-level widget at `bounds` (kept in `other_widget_`).
  [[nodiscard]] bool DeactivateBrowserWindowWithWidget(
      const gfx::Rect& bounds) {
    views::Widget* widget = browser_view()->GetWidget();
    other_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = bounds;
    other_widget_->Init(std::move(params));
    other_widget_->Show();
    other_widget_->Activate();
    return base::test::RunUntil([&] { return !widget->IsActive(); });
  }

  // Takes activation from the browser window, with `Widget::Deactivate()` or,
  // where that can't work, another top-level widget kept in `other_widget_`.
  [[nodiscard]] bool DeactivateBrowserWindow() {
    if (!CanDeactivateWidget()) {
      return DeactivateBrowserWindowWithWidget(gfx::Rect(0, 0, 200, 200));
    }
    views::Widget* widget = browser_view()->GetWidget();
    widget->Deactivate();
    return base::test::RunUntil([&] { return !widget->IsActive(); });
  }

  // Lets the tasks the host posts on an activation change run.
  void RunPostedActivationTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  std::unique_ptr<views::Widget> other_widget_;

  // Waits until the omnibox is focused everywhere: the edit model, views focus
  // (in the content), and the WebUI input in the DOM.
  [[nodiscard]] bool WaitForOmniboxRefocused() {
    views::FocusManager* focus_manager = browser_view()->GetFocusManager();
    if (!base::test::RunUntil([&] {
          return edit_model()->has_focus() &&
                 web_view()->Contains(focus_manager->GetFocusedView());
        })) {
      ADD_FAILURE() << "Omnibox not refocused: has_focus="
                    << edit_model()->has_focus() << ", focused view="
                    << (focus_manager->GetFocusedView()
                            ? focus_manager->GetFocusedView()->GetClassName()
                            : "null");
      return false;
    }
    return WaitForJs(kInputFocused);
  }

  // Moves focus to the active tab's contents, as clicking the page does.
  void BlurToWebContents() {
    ASSERT_TRUE(ActivateBrowserWindow());
    browser_view()->GetActiveContentsWebView()->RequestFocus();
    active_tab()->Focus();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The WebView is a child of (and fills) LocationBarView, paints above its
// siblings, and starts with the dropdown closed: unbounded wrapper filling the
// WebView.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       CreatesViewAndFillsLocationBar) {
  ASSERT_TRUE(omnibox::ShouldHostUnboundedOmniboxInLocationBar());
  // Stays on about:blank, which has no zero-suggest dropdown.
  content::WebContents* contents = FocusAndWaitForPopup();

  EXPECT_EQ(location_bar(), web_view()->parent());
  EXPECT_EQ(location_bar()->GetLocalBounds(), web_view()->bounds());
  EXPECT_EQ(web_view(), location_bar()->children().back());
  EXPECT_EQ(GURL(chrome::kChromeUIOmniboxPopupURL),
            contents->GetLastCommittedURL());
  EXPECT_EQ(OmniboxPopupState::kFull, popup_state());
  EXPECT_EQ(true, Eval(std::string(R"(
    const rect = wrapper.getBoundingClientRect();
    return sb.hasAttribute('unbounded-supported_') &&
           wrapper.hasAttribute('unbounded') && rect.x === 0 &&
           rect.y === 0 && rect.width === window.innerWidth &&
           rect.height === window.innerHeight && )") +
                       kDropdownClosed + ";"));
}

// The content reports itself presented once the compositor presents a frame
// after it is shown, and showing it again (as `OnFocus()` does) keeps that.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       ReportsPresentedAfterFirstFrame) {
  FocusAndWaitForPopup();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return web_view()->popup_presented_for_testing(); }));

  popup_view()->popup_host()->Show();
  EXPECT_TRUE(web_view()->popup_presented_for_testing());
}

// Typing reaches the edit model and pops the dropdown out into an unbounded
// window, while the WebView itself stays inside LocationBarView.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       TypingOpensUnboundedDropdown) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("chrome://version");

  EXPECT_TRUE(edit_model()->user_input_in_progress());
  EXPECT_FALSE(controller()->autocomplete_controller()->result().empty());
  EXPECT_EQ(location_bar()->GetLocalBounds(), web_view()->bounds());
  EXPECT_EQ(web_view(), location_bar()->children().back());

#if defined(USE_AURA)
  aura::Window* owner =
      popup_contents()->GetRenderWidgetHostView()->GetNativeView();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !wm::GetTransientChildren(owner).empty(); }));
  aura::Window* unbounded_window = wm::GetTransientChildren(owner).back();
  EXPECT_TRUE(unbounded_window->IsVisible());
  EXPECT_GT(unbounded_window->GetBoundsInScreen().height(),
            location_bar()->GetBoundsInScreen().height());
#endif

  // Arrowing unhides the screen-reader #selectionListbox; it must not stretch
  // the unbounded wrapper (regression test for its `left: -10000px`).
  PressKey(ui::DomKey::ARROW_DOWN, ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN);
  ASSERT_TRUE(WaitForJs(
      "!crInput.shadowRoot.querySelector('#selectionListbox').hidden"));
  EXPECT_EQ(true, Eval(R"(
    return wrapper.matches(':unbounded') &&
           wrapper.getBoundingClientRect().width <
               sb.getBoundingClientRect().width + 50;)"));
#if defined(USE_AURA)
  EXPECT_LT(unbounded_window->GetBoundsInScreen().width(),
            location_bar()->GetBoundsInScreen().width() + 200);
#endif
}

IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       EnterNavigatesActiveTabAndClosesPopup) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  const GURL target = embedded_test_server()->GetURL("/title2.html");
  TypeAndWaitForDropdown(target.spec());

  // Opening the match closes the popup once, and nothing reopens it from
  // inside that change.
  std::vector<OmniboxPopupState> states;
  base::CallbackListSubscription subscription =
      controller()->popup_state_manager()->AddPopupStateChangedCallback(
          base::BindLambdaForTesting(
              [&](OmniboxPopupState old_state, OmniboxPopupState new_state) {
                states.push_back(new_state);
              }));
  content::TestNavigationObserver observer(active_tab());
  PressKey(ui::DomKey::ENTER, ui::DomCode::ENTER, ui::VKEY_RETURN);
  observer.Wait();
  EXPECT_EQ(target, active_tab()->GetLastCommittedURL());
  ASSERT_TRUE(WaitForPopupClosed());
  EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
  EXPECT_THAT(states, testing::ElementsAre(OmniboxPopupState::kNone));

  // The hidden content holds neither views focus nor stored focus.
  ASSERT_TRUE(ActivateBrowserWindow());
  EXPECT_FALSE(web_view()->GetVisible());
  views::FocusManager* focus_manager = browser_view()->GetFocusManager();
  EXPECT_FALSE(web_view()->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(web_view()->Contains(focus_manager->GetStoredFocusView()));
  EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
}

// Reopening after a close reattaches the same page, which shows the new URL
// and still sends typing to the edit model.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       ReopenAfterCloseReattachesPage) {
  NavigateTo("/title1.html");
  content::WebContents* contents = FocusAndWaitForPopup();
  ASSERT_TRUE(contents);
  ASSERT_NO_FATAL_FAILURE(NavigateFromOmniboxAndWaitForDetach("/title2.html"));

  // `FocusAndWaitForPopup()` also waits for the input to show the omnibox
  // text.
  EXPECT_EQ(contents, FocusAndWaitForPopup());
  EXPECT_EQ(contents, web_view()->web_contents());
  EXPECT_NE(omnibox_view()->GetText().find(u"title2.html"),
            std::u16string::npos);
  TypeAndWaitForDropdown("title");
}

// The content keeps filling LocationBarView when the location bar resizes.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       ContentFollowsLocationBarResize) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  const gfx::Rect old_bounds = location_bar()->bounds();
  gfx::Rect window_bounds =
      browser_view()->GetWidget()->GetWindowBoundsInScreen();
  window_bounds.set_width(window_bounds.width() - 100);
  browser_view()->GetWidget()->SetBounds(window_bounds);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return location_bar()->bounds().width() != old_bounds.width(); }));

  EXPECT_TRUE(web_view()->GetVisible());
  EXPECT_EQ(location_bar()->GetLocalBounds(), web_view()->bounds());
  EXPECT_EQ(web_view(), location_bar()->children().back());
}

// `RequestFocus()` on a hidden host doesn't attach the page or reopen the
// popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       RequestFocusWhileHiddenDoesNothing) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  ASSERT_NO_FATAL_FAILURE(NavigateFromOmniboxAndWaitForDetach("/title2.html"));

  popup_view()->popup_host()->RequestFocus();
  EXPECT_FALSE(web_view()->web_contents());
  EXPECT_FALSE(web_view()->GetVisible());
  EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
}

// A trusted Escape reaches the searchbox's staged Escape handling: the
// unbounded window consumes the keydown and requests a dismissal, which the
// page cancels and replays the keydown on keyup. The first Escape reverts
// the selected suggestion to the typed text, the second closes the dropdown.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       SearchboxEscapeIsStaged) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("title");

  PressKey(ui::DomKey::ARROW_DOWN, ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN);
  ASSERT_TRUE(WaitForJs("input.value !== 'title'"));

  PressEscape();
  EXPECT_TRUE(WaitForInputValue("title"));
  EXPECT_EQ(true, Eval(std::string("return ") + kDropdownOpen + ";"));

  PressEscape();
  EXPECT_TRUE(WaitForJs(kDropdownClosed));
  EXPECT_EQ(u"title", edit_model()->user_text());
}

// Closing the unbounded element, as light dismiss (a real Escape or an outside
// click) does, closes the dropdown through its `beforetoggle` event.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       HidingUnboundedElementClosesDropdown) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("title");

  // A programmatic hide fires `beforetoggle` like light dismiss does.
  ASSERT_EQ(true, Eval("await wrapper.hideUnboundedElement(); return true;"));
  EXPECT_TRUE(WaitForJs(kDropdownClosed));
}

// Moving views focus to the page blurs the omnibox and hides the WebView.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       FocusingWebContentsClosesPopup) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  EXPECT_TRUE(edit_model()->has_focus());

  BlurToWebContents();

  ASSERT_TRUE(WaitForPopupClosed());
  EXPECT_FALSE(web_view()->GetVisible());
  EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
  EXPECT_FALSE(edit_model()->has_focus());
  EXPECT_TRUE(omnibox_view()->GetVisible());
}

// Tab / Shift+Tab in the WebUI move focus out of LocationBarView (not trapped
// in it) and close the popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       AdvanceFocusTabAndShiftTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
  views::FocusManager* focus_manager = browser_view()->GetFocusManager();

  for (bool reverse : {false, true}) {
    SCOPED_TRACE(reverse ? "Shift+Tab" : "Tab");
    FocusAndWaitForPopup();
    // Restores the focus stored while the window was inactive.
    ASSERT_TRUE(ActivateBrowserWindow());
    EXPECT_EQ(web_view(), focus_manager->GetFocusedView());
    // A forward Tab from the input stops on the AIM button when it is shown
    // (`OmniboxPopupSearchboxElement.handleKeyNavigation()`); the next Tab, or
    // a Shift+Tab from the input, asks the browser to advance focus. Neither
    // step depends on the WebUI seeing a `blur`, which window deactivation
    // also sends.
    if (!reverse && Eval("return sb.aimButtonVisible_ === true;") == true) {
      PressKey(ui::DomKey::TAB, ui::DomCode::TAB, ui::VKEY_TAB);
      ASSERT_TRUE(
          WaitForJs("sb.shadowRoot.activeElement === sb.$.composeButton"));
    }
    PressKey(ui::DomKey::TAB, ui::DomCode::TAB, ui::VKEY_TAB, reverse);
    // `OmniboxPopupWebUIBaseContent::AdvanceFocus()` moves focus past
    // LocationBarView and closes the popup in the same task.
    ASSERT_TRUE(WaitForPopupClosed());
    EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
    EXPECT_FALSE(edit_model()->has_focus());
    ASSERT_TRUE(ActivateBrowserWindow());
    ASSERT_TRUE(focus_manager->GetFocusedView());
    EXPECT_FALSE(location_bar()->Contains(focus_manager->GetFocusedView()));
  }
}

// Focus moving from the content to another child of LocationBarView, as
// Shift+Tab onto the location icon does, is a blur: the omnibox loses focus,
// the content closes and focus leaves it. Uses an accessibility-focusable test
// view, since the location icon isn't focusable on macOS without Full Keyboard
// Access.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       FocusingLocationBarChildClosesContent) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  ASSERT_TRUE(ActivateBrowserWindow());
  views::FocusManager* focus_manager = browser_view()->GetFocusManager();
  ASSERT_EQ(web_view(), focus_manager->GetFocusedView());

  views::View* child =
      location_bar()->AddChildView(std::make_unique<views::View>());
  child->SetBounds(0, 0, 10, 10);
  child->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  child->GetViewAccessibility().SetName(u"Test child");
  child->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  ASSERT_TRUE(child->GetViewAccessibility().IsAccessibilityFocusable());
  focus_manager->SetFocusedView(child);
  ASSERT_TRUE(WaitForPopupClosed());
  EXPECT_FALSE(edit_model()->has_focus());
  EXPECT_FALSE(web_view()->GetVisible());
  EXPECT_EQ(OmniboxPopupState::kNone, popup_state());
  EXPECT_FALSE(web_view()->Contains(focus_manager->GetFocusedView()));
  location_bar()->RemoveChildViewT(child);
}

// LocationBarView::FocusLocation() moves views focus from the content to
// LocationBarView before forwarding it back to the WebUI. That hop stays
// within the omnibox, so it doesn't blur it or close the popup.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       FocusLocationWhileFocusedDoesNotBlur) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  views::FocusManager* focus_manager = browser_view()->GetFocusManager();

  for (const bool with_typed_text : {false, true}) {
    SCOPED_TRACE(with_typed_text ? "with typed text" : "no typed text");
    if (with_typed_text) {
      TypeAndWaitForDropdown("abc");
    }
    ASSERT_TRUE(ActivateBrowserWindow());
    ASSERT_EQ(web_view(), focus_manager->GetFocusedView());
    const std::u16string expected_text = omnibox_view()->GetText();
    const std::u16string expected_user_text = edit_model()->user_text();

    OmniboxStateRecorder recorder(active_tab(),
                                  controller()->popup_state_manager());
    chrome::FocusLocationBar(browser());
    RunPostedActivationTasks();
    ASSERT_EQ(true, Eval("return true;"));

    EXPECT_THAT(recorder.popup_states(), testing::IsEmpty());
    EXPECT_THAT(recorder.focus_states(), testing::IsEmpty());
    EXPECT_TRUE(edit_model()->has_focus());
    EXPECT_EQ(OmniboxPopupState::kFull, popup_state());
    EXPECT_EQ(expected_text, omnibox_view()->GetText());
    EXPECT_EQ(expected_user_text, edit_model()->user_text());
    EXPECT_TRUE(WaitForInputValue(base::UTF16ToUTF8(expected_text)));
  }
}

// Deactivating the window blurs the omnibox, as for the Views omnibox: the
// WebUI is told, and without a draft the content hides. Reactivating it
// refocuses the omnibox without a zero-prefix query.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       WindowDeactivationBlursAndReactivationRefocuses) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  ASSERT_TRUE(ActivateBrowserWindow());

  ASSERT_TRUE(DeactivateBrowserWindow()) << "The browser window stayed active.";
  ASSERT_TRUE(base::test::RunUntil([&] { return !edit_model()->has_focus(); }));
  EXPECT_FALSE(popup_view()->is_focused());
  ASSERT_TRUE(WaitForPopupClosed());
  EXPECT_FALSE(web_view()->GetVisible());

  AutocompleteStartCounter starts(controller()->autocomplete_controller());
  other_widget_.reset();
  ASSERT_TRUE(ActivateBrowserWindow());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return edit_model()->has_focus() && web_view()->GetVisible() &&
           browser_view()->GetFocusManager()->GetFocusedView() == web_view();
  }));
  EXPECT_TRUE(popup_view()->is_focused());
  EXPECT_EQ(OmniboxPopupState::kFull, popup_state());
  EXPECT_TRUE(WaitForJs("sb.shadowRoot.activeElement === crInput"));
  EXPECT_EQ(0, starts.count());
}

#if !BUILDFLAG(IS_MAC)
// Clicking the page to reactivate the window focuses the page and leaves the
// omnibox unfocused, without showing the content in between.
// On Mac, RenderWidgetHostViewCocoa does not accept first mouse, so clicking an
// inactive window's page only activates the window and restores focus to the
// omnibox (as for OmniboxViewViews); closing `other_widget_` also already
// reactivates the browser window there.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       ClickingPageToReactivateDoesNotRefocus) {
#if BUILDFLAG(IS_OZONE)
  // TODO(crbug.com/584371333): Closing `other_widget_` reactivates the browser
  // window before the click on Wayland, which refocuses the omnibox.
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "The compositor reactivates the window before the click.";
  }
#endif
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  ASSERT_TRUE(ActivateBrowserWindow());
  views::View* contents_view = browser_view()->GetActiveContentsWebView();
#if BUILDFLAG(IS_WIN)
  // `Widget::Deactivate()` calls `::SetForegroundWindow(next_hwnd)`, which on
  // CI bots can activate an unrelated window occluding `contents_view`.
  // Deactivate via an in-process widget disjoint from the click point and
  // leave it open until the click reactivates the browser.
  const gfx::Point click_pt = contents_view->GetBoundsInScreen().CenterPoint();
  const int other_x = click_pt.x() >= 200 ? 0 : click_pt.x() + 100;
  ASSERT_TRUE(
      DeactivateBrowserWindowWithWidget(gfx::Rect(other_x, 0, 100, 100)))
      << "The browser window stayed active.";
#else
  ASSERT_TRUE(DeactivateBrowserWindow()) << "The browser window stayed active.";
#endif
  ASSERT_TRUE(WaitForPopupClosed());
  RunPostedActivationTasks();

  std::vector<OmniboxPopupState> states;
  base::CallbackListSubscription subscription =
      controller()->popup_state_manager()->AddPopupStateChangedCallback(
          base::BindLambdaForTesting(
              [&](OmniboxPopupState old_state, OmniboxPopupState new_state) {
                states.push_back(new_state);
              }));
#if !BUILDFLAG(IS_WIN)
  other_widget_.reset();
#endif
  ui_test_utils::ClickOnView(contents_view);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return browser_view()->GetWidget()->IsActive(); }));
  RunPostedActivationTasks();

  EXPECT_FALSE(edit_model()->has_focus());
  EXPECT_FALSE(web_view()->GetVisible());
  EXPECT_EQ(contents_view, browser_view()->GetFocusManager()->GetFocusedView());
  EXPECT_TRUE(active_tab()->GetRenderWidgetHostView()->HasFocus());
  EXPECT_THAT(states, testing::IsEmpty());
}
#endif  // !BUILDFLAG(IS_MAC)

// Reactivating the window after a deactivation that kept a draft refocuses the
// omnibox, keeping the draft, without a zero-prefix query.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       ReactivationRefocusesKeepingDraft) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("draft");
  ASSERT_TRUE(ActivateBrowserWindow());

  ASSERT_TRUE(DeactivateBrowserWindow()) << "The browser window stayed active.";
  ASSERT_TRUE(base::test::RunUntil([&] { return !edit_model()->has_focus(); }));
  ASSERT_TRUE(web_view()->GetVisible());
  EXPECT_TRUE(WaitForJs(kDropdownClosed));

  AutocompleteStartCounter starts(controller()->autocomplete_controller());
  other_widget_.reset();
  ASSERT_TRUE(ActivateBrowserWindow());
  ASSERT_TRUE(WaitForOmniboxRefocused());
  EXPECT_EQ(u"draft", edit_model()->user_text());
  EXPECT_EQ(OmniboxPopupState::kFull, popup_state());
  EXPECT_TRUE(WaitForInputValue("draft"));
  EXPECT_EQ(0, starts.count());
}

// With kOmniboxFullWebUIShadow, `ShouldDrawFullPopupShadowInWebUI()` stays
// false while every Full popup is hosted in the location bar, so
// `webui-shadow-enabled_` is omitted and `omnibox_popup_document_style.css`
// suppresses the outer shadow padding (while `#inputWrapper[unbounded]` in
// `omnibox_popup_searchbox.css` still draws `--omnibox-webui-shadow` when
// open).
class OmniboxPopupLocationBarHostShadowTest
    : public OmniboxPopupLocationBarHostTest {
 public:
  OmniboxPopupLocationBarHostShadowTest() {
    shadow_feature_list_.InitAndEnableFeature(omnibox::kOmniboxFullWebUIShadow);
  }

 private:
  base::test::ScopedFeatureList shadow_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostShadowTest,
                       DocumentOmitsOuterShadowPadding) {
  ASSERT_FALSE(omnibox::ShouldDrawFullPopupShadowInWebUI());
  FocusAndWaitForPopup();
  EXPECT_EQ(true, Eval(R"(
    const app = document.querySelector('omnibox-full-app');
    return !sb.hasAttribute('webui-shadow-enabled_') &&
           !app.hasAttribute('webui-shadow-enabled_');)"));
}
