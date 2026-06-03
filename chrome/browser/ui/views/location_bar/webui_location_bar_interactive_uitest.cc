// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

const WebContentsInteractionTestUtil::DeepQuery kOmniboxInputDeepQuery = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#textInput"};
const WebContentsInteractionTestUtil::DeepQuery kSearchKeywordText = {
    "toolbar-app", "location-bar", "selected-keyword", "span"};

const WebContentsInteractionTestUtil::DeepQuery kClassicMatchText0 = {
    "omnibox-popup-app", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index=\"0\"]", "#suggestion"};
const WebContentsInteractionTestUtil::DeepQuery kClassicMatchText1 = {
    "omnibox-popup-app", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index=\"1\"]", "#suggestion"};
const WebContentsInteractionTestUtil::DeepQuery kClassicMatchText2 = {
    "omnibox-popup-app", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index=\"2\"]", "#suggestion"};
const WebContentsInteractionTestUtil::DeepQuery kDropdownContent = {
    "omnibox-popup-app", "cr-searchbox-dropdown", "#content"};

class ViewWidthObserver
    : public ui::test::
          ObservationStateObserver<int, views::View, views::ViewObserver> {
 public:
  explicit ViewWidthObserver(views::View* view)
      : ObservationStateObserver<int, views::View, views::ViewObserver>(view) {}
  ~ViewWidthObserver() override = default;

  // ObservationStateObserver:
  int GetStateObserverInitialState() const override {
    return source()->width();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    OnStateObserverStateChanged(observed_view->width());
  }
  void OnViewIsDeleting(views::View*) override {
    OnObservationStateObserverSourceDestroyed();
  }
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ViewWidthObserver, kViewWidth);

}  // namespace

using TestBase = SearchboxInteractiveTestMixin<
    WebUiInteractiveTestMixin<InteractiveBrowserTest>>;

class WebUILocationBarInteractiveUiTest : public TestBase {
 public:
  WebUILocationBarInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUILocationBar},
        {});
  }
  ~WebUILocationBarInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    test_util().AddSimulator(
        std::make_unique<ui::InteractionTestUtilSimulatorWebUI>());

    // Insert an interceptor for network requests, so autocomplete doesn't
    // go off searching google.com, and to provide some things for tests
    // to use. We first must destroy the searchbox mixin's one, however.
    TearDownUrlLoaderInterceptor();
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&WebUILocationBarInteractiveUiTest::HandleRequest));

    // Wait for the toolbar to load. Note that we can't wait for the widget to
    // become visible instead because the Widget will always be visible on Mac
    // OS.
    ASSERT_TRUE(base::test::RunUntil([browser = browser()]() {
      InitialWebUIManager* manager = InitialWebUIManager::From(browser);
      return !manager || !manager->IsShowPending();
    }));
  }

  void TearDownOnMainThread() override {
    TestBase::TearDownOnMainThread();
    url_loader_interceptor_.reset();
  }

  views::WebView* GetToolbarWebView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetWebUIToolbarViewForTesting()
        ->GetWebViewForTesting();
  }

  auto GetActiveClassicPopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      WebUILocationBar* location_bar = static_cast<WebUILocationBar*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar());
      return location_bar->GetOmniboxPopupView()
          ->presenter()
          ->GetWebUIContent();
    });
  }

  auto WaitForClassicPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kClassicPopupWebViewId,
                                             GetActiveClassicPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kClassicPopupWebViewId, GURL(chrome::kChromeUIOmniboxPopupURL))));
  }

  auto RemoveFocusFromPopup() {
    return Steps(InAnyContext(MoveMouseTo(kToolbarAppMenuButtonElementId)),
                 InSameContext(ClickMouse()),
                 InAnyContext(WaitForHide(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame)));
  }

  auto FakeKeyDownAt(ui::ElementIdentifier webcontents_id,
                     const WebContentsInteractionTestUtil::DeepQuery& where,
                     std::string_view key,
                     bool shift = false,
                     bool control = false,
                     bool alt = false,
                     bool command = false) {
    const char kTemplate[] = R"(
      (el) => {
        const ev = new KeyboardEvent('keydown', {
          key: $1,
          shiftKey: $2,
          ctrlKey: $3,
          altKey: $4,
          metaKey: $5,
        });
        el.dispatchEvent(ev);
      }
    )";
    return ExecuteJsAt(
        webcontents_id, where,
        content::JsReplace(kTemplate, key, shift, control, alt, command));
  }

  auto WaitTillOmniboxViewFocus() {
    return WaitForJsResultAt(kWebUIToolbarId, kOmniboxInputDeepQuery,
                             "el => el.matches(':focus-visible')");
  }

  auto WaitTillOmniboxViewText(std::string_view expected_text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.value === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kTextOK;
    text_matches.where = kOmniboxInputDeepQuery;
    text_matches.test_function = content::JsReplace(kTemplate, expected_text);
    return WaitForStateChange(kWebUIToolbarId, text_matches);
  }

  auto WaitTillSearchKeywordText(std::string_view expected_text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kKeywordTextOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.textContent === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kKeywordTextOK;
    text_matches.where = kSearchKeywordText;
    text_matches.test_function = content::JsReplace(kTemplate, expected_text);
    return WaitForStateChange(kWebUIToolbarId, text_matches);
  }

  auto WaitTillInlineComplete(std::string_view expected_text,
                              std::string_view expected_completion) {
    // Inline completion is expected to be rendered as selection after the
    // expected text.
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInlineCompleteOK);
    const char kTemplate[] = R"(
      (el) => {
        const expectedText = $1;
        const expectedCompletion = $2;
        const combined = expectedText + expectedCompletion;
        if (el.value !== combined) {
          return false;
        }

        return el.selectionStart === expectedText.length &&
               el.selectionEnd === combined.length;
      }
    )";
    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kInlineCompleteOK;
    text_matches.where = kOmniboxInputDeepQuery;
    text_matches.test_function =
        content::JsReplace(kTemplate, expected_text, expected_completion);
    return WaitForStateChange(kWebUIToolbarId, text_matches);
  }

 private:
  static bool HandleRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.host() == "www.google.com" &&
        params->url_request.url.path() == "/complete/search") {
      constexpr std::string_view headers =
          "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";
      constexpr std::string_view body =
          R"()]}'\n["input", [
            "https://local.test/input/",
            "https://developer.mozilla.org/en-US/docs/Web/API/InputEvent"],
            [],
            [],
            {"google:suggesttype":[
                "NAVIGATION",
                "NAVIGATION",
              ],
              "google:suggestrelevance": [
                 1010,
                 1000,
              ],
            }
        ])";
      content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                   params->client.get());
    } else {
      constexpr std::string_view headers =
          "HTTP/1.1 404 Not found\nContent-Type: application/json\n\n";

      content::URLLoaderInterceptor::WriteResponse(headers, "",
                                                   params->client.get());
    }
    return true;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

