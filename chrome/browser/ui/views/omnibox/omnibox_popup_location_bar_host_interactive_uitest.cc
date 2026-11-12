// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_location_bar_host.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Defines `sb` (<omnibox-popup-searchbox>), `wrapper` (#inputWrapper),
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

constexpr char kDropdownOpen[] = "sb.dropdownIsVisible";
constexpr char kDropdownClosed[] = "!sb.dropdownIsVisible";

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
        {omnibox::kOmniboxAimPopupRequireEligibility});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// The WebView is a child of (and fills) LocationBarView, paints above its
// siblings, and starts with the dropdown closed.
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
  EXPECT_EQ(true, Eval(std::string("return ") + kDropdownClosed + ";"));
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

// Typing reaches the edit model and opens the page's dropdown, while the
// WebView itself keeps filling LocationBarView.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest, TypingOpensDropdown) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("chrome://version");

  EXPECT_TRUE(edit_model()->user_input_in_progress());
  EXPECT_FALSE(controller()->autocomplete_controller()->result().empty());
  EXPECT_EQ(location_bar()->GetLocalBounds(), web_view()->bounds());
  EXPECT_EQ(web_view(), location_bar()->children().back());
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

// Escape closes the dropdown.
IN_PROC_BROWSER_TEST_F(OmniboxPopupLocationBarHostTest,
                       SearchboxEscapeClosesDropdown) {
  NavigateTo("/title1.html");
  FocusAndWaitForPopup();
  TypeAndWaitForDropdown("title");

  PressKey(ui::DomKey::ESCAPE, ui::DomCode::ESCAPE, ui::VKEY_ESCAPE);
  EXPECT_TRUE(WaitForJs(kDropdownClosed));
}

// With kOmniboxFullWebUIShadow, the location bar's page still draws no shadow,
// since it fills the location bar: `ShouldDrawFullPopupShadowInWebUI()` is
// false while every Full popup is hosted in the location bar.
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
                       PageDrawsNoShadow) {
  ASSERT_FALSE(omnibox::ShouldDrawFullPopupShadowInWebUI());
  FocusAndWaitForPopup();
  EXPECT_EQ(true, Eval(R"(
    const app = document.querySelector('omnibox-full-app');
    return !sb.hasAttribute('webui-shadow-enabled_') &&
           !app.hasAttribute('webui-shadow-enabled_');)"));
}
