// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
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
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider_test_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/screen.h"
#include "ui/gfx/range/range.h"
#include "ui/views/mouse_constants.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

const WebContentsInteractionTestUtil::DeepQuery kOmniboxInputDeepQuery = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#textInput"};
const WebContentsInteractionTestUtil::DeepQuery kOmniboxAdditionalText = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#additionalText"};
const WebContentsInteractionTestUtil::DeepQuery kSearchKeywordText = {
    "toolbar-app", "location-bar", "selected-keyword", "#long"};

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

class NotifyWhenShortcutsLoadedObserver
    : public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  explicit NotifyWhenShortcutsLoadedObserver(base::OnceClosure on_loaded)
      : on_loaded_(std::move(on_loaded)) {}

  void OnShortcutsLoaded() override { std::move(on_loaded_).Run(); }

 private:
  base::OnceClosure on_loaded_;
};

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

  auto WaitTillAdditionalText(std::string_view expected_text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAdditionalTextOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.textContent === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kAdditionalTextOK;
    text_matches.where = kOmniboxAdditionalText;
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

  auto WaitTillOmniboxViewSelection(std::string_view expected_selected,
                                    gfx::Range expected_selection,
                                    bool expect_no_dir = false) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kSelectionOK);
    const char kTemplate[] = R"(
      (el) => {
        const expectedSelection = $1;
        const min = $2;
        const max = $3;
        let dir = $4 ? 'backward' : 'forward';
        if ($5) {
          dir = 'none';
        }
        if (el.selectionStart !== min ||
            el.selectionEnd !== max) {
          return false;
        }

        if (el.selectionDirection === dir) {
          return true;
        }

        // Mac likes to default selections to none. Handle that implicitly
        // for caret, since almost every other selection we set ourselves,
        // with explicit direction.
        if (min === max) {
           return el.selectionDirection === 'none';
        }
        return false;
      }
    )";
    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kSelectionOK;
    text_matches.where = kOmniboxInputDeepQuery;
    text_matches.test_function =
        content::JsReplace(kTemplate, expected_selected,
                           static_cast<double>(expected_selection.GetMin()),
                           static_cast<double>(expected_selection.GetMax()),
                           expected_selection.is_reversed(), expect_no_dir);
    return WaitForStateChange(kWebUIToolbarId, text_matches);
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
    step.SetDescription("DoWaitForTime()");
    return step;
  }

  // We synthesize double-clicks by using the content API to inject an event.
  // Using Kombucha ClickMouse(); ClickMouse() turns into two single-clicks in
  // slow environments (e.g. ASAN bots) and doesn't work on Mac.
  //
  // Injecting via JS would not get default behavior from blink.
  auto SynthesizeDoubleClickInToolbarWebUI() {
    return Do([&]() {
      views::WebView* web_view = GetToolbarWebView();
      content::RenderWidgetHost* widget =
          web_view->GetWebContents()->GetRenderViewHost()->GetWidget();

      gfx::Point point = display::Screen::Get()->GetCursorScreenPoint();

      for (int click = 1; click <= 2; ++click) {
        blink::WebMouseEvent mouse_event(
            blink::WebInputEvent::Type::kMouseDown,
            blink::WebInputEvent::kNoModifiers,
            blink::WebInputEvent::GetStaticTimeStampForTests());
        mouse_event.button = blink::WebMouseEvent::Button::kLeft;
        mouse_event.click_count = click;
        mouse_event.SetPositionInWidget(
            point.x() - web_view->GetBoundsInScreen().x(),
            point.y() - web_view->GetBoundsInScreen().y());
        mouse_event.SetPositionInScreen(point.x(), point.y());
        widget->ForwardMouseEvent(mouse_event);
        mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
        widget->ForwardMouseEvent(mouse_event);
      }
    });
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

IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, AdditionalText) {
  auto shortcuts_backend =
      ShortcutsBackendFactory::GetForProfile(browser()->profile());
  if (!shortcuts_backend->initialized()) {
    base::RunLoop run_loop;
    NotifyWhenShortcutsLoadedObserver notify_init(run_loop.QuitClosure());
    shortcuts_backend->AddObserver(&notify_init);
    run_loop.Run();
    shortcuts_backend->RemoveObserver(&notify_init);
  }

  std::array<TestShortcutData, 1> test_shortcut = {
      // Thanks, shortcuts_provider_unittest.cc
      {{"BD85DBA2-8C29-49F9-84AE-48E1E12345E0", "news weather",
        "www.cnn.com/index.html", "http://www.cnn.com/index.html",
        AutocompleteMatch::DocumentType::NONE, "www.cnn.com/index.html", "0,1",
        "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
        "0,0,19,2,23,0,38,2,45,0", ui::PAGE_TRANSITION_TYPED,
        AutocompleteMatchType::HISTORY_TITLE, "", 1, 10}}};
  PopulateShortcutsBackendWithTestData(shortcuts_backend, test_shortcut);

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      WaitTillOmniboxViewFocus(), EnterText(kOmniboxElementId, u"ne"),
      WaitForClassicPopupReady(), SendKeyPress(kWebUIToolbarId, ui::VKEY_W),
      WaitTillInlineComplete("new", "s weather"),
      WaitTillAdditionalText(" - www.cnn.com/index.html"));
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