// Show and hide the omnibox popup.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, ShowHidePopup) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up.
      EnterText(kOmniboxElementId, u"input"), WaitForClassicPopupReady(),
      // Removing the focus should hide the popup.
      RemoveFocusFromPopup());
}

// Test that the popup shrinks when the browser window does.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, Resize) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  int initial_width = -1;
  views::View* frame_view = nullptr;
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      WaitForElementToRender(kWebUIToolbarId, kOmniboxInputDeepQuery),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up.
      EnterText(kOmniboxElementId, u"input"), WaitForClassicPopupReady(),
      WaitForElementToRender(kClassicPopupWebViewId, kDropdownContent),
      InSameContext(WithElement(OmniboxPopupPresenterBase::kRoundedResultsFrame,
                                [&](ui::TrackedElement* element) {
                                  initial_width =
                                      element->GetScreenBounds().width();
                                })),
      InAnyContext(WithView(OmniboxPopupPresenterBase::kRoundedResultsFrame,
                            [&](views::View* view) { frame_view = view; })),
      // Start watching the width.
      ObserveState(kViewWidth, [&]() { return frame_view; }),
      // Shrink the window horizontally.
      Do([&]() {
        auto* browser_widget = browser_view->GetWidget();
        gfx::Size size = browser_widget->GetSize();
        size.set_width(size.width() - 100);
        browser_widget->SetSize(size);
      }),

      InSameContext(
          WaitForState(kViewWidth, [&]() { return initial_width - 100; })),
      StopObservingState(kViewWidth));
}