// Tests that click-focusing the omnibox selects all (and accidentally
// default focus behavior for about:blank pages).
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, ClickSelectsAll) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      // The browser will focus the location bar automatically since it's
      // about-blank; and since it didn't have focus before, it should
      // select-all.
      WaitTillOmniboxViewFocus(),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Clear selection.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Transfer the focus to contents.
      FocusWebContents(kTabId),
      // Now click the omnibox; the contents should get selected again.
      MoveMouseTo(kOmniboxElementId), ClickMouse(), WaitTillOmniboxViewFocus(),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)));
}

// Click when already focused doesn't select all.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest,
                       SecondClickDoesNotSelectAll) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      // The browser will focus the location bar automatically since it's
      // about-blank; and since it didn't have focus before, it should
      // select-all.
      WaitTillOmniboxViewFocus(),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Clear selection.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewSelection("", gfx::Range()),
      // Now click the omnibox; should not select-all; and since it
      // clicked in the middle and the URL is pretty short , the caret should be
      // at the end.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      WaitTillOmniboxViewSelection("", gfx::Range(11)));
}

// Test that pressing home triggers unelision.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, UnelideHome) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusWebContents(kTabId),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test"),
      // Click to focus location bar.
      MoveMouseTo(kOmniboxElementId), ClickMouse(), WaitTillOmniboxViewFocus(),
      // Selected, but not unelided yet.
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("local.test", gfx::Range(10, 0)),
      // Press home. This should trigger unelision.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_HOME),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(0)));
}

// Tests that if initial interaction just selected-all and didn't unelide
// that moving the caret will unelide.
// TODO(cebug.com/468203351): Flaky on Linux debug builders.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_UnelideCaretMove DISABLED_UnelideCaretMove
#else
#define MAYBE_UnelideCaretMove UnelideCaretMove
#endif
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest,
                       MAYBE_UnelideCaretMove) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusWebContents(kTabId),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      // Click to focus location bar.
      MoveMouseTo(kOmniboxElementId), ClickMouse(), WaitTillOmniboxViewFocus(),
      // Selected, but not unelided yet.
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("local.test", gfx::Range(10, 0)),
      // Press left-arrow. This should trigger unelision (and put
      // the caret after the scheme).
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(8)));
}

// Test of Ctrl-K focus omnibox + activates default search shortcut.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, FocusSearch) {
  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_SEARCH, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      SendAccelerator(kBrowserViewElementId, accelerator),
      // Since the user didn't change text, it should be cleared.
      WaitTillOmniboxViewFocus(), WaitTillOmniboxViewText(""),
      WaitTillSearchKeywordText("Search Google"),
      // Enter a character.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_S), WaitTillOmniboxViewText("s"),
      // Pressing the accel again should select-all
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"),
      // Transfer the focus to contents. Search keyword should still be active.
      FocusWebContents(kTabId),
      // Wait a bit to get things a chance to screw up if we're doing the wrong
      // thing here.
      DoWaitForTime(base::Milliseconds(100)), WaitTillOmniboxViewText("s"),
      WaitTillSearchKeywordText("Search Google"));
}

// Test of Ctrl-K focus omnibox when the user has edited the text.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, FocusSearch2) {
  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_SEARCH, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      // Since it's about:blank we should have focus.
      WaitTillOmniboxViewFocus(),
      // Enter a character.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_S), WaitTillOmniboxViewText("s"),
      // Ctrl-K with custom input should preserve it (and select it).
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"),
      // Cancel selection.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Ctrl-K again will reapply the select-all.
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"));
}

// Test of Ctrl-L (and others) focus location bar.
// TODO(crbug.com/532463469): Flaky on Linux debug builders.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_FocusLocation DISABLED_FocusLocation
#else
#define MAYBE_FocusLocation FocusLocation
#endif
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, MAYBE_FocusLocation) {
  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_LOCATION, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusWebContents(kTabId),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      // Press Ctrl-L; it should focus, unelide, and select-all. Also should
      // not add a search chip, since that's a separate accel.
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewFocus(), WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("https://local.test", gfx::Range(18, 0)),
      EnsureNotPresent(kWebUIToolbarId, kSearchKeywordText),
      // Clear selection.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Ctrl-L again should reapply the select-all.
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("https://local.test", gfx::Range(18, 0)),
      EnsureNotPresent(kWebUIToolbarId, kSearchKeywordText));
}

IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, TypeWithMouseDown) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Clear selection, since it would mask the issue, and transfer focus
      // to page.
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewSelection("", gfx::Range(0)), FocusWebContents(kTabId),
      InAnyContext(MoveMouseTo(kOmniboxElementId)),
      InSameContext(ClickMouse(ui_controls::LEFT, /*release=*/false)),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_W), WaitTillOmniboxViewText("w"),
      InSameContext(ReleaseMouse()), SendKeyPress(kWebUIToolbarId, ui::VKEY_W),
      // Should have two ww's, not one.
      WaitTillOmniboxViewText("ww"));
}

// Test of selecting a word portion of URL with double-click select.
// This is just a regular double-click. That it's the first word is
// relevant, since we also need to make sure the selection isn't extended to
// encompass https:// unlike what it would do otherwise.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, DoubleClick) {
#if BUILDFLAG(IS_MAC)
  // Mac likes to make selections non-directional by default, and this test
  // has it setting one rather than our code.
  const bool expect_no_dir = true;
#else
  const bool expect_no_dir = false;
#endif

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusWebContents(kTabId),
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(10)),
      InAnyContext(MoveMouseTo(
          kOmniboxElementId,
          base::BindOnce(
              [](ui::TrackedElement* reference_element) -> gfx::Point {
                // Return somewhere in the first word --- a bit to the
                // right of left-center.
                return reference_element->GetScreenBounds().left_center() +
                       gfx::Vector2d(10, 0);
              }))),
      SynthesizeDoubleClickInToolbarWebUI(),
      // The URL is unelided, and "local" is selected.
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("local", gfx::Range(8, 13), expect_no_dir));
}

// Test of selecting a word portion of URL with double-click select.
// This arranges for unelision to have happened on first click and not second;
// and selects the last word.
IN_PROC_BROWSER_TEST_F(WebUILocationBarInteractiveUiTest, DoubleClick2) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusWebContents(kTabId),
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(10)),
      // Focus location bar. This is important since if it's already focused
      // it won't try to select-all on first click. Also we do it with
      // JS and not Ctrl-L since that would unelide.
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      WaitTillOmniboxViewFocus(),
      // There is a caveat to the above, however --- the JS implementation
      // uses time to figure out that the click isn't what caused the focus
      // change, since there doesn't seem to be a reliable way of telling.
      DoWaitForTime(views::kMinimumTimeBetweenButtonClicks * 1.1),
      InAnyContext(MoveMouseTo(
          kOmniboxElementId,
          base::BindOnce(
              [](ui::TrackedElement* reference_element) -> gfx::Point {
                // Return a bit to the left of right-center; double-click
                // there will select the last word.
                return reference_element->GetScreenBounds().right_center() -
                       gfx::Vector2d(10, 0);
              }))),
      SynthesizeDoubleClickInToolbarWebUI(),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("test", gfx::Range(14, 18)));
}