// Use arrow keys to select between various suggestions.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, NavigateSuggestions) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up.
      EnterText(kOmniboxElementId, u"input"), WaitForClassicPopupReady(),
      // Should have an entry for just searching for "input", as well as the
      // two suggestions from our interceptor. Note that the https://
      // gets dropped for pretty-printing by the popup.
      WaitForVerbatimMatch(kClassicPopupWebViewId, kClassicMatchText0, "input"),
      WaitForMatch(kClassicPopupWebViewId, kClassicMatchText1,
                   "local.test/input/"),
      WaitForMatch(kClassicPopupWebViewId, kClassicMatchText2,
                   "developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // Press keydown to select the next suggestion.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "ArrowDown"),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // And again.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "ArrowDown"),
      WaitTillOmniboxViewText(
          "https://developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // Now go up.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "ArrowUp"),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // Escape resets to the default suggestion.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "Escape"),
      WaitTillOmniboxViewText("input"),

      // Now down again to local.test.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "ArrowDown"),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // And press enter to accept.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "Enter"),

      // This should navigate to the page (or rather the 404 added by our
      // interceptor).
      WaitForWebContentsNavigation(kTabId, GURL("https://local.test/input/")),

      // Removing the focus should hide the popup.
      RemoveFocusFromPopup());
}

// Use an inline suggestion. Since we can't actually fake keyboard input, this
// is limited to things which the JS impl directly does in response to events
// and not things done as normal <input> interactions.
// TODO(crbug.com/519455538): Fix the data race and re-enable the test.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest,
                       DISABLED_InlineSuggestion) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this->browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  GURL url("https://local.test/");
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  ui_test_utils::WaitForHistoryToLoad(history_service);

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up.
      EnterText(kOmniboxElementId, u"https://local"),
      WaitForClassicPopupReady(),
      WaitTillInlineComplete("https://local", ".test"),

      // Enter . to advance completion.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "."),
      WaitTillInlineComplete("https://local.", "test"),

      // Likewise for t.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "t"),
      WaitTillInlineComplete("https://local.t", "est"),

      // Accept it.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "Enter"),
      WaitForWebContentsNavigation(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test"),

      // Removing the focus should hide the popup.
      RemoveFocusFromPopup());
}

// Use Ctrl-Alt-Enter to append www. and .com to URL and open it in new tab.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, Modifiers) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text, it should show up. We include the schema to make
      // sure we always end up with https://.
      EnterText(kOmniboxElementId, u"https://google"),
      WaitForClassicPopupReady(), WaitTillOmniboxViewText("https://google"),
      // Omnibox needs to see Ctrl pressed down, not just as modifier, to
      // append stuff around it.
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "Control"),
      InstrumentNextTab(kSecondTabId),
      FakeKeyDownAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "Enter",
                    /*shift=*/false, /*control=*/true,
                    /*alt=*/true, /*command=*/false),
      WaitForWebContentsNavigation(kSecondTabId,
                                   GURL("https://www.google.com")));
}

// Clicking the location icon should show the Page Info bubble.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, ClickLocationIcon) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(
          kWebUIToolbarId,
          {"toolbar-app", "location-bar", "location-icon", "#container"},
          "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
}

// Interact with @tabs search keyword.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, SearchAtKeyword) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      WaitTillOmniboxViewFocus(),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text.
      EnterText(kOmniboxElementId, u"@tab"), WaitForClassicPopupReady(),
      WaitTillOmniboxViewText("@tab"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_S),
      WaitTillOmniboxViewText("@tabs"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_SPACE),
      // Omnibox text should should become empty, and a keyword chip
      // should show up.
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Tabs"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_S), WaitTillOmniboxViewText("s"),
      WaitTillSearchKeywordText("Search Tabs"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_BACK), WaitTillOmniboxViewText(""),
      WaitTillSearchKeywordText("Search Tabs"),
      // Backspace with only chip present converts it back to plain text.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_BACK),
      WaitTillOmniboxViewText("@tabs "),
      EnsureNotPresent(kWebUIToolbarId, kSearchKeywordText));
}

// Interact with 'google.com' as a search keyword.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, SearchKeyword) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      WaitTillOmniboxViewFocus(),
      // Shouldn't have a popup visible yet.
      InAnyContext(
          EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Type some text.
      EnterText(kOmniboxElementId, u"google.com"), WaitForClassicPopupReady(),
      WaitTillOmniboxViewText("google.com"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_SPACE),
      // Omnibox text should should become empty, and a keyword chip
      // should show up.
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Google"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_S), WaitTillOmniboxViewText("s"),
      WaitTillSearchKeywordText("Search Google"),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_BACK), WaitTillOmniboxViewText(""),
      WaitTillSearchKeywordText("Search Google"),
      // Backspace with only chip present converts it back to plain text.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_BACK),
      WaitTillOmniboxViewText("google.com "),
      EnsureNotPresent(kWebUIToolbarId, kSearchKeywordText));
}
