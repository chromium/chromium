// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
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
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/range/range.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/test/widget_test.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAimPopupWebViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIToolbarId);

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kPopupShort);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingElementStateObserver<bool>,
                                    kPopupTall);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kAIMWebContentsVisible);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kAIMWebContentsHidden);

const WebContentsInteractionTestUtil::DeepQuery kOmniboxInputDeepQuery = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#textInput", "#input"};
const WebContentsInteractionTestUtil::DeepQuery kFullPopupInputDeepQuery = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-input",
    "#input"};
const WebContentsInteractionTestUtil::DeepQuery kOmniboxAdditionalText = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#additionalText"};
const WebContentsInteractionTestUtil::DeepQuery kOmniboxInlineAutocomplete = {
    "toolbar-app", "location-bar", "readonly-omnibox", "#inlineAutocomplete"};
const WebContentsInteractionTestUtil::DeepQuery kSearchKeywordText = {
    "toolbar-app", "location-bar", "selected-keyword", "#long"};
const WebContentsInteractionTestUtil::DeepQuery kFullPopupSearchKeywordText = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-input",
    "#keyword"};
const WebContentsInteractionTestUtil::DeepQuery kAIMButtonOuter = {
    "toolbar-app", "location-bar", "page-action-icons", "page-action-icon"};
const WebContentsInteractionTestUtil::DeepQuery kAIMButton = {
    "toolbar-app", "location-bar", "page-action-icons", "page-action-icon",
    "toolbar-chip-button"};
const WebContentsInteractionTestUtil::DeepQuery kFullPopupAIMButton = {
    "omnibox-full-app", "omnibox-popup-searchbox",
    "cr-searchbox-compose-button", "cr-button"};

// This marks tests where additional work on an implementation is a prerequisite
// for test passing.
#define FAILS_IN_MODE(broken_mode, comment) \
  do {                                      \
    if (mode() == broken_mode) {            \
      GTEST_SKIP() << comment;              \
    }                                       \
  } while (0)

// This marks tests where the test needs work for given implementation.
#define PORT_UNFINISHED(broken_mode, comment) \
  FAILS_IN_MODE(broken_mode, comment)

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

class ClipboardTextObserver
    : public ui::test::ObservationStateObserver<std::u16string,
                                                ui::ClipboardMonitor,
                                                ui::ClipboardObserver> {
 public:
  explicit ClipboardTextObserver(ui::ClipboardMonitor* clipboard_monitor)
      : ObservationStateObserver<std::u16string,
                                 ui::ClipboardMonitor,
                                 ui::ClipboardObserver>(clipboard_monitor) {
    PollClipboard();
  }
  ~ClipboardTextObserver() override = default;

  // ObservationStateObserver:
  std::u16string GetStateObserverInitialState() const override {
    return std::u16string();
  }

  // ClipboardObserver:
  void OnClipboardDataChanged() override { PollClipboard(); }

 private:
  void PollClipboard() {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste,
                        /*data_dst=*/std::nullopt,
                        base::BindOnce(&ClipboardTextObserver::GotClipboard,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void GotClipboard(std::u16string result) {
    OnStateObserverStateChanged(std::move(result));
  }

  base::WeakPtrFactory<ClipboardTextObserver> weak_ptr_factory_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ClipboardTextObserver, kClipboardText);

class NotifyWhenShortcutsLoadedObserver
    : public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  explicit NotifyWhenShortcutsLoadedObserver(base::OnceClosure on_loaded)
      : on_loaded_(std::move(on_loaded)) {}

  void OnShortcutsLoaded() override { std::move(on_loaded_).Run(); }

 private:
  base::OnceClosure on_loaded_;
};

// A pretend IME that composes some text in response to 'Z' and 'X' key presses.
class PretendComposeInputMethod : public ui::MockInputMethod {
 public:
  PretendComposeInputMethod()
      : ui::MockInputMethod(/*ime_key_event_dispatcher=*/nullptr) {}

  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override {
    auto* text_input_client = GetTextInputClient();
    if (event->type() == ui::EventType::kKeyPressed &&
        event->GetDomKey() == ui::DomKey::FromCharacter('z')) {
      ui::CompositionText ct;
      ct.text = u"loc";
      ct.ime_text_spans = {
          ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0u, 3u)};
      ct.selection = gfx::Range(3);
      text_input_client->SetCompositionText(ct);
      return ui::EventDispatchDetails();
    }
    if (event->type() == ui::EventType::kKeyPressed &&
        event->GetDomKey() == ui::DomKey::FromCharacter('x')) {
      ui::CompositionText ct;
      ct.text = u"local.t";
      ct.ime_text_spans = {
          ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0u, 7u)};
      ct.selection = gfx::Range(7);
      text_input_client->SetCompositionText(ct);
      return ui::EventDispatchDetails();
    }
    return ui::MockInputMethod::DispatchKeyEvent(event);
  }

  base::WeakPtr<PretendComposeInputMethod> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<PretendComposeInputMethod> weak_ptr_factory_{this};
};

enum class Mode { kCutout, kFull };

enum class View {
  kStatic,
  kEditable,
};

std::string ModeToString(Mode mode) {
  switch (mode) {
    case Mode::kCutout:
      return "Cutout";
    case Mode::kFull:
      return "Full";
  }
}

}  // namespace

using TestBase = SearchboxInteractiveTestMixin<
    WebUiInteractiveTestMixin<InteractiveBrowserTest>>;

class WebUILocationBarInteractiveUiTest
    : public TestBase,
      public testing::WithParamInterface<Mode> {
 public:
  WebUILocationBarInteractiveUiTest() {
    // TODO(crbug.com/539786691): Re-enable kPrewarm once the feature is
    // compatible with the test.
    if (mode() == Mode::kCutout) {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton,
           features::kWebUILocationBar,
           omnibox::internal::kWebUIOmniboxAimPopup},
          {omnibox::kAimServerEligibilityEnabled, features::kPrewarm});
    } else {
      feature_list_.InitWithFeatures(
          {features::kInitialWebUI, features::kWebUIReloadButton,
           features::kWebUILocationBar, omnibox::kWebUIOmniboxFullPopup,
           omnibox::internal::kWebUIOmniboxAimPopup},
          {omnibox::kAimServerEligibilityEnabled, features::kPrewarm});
    }
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
      return !manager || !manager->IsInitialWebUIPending();
    }));
  }

  void TearDownOnMainThread() override {
    TestBase::TearDownOnMainThread();
    url_loader_interceptor_.reset();
  }

  Mode mode() { return GetParam(); }

  ui::ElementIdentifier PopupWebContents() { return kClassicPopupWebViewId; }

  ui::ElementIdentifier InputWebContents() {
    if (mode() == Mode::kFull) {
      return kClassicPopupWebViewId;
    } else {
      return kWebUIToolbarId;
    }
  }

  const WebContentsInteractionTestUtil::DeepQuery& AIMButton() {
    if (mode() == Mode::kFull) {
      return kFullPopupAIMButton;
    } else {
      return kAIMButton;
    }
  }

  WebContentsInteractionTestUtil::DeepQuery DropdownContent() {
    if (mode() == Mode::kFull) {
      return WebContentsInteractionTestUtil::DeepQuery(
          {"omnibox-full-app", "omnibox-popup-searchbox",
           "cr-searchbox-dropdown", "#content"});
    } else {
      return WebContentsInteractionTestUtil::DeepQuery(
          {"omnibox-popup-app", "cr-searchbox-dropdown", "#content"});
    }
  }

  WebContentsInteractionTestUtil::DeepQuery MatchText(int num) {
    std::string match =
        base::StringPrintf("cr-searchbox-match[match-index=\"%d\"]", num);
    if (mode() == Mode::kFull) {
      return WebContentsInteractionTestUtil::DeepQuery(
          {"omnibox-full-app", "omnibox-popup-searchbox",
           "cr-searchbox-dropdown", match, "#suggestion"});
    } else {
      return WebContentsInteractionTestUtil::DeepQuery(
          {"omnibox-popup-app", "cr-searchbox-dropdown", match, "#suggestion"});
    }
  }

  WebContentsInteractionTestUtil::DeepQuery Input() {
    if (mode() == Mode::kFull) {
      return kFullPopupInputDeepQuery;
    } else {
      return kOmniboxInputDeepQuery;
    }
  }

  WebContentsInteractionTestUtil::DeepQuery SearchKeyword() {
    if (mode() == Mode::kFull) {
      return kFullPopupSearchKeywordText;
    } else {
      return kSearchKeywordText;
    }
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

  auto GetActiveAimPopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      WebUILocationBar* location_bar = static_cast<WebUILocationBar*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar());
      auto* aim_presenter = static_cast<OmniboxPopupAimPresenter*>(
          location_bar->GetOmniboxPopupAimPresenter());
      return aim_presenter->GetWebUIContent();
    });
  }

  auto WaitForAimPopupReady() {
    return Steps(
        // Wait till the view is accessible as a tracked element; waiting for
        // the frame isn't sufficient w/full popup since two popups using
        // the same rounded frame can be visible at once.
        PollState(
            kAIMWebContentsVisible,
            [this]() {
              auto* view = GetActiveAimPopupWebView().Run();
              auto* element =
                  views::ElementTrackerViews::GetInstance()->GetElementForView(
                      view, /* assign_temporary_id =*/true);
              return element != nullptr;
            }),
        WaitForState(kAIMWebContentsVisible, true),
        StopObservingState(kAIMWebContentsVisible),
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kAimPopupWebViewId,
                                             GetActiveAimPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kAimPopupWebViewId, GURL(chrome::kChromeUIOmniboxPopupAimURL))));
  }

  auto WaitForAimPopupHide() {
    if (mode() == Mode::kFull) {
      return Steps(
          // Wait for the WebContents view element to be hidden.
          PollState(kAIMWebContentsHidden,
                    [this]() {
                      auto* view = GetActiveAimPopupWebView().Run();
                      auto* element =
                          views::ElementTrackerViews::GetInstance()
                              ->GetElementForView(
                                  view, /* assign_temporary_id =*/true);
                      return element == nullptr;
                    }),
          WaitForState(kAIMWebContentsHidden, true),
          StopObservingState(kAIMWebContentsHidden));
    } else {
      return InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame));
    }
  }

  auto WaitForPopupHide() {
    if (mode() == Mode::kFull) {
      return Steps(InAnyContext(
          PollState(
              kPopupShort,
              []() {
                const ui::TrackedElement* el =
                    ui::ElementTracker::GetElementTracker()
                        ->GetElementInAnyContext(
                            OmniboxPopupPresenterBase::kRoundedResultsFrame);
                if (!el) {
                  return true;
                }

                auto size = el->GetScreenBounds();
                return size.height() <= 100;
              }),
          WaitForState(kPopupShort, true), StopObservingState(kPopupShort)));
    } else {
      return InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame));
    }
  }

  auto WaitForPopupShow() {
    if (mode() == Mode::kFull) {
      return Steps(InAnyContext(
          PollElement(kPopupTall,
                      OmniboxPopupPresenterBase::kRoundedResultsFrame,
                      [](const ui::TrackedElement* el) {
                        auto size = el->GetScreenBounds();
                        return size.height() > 100;
                      }),
          WaitForState(kPopupTall, true), StopObservingState(kPopupTall)));
    } else {
      return WaitForClassicPopupReady();
    }
  }

  auto RemoveFocusFromPopup() {
    return Steps(InAnyContext(MoveMouseTo(kToolbarAppMenuButtonElementId)),
                 InSameContext(ClickMouse()), WaitForPopupHide());
  }

  // If using a cutout popup, make sure it's hidden when expected.
  auto EnsureNoPopup() {
    // With full mode, the view can exist even if there is no popup,
    // since it can be pretending to be the location bar.
    if (mode() == Mode::kFull) {
      return Steps();
    }
    return InAnyContext(
        EnsureNotPresent(OmniboxPopupPresenterBase::kRoundedResultsFrame));
  }

  auto FakeKeyDown(std::string_view key,
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
          bubbles: true,
          composed: true,
        });
        el.dispatchEvent(ev);
      }
    )";
    ui::ElementIdentifier webcontents_id = InputWebContents();
    const WebContentsInteractionTestUtil::DeepQuery& where = Input();
    return ExecuteJsAt(
               webcontents_id, where,
               content::JsReplace(kTemplate, key, shift, control, alt, command))
        .SetMustRemainVisible(false);
  }

  auto SendEnter() {
    if (mode() == Mode::kFull) {
      // Sending enter to full tends to make Kombucha upset since that closes
      // the window, so fake it via JS.
      // return FakeKeyDown("Enter");
      auto kp = SendKeyPress(InputWebContents(), ui::VKEY_RETURN);
      kp.SetMustRemainVisible(false);
      kp.SetContext(ui::InteractionSequence::ContextMode::kAny);
      return kp;
    } else {
      return SendKeyPress(InputWebContents(), ui::VKEY_RETURN);
    }
  }

  auto EnterText(const std::u16string& text) {
    if (mode() == Mode::kFull) {
      const char kEnterInput[] = R"(
        (el) => {
          el.value = $1;
          el.dispatchEvent(new InputEvent('input'));
        }
      )";
      return ExecuteJsAt(kClassicPopupWebViewId, kFullPopupInputDeepQuery,
                         content::JsReplace(kEnterInput, text));

    } else {
      return InteractiveTestApi::EnterText(kOmniboxElementId, text);
    }
  }

  auto WaitTillOmniboxViewFocus(View view = View::kEditable) {
    auto webview = kWebUIToolbarId;
    auto query = kOmniboxInputDeepQuery;
    if (mode() == Mode::kFull && view == View::kEditable) {
      webview = kClassicPopupWebViewId;
      query = kFullPopupInputDeepQuery;
    }
    return WaitForJsResultAt(webview, query,
                             "(el) => { "
                             "  return el.matches(':focus-visible');"
                             "}");
  }

  auto FocusOmnibox() {
    if (mode() == Mode::kFull) {
      return Steps(MoveMouseTo(kOmniboxElementId), ClickMouse(),
                   WaitForClassicPopupReady(), WaitTillOmniboxViewFocus());
    } else {
      return Steps(FocusWebContents(kWebUIToolbarId),
                   ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery,
                               "el => el.focus()"));
    }
  }

  auto HandleAutofocus() {
    if (mode() == Mode::kFull) {
      // Make sure we attach to the popup like FocusOmnibox() would, and
      // workaround startup races in focus.
      return Steps(WaitForClassicPopupReady(),
                   FocusWebContents(kClassicPopupWebViewId));
    } else {
      return Steps();
    }
  }

  auto FocusTab() {
    if (mode() == Mode::kFull) {
      // It seems like Full doesn't notice when it loses focus
      // due to FocusWebContents(), so do it via a click.
      return Steps(MoveMouseTo(kTabId), ClickMouse());
    } else {
      return Steps(FocusWebContents(kTabId));
    }
  }

  auto WaitTillOmniboxViewText(std::string_view expected_text,
                               View view = View::kEditable) {
    auto webview = kWebUIToolbarId;
    auto query = kOmniboxInputDeepQuery;
    if (mode() == Mode::kFull && view == View::kEditable) {
      webview = kClassicPopupWebViewId;
      query = kFullPopupInputDeepQuery;
    }

    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.value === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kTextOK;
    text_matches.where = query;
    text_matches.test_function = content::JsReplace(kTemplate, expected_text);
    return WaitForStateChange(webview, text_matches);
  }

  auto WaitTillOmniboxViewPlaceholder(std::u16string_view expected_text) {
    auto webview = kWebUIToolbarId;
    auto query = kOmniboxInputDeepQuery;
    if (mode() == Mode::kFull) {
      webview = kClassicPopupWebViewId;
      query = kFullPopupInputDeepQuery;
    }
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPlaceholderOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.placeholder === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kPlaceholderOK;
    text_matches.where = query;
    text_matches.test_function = content::JsReplace(kTemplate, expected_text);
    return WaitForStateChange(webview, text_matches);
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
    text_matches.where = SearchKeyword();
    text_matches.test_function = content::JsReplace(kTemplate, expected_text);
    return WaitForStateChange(InputWebContents(), text_matches);
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

  // This checks for inline completion rendered as selection.
  // This is available if not IME-composing.
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
    text_matches.where = Input();
    text_matches.test_function =
        content::JsReplace(kTemplate, expected_text, expected_completion);
    return WaitForStateChange(InputWebContents(), text_matches);
  }

  // This checks for inline completion in a separate widget.
  // This is always available, but only visible when IME-composing.
  auto WaitTillStandaloneInlineComplete(std::string_view expected_completion) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStandaloneInlineCompleteOK);
    const char kTemplate[] = R"(
      (el) => {
        return el.textContent === $1;
      }
    )";

    WebContentsInteractionTestUtil::StateChange text_matches;
    text_matches.event = kStandaloneInlineCompleteOK;
    text_matches.where = kOmniboxInlineAutocomplete;
    text_matches.test_function =
        content::JsReplace(kTemplate, expected_completion);
    return WaitForStateChange(kWebUIToolbarId, text_matches);
  }

  auto WaitTillOmniboxViewSelection(std::string_view expected_selected,
                                    gfx::Range expected_selection,
                                    View view = View::kEditable,
                                    bool expect_no_dir = false) {
    auto webview = kWebUIToolbarId;
    auto query = kOmniboxInputDeepQuery;
    if (mode() == Mode::kFull && view == View::kEditable) {
      webview = kClassicPopupWebViewId;
      query = kFullPopupInputDeepQuery;
    }
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
        const ignoreDir = $6;
        if (el.selectionStart !== min ||
            el.selectionEnd !== max) {
          return false;
        }

        if (el.value.substring(min, max) !== expectedSelection) {
          return false;
        }

        // We don't check the direction for full popup.
        if (ignoreDir) {
          return true;
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
    text_matches.where = query;
    text_matches.test_function = content::JsReplace(
        kTemplate, expected_selected,
        static_cast<double>(expected_selection.GetMin()),
        static_cast<double>(expected_selection.GetMax()),
        expected_selection.is_reversed(), expect_no_dir, mode() == Mode::kFull);
    return WaitForStateChange(webview, text_matches);
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

  auto GrabPopupWidthAndView() {
    return Steps(InAnyContext(WithElement(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame,
                     [this](ui::TrackedElement* element) {
                       popup_initial_width_ =
                           element->GetScreenBounds().width();
                     })),
                 InSameContext(WithView(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame,
                     [this](views::View* view) { popup_frame_ = view; })));
  }

  auto StartWatchingPopupWidth() {
    return Steps(InAnyContext(
        ObserveState(kViewWidth, [this]() { return popup_frame_; })));
  }

  auto WaitTillPopupShrunkBy100() {
    return Steps(InAnyContext(WaitForState(
        kViewWidth, [this]() { return this->popup_initial_width_ - 100; })));
  }

  auto StopWatchingPopupWidth() {
    return Steps(StopObservingState(kViewWidth),
                 Do([this]() { this->popup_frame_ = nullptr; }));
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

  int popup_initial_width_ = -1;
  raw_ptr<views::View> popup_frame_ = nullptr;
};

class WebUILocationBarIMEInteractiveUiTest
    : public WebUILocationBarInteractiveUiTest {
 public:
  WebUILocationBarIMEInteractiveUiTest() {
    auto* ime = new PretendComposeInputMethod();
    ime_ = ime->GetWeakPtr();
    ui::SetUpInputMethodForTesting(ime);
  }

  void SetUpOnMainThread() override {
    WebUILocationBarInteractiveUiTest::SetUpOnMainThread();

    // We need to set the proper IME dispatcher on the input method we created
    // since we didn't have it yet when allocating it.
    views::Widget* widget = GetToolbarWebView()->GetWidget();
    widget->GetInputMethod()->SetImeKeyEventDispatcher(
        views::test::WidgetTest::GetImeKeyEventDispatcherForWidget(widget));
  }

  void TearDownOnMainThread() override {
    if (ime_) {
      // Make sure there aren't dangling pointers on CrOS.
      ime_->SetImeKeyEventDispatcher(nullptr);
    }
    WebUILocationBarInteractiveUiTest::TearDownOnMainThread();
  }

 private:
  base::WeakPtr<PretendComposeInputMethod> ime_;
};

// Show and hide the omnibox popup.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, ShowHidePopup) {
  RunTestSequence(InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
                  InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
                  EnsureNoPopup(), FocusOmnibox(),
                  // Shouldn't have a popup visible yet.
                  EnsureNoPopup(),
                  // Type some text, it should show up.
                  EnterText(u"input"), WaitForPopupShow(),
                  // Removing the focus should hide the popup.
                  RemoveFocusFromPopup());
}

// Show and hide the omnibox AI mode popup.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, ShowHideAIPopup) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text, it should show up.
      EnterText(u"i"), WaitForPopupShow(), WaitTillOmniboxViewText("i"),
      // Clear it.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
      WaitTillOmniboxViewText(""),
      // Since text is empty, we should be able to see the AI mode button.
      WaitForJsResultAt(
          InputWebContents(), AIMButton(),
          "el => (el.tooltip || el.title) === 'Ask AI Mode in Google Search'"),
      // Click it.
      ClickElement(InputWebContents(), AIMButton()),
      // Should hide classic popup, show AIM one.
      WaitForPopupHide(), WaitForAimPopupReady(),
      // Press Esc to close it.
      InAnyContext(SendKeyPress(kAimPopupWebViewId, ui::VKEY_ESCAPE)
                       .SetMustRemainVisible(false)),
      WaitForAimPopupHide());
}

// Test tabbing over to the AIM button, make sure typing when it's active still
// works and that activating it triggers an AI-mode search and not a regular
// one.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, TabAIButton) {
  FAILS_IN_MODE(Mode::kFull,
                "Full popup doesn't appear to do AIM button fake focus."
                "See crbug.com/546705809");

  const char kCheckForceFocusRing[] =
      "(el) => "
      "el.shadowRoot.querySelector('#button').hasAttribute('force-focus-ring')";

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(), WaitTillOmniboxViewText("about:blank"),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(), EnterText(u"inp"), WaitForPopupShow(),
      WaitTillOmniboxViewText("inp"),
      // Press tab to "focus" AIM button.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_TAB)),
      WaitForJsResultAt(kWebUIToolbarId, kAIMButtonOuter, kCheckForceFocusRing),
      // Typing still goes to the input (and that clears the fake focus)
      SendKeyPress(InputWebContents(), ui::VKEY_U),
      WaitForJsResultAt(kWebUIToolbarId, kAIMButtonOuter, kCheckForceFocusRing,
                        false),
      SendKeyPress(InputWebContents(), ui::VKEY_T),
      WaitTillOmniboxViewText("input"),
      // Focus the AIM button again.
      SendKeyPress(InputWebContents(), ui::VKEY_TAB),
      WaitForJsResultAt(kWebUIToolbarId, kAIMButtonOuter, kCheckForceFocusRing),

      // Enter on the button, should trigger an AI-mode search.
      SendKeyPress(InputWebContents(), ui::VKEY_RETURN),
      WaitForWebContentsNavigation(kTabId));

  GURL url = browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL();
  // AI mode search should have an aep parameter;
  EXPECT_THAT(url.GetQuery(), testing::HasSubstr("aep="));
}

// Test that the popup shrinks when the browser window does.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, Resize) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  RunTestSequence(InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
                  InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
                  EnsureNoPopup(), FocusOmnibox(),
                  // Shouldn't have a popup visible yet.
                  EnsureNoPopup(),
                  // Type some text, it should show up.
                  EnterText(u"input"), WaitForPopupShow(),
                  WaitForElementToRender(PopupWebContents(), DropdownContent()),
                  GrabPopupWidthAndView(),
                  // Start watching the width.
                  StartWatchingPopupWidth(),
                  // Shrink the window horizontally.
                  Do([&]() {
                    auto* browser_widget = browser_view->GetWidget();
                    gfx::Size size = browser_widget->GetSize();
                    size.set_width(size.width() - 100);
                    browser_widget->SetSize(size);
                  }),

                  WaitTillPopupShrunkBy100(), StopWatchingPopupWidth());
}

// Use arrow keys to select between various suggestions.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, NavigateSuggestions) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text, it should show up.
      EnterText(u"input"), WaitForPopupShow(),
      // Should have an entry for just searching for "input", as well as the
      // two suggestions from our interceptor. Note that the https://
      // gets dropped for pretty-printing by the popup.
      WaitForVerbatimMatch(PopupWebContents(), MatchText(0), "input"),
      WaitForMatch(PopupWebContents(), MatchText(1), "local.test/input/"),
      WaitForMatch(PopupWebContents(), MatchText(2),
                   "developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // Press keydown to select the next suggestion.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_DOWN)),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // And again.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_DOWN)),
      WaitTillOmniboxViewText(
          "https://developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // Now go up.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_UP)),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // Escape resets to the default suggestion.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_ESCAPE)),
      WaitTillOmniboxViewText("input"),

      // Now down again to local.test.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_DOWN)),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // PageDown to MDN one.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_NEXT)),
      WaitTillOmniboxViewText(
          "https://developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // PageUp to default suggestion.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_PRIOR)),
      WaitTillOmniboxViewText("input"),

      // PageDown to MDN again.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_NEXT)),
      WaitTillOmniboxViewText(
          "https://developer.mozilla.org/en-US/docs/Web/API/InputEvent"),

      // And up to local.test
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_UP)),
      WaitTillOmniboxViewText("https://local.test/input/"),

      // Press enter to accept.
      SendEnter(),

      // This should navigate to the page (or rather the 404 added by our
      // interceptor).
      WaitForWebContentsNavigation(kTabId, GURL("https://local.test/input/")),

      // Removing the focus should hide the popup.
      RemoveFocusFromPopup());
}

// Use tab key to navigate suggestions, including the delete buttons, and
// pressing space on one to delete an entry, as well as deleting an entry
// via Shift-Delete.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest,
                       NavigateSuggestionsTab) {
  FAILS_IN_MODE(Mode::kFull,
                "Arrow then tab has strange(?) behavior in full."
                "See crbug.com/550284225");

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this->browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  GURL url("https://local.test/");
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  history_service->AddPage(GURL("https://local.test/1"),
                           base::Time::Now() - base::Minutes(1),
                           history::SOURCE_BROWSED);
  history_service->AddPage(GURL("https://local.test/2"),
                           base::Time::Now() - base::Minutes(2),
                           history::SOURCE_BROWSED);
  ui_test_utils::WaitForHistoryToLoad(history_service);

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text, it should show up.
      EnterText(u"https://local"), WaitForPopupShow(),
      WaitTillInlineComplete("https://local", ".test"),
      // Should have a bunch of suggestions.
      WaitForMatch(PopupWebContents(), MatchText(0), "https://local.test"),
      WaitForVerbatimMatch(PopupWebContents(), MatchText(1), "https://local"),
      WaitForMatch(PopupWebContents(), MatchText(2), "https://local.test/1"),
      WaitForMatch(PopupWebContents(), MatchText(3), "https://local.test/2"),
      // Make the first navigation with arrow keys, to not care about any AIM
      // button.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_DOWN)),
      WaitTillOmniboxViewText("https://local"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_TAB)),
      WaitTillOmniboxViewText("https://local.test/1"),
      // Tab again will be the delete button, so the text won't change.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_TAB)),
      WaitTillOmniboxViewText("https://local.test/1"),
      // Space will delete the entry, so now
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_SPACE)),
      // Should now be at /2, and that should also be the [2]nd suggestion
      WaitTillOmniboxViewText("https://local.test/2"),
      WaitForMatch(PopupWebContents(), MatchText(2), "https://local.test/2"),
      // Delete via key shortcut.
      InAnyContext(
          SendKeyPress(InputWebContents(), ui::VKEY_DELETE, ui::EF_SHIFT_DOWN)),
      WaitTillOmniboxViewText("https://local"));
}

// Use an inline suggestion.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, InlineSuggestion) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this->browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  GURL url("https://local.test/");
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  ui_test_utils::WaitForHistoryToLoad(history_service);

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text, it should show up.
      EnterText(u"https://local"), WaitForPopupShow(),
      WaitTillInlineComplete("https://local", ".test"),

      // Enter . to advance completion.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_OEM_PERIOD)),
      WaitTillInlineComplete("https://local.", "test"),

      // Likewise for t.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_T)),
      WaitTillInlineComplete("https://local.t", "est"),

      // Accept it.
      SendEnter(),
      WaitForWebContentsNavigation(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic));
}

IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest,
                       FocusLocationNoDefaultSuggestion) {
  FAILS_IN_MODE(Mode::kFull,
                "Relies on additional text as a source for sync suggestions;"
                "see crbug.com/525540275");

  auto shortcuts_backend =
      ShortcutsBackendFactory::GetForProfile(browser()->GetProfile());
  if (!shortcuts_backend->initialized()) {
    base::RunLoop run_loop;
    NotifyWhenShortcutsLoadedObserver notify_init(run_loop.QuitClosure());
    shortcuts_backend->AddObserver(&notify_init);
    run_loop.Run();
    shortcuts_backend->RemoveObserver(&notify_init);
  }

  std::array<TestShortcutData, 1> test_shortcut = {
      // Thanks, shortcuts_provider_unittest.cc
      {{"BD85DBA2-8C29-49F9-84AE-48E1E12345E0", "https://www.cnn.com",
        "www.cnn.com/index.html", "https://www.cnn.com/index.html",
        AutocompleteMatch::DocumentType::NONE, "www.cnn.com/index.html", "0,1",
        "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
        "0,0,19,2,23,0,38,2,45,0", ui::PAGE_TRANSITION_TYPED,
        AutocompleteMatchType::HISTORY_TITLE, "", 1, 10}}};
  PopulateShortcutsBackendWithTestData(shortcuts_backend, test_shortcut);

  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_LOCATION, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      WaitTillOmniboxViewText("about:blank", View::kStatic),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0),
                                   View::kStatic),
      // Unfocus, since we want to test us focusing.
      FocusTab(), NavigateWebContents(kTabId, GURL("https://www.cnn.com/")),
      WaitTillOmniboxViewText("cnn.com", View::kStatic),
      // Press Ctrl-L; it should not show a suggestion (as additional text, in
      // this case).
      SendAccelerator(kBrowserViewElementId, accelerator),
      // Since we are checking for a negative, delay before checking.
      DoWaitForTime(base::Milliseconds(500)),
      CheckJsResultAt(kWebUIToolbarId, kOmniboxAdditionalText,
                      "el => el.textContent === ''"));
}

// Faking composing the way PretendComposeInputMethod does doesn't appear to
// work on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InlineSuggestionIME DISABLED_InlineSuggestionIME
#else
#define MAYBE_InlineSuggestionIME InlineSuggestionIME
#endif
IN_PROC_BROWSER_TEST_P(WebUILocationBarIMEInteractiveUiTest,
                       MAYBE_InlineSuggestionIME) {
  FAILS_IN_MODE(Mode::kFull, "No separate autocomplete IME in full");
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this->browser()->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  GURL url("https://local.test/");
  history_service->AddPage(url, base::Time::Now(), history::SOURCE_BROWSED);
  ui_test_utils::WaitForHistoryToLoad(history_service);

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Clear the box.
      EnterText(u""),

      // 'z' composes "loc", which should kick off completion (if we give it
      // a chance).
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_Z)),
      WaitTillOmniboxViewText("loc"),
      WaitTillStandaloneInlineComplete("al.test"),

      // 'x' composes to local.t'
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_X)),
      WaitTillOmniboxViewText("local.t"),
      WaitTillStandaloneInlineComplete("est"),

      // Accept it.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_RETURN)),
      WaitForWebContentsNavigation(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test"));
}

IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, AdditionalText) {
  FAILS_IN_MODE(Mode::kFull,
                "No additional text in full, it seems."
                "See crbug.com/525540275");
  auto shortcuts_backend =
      ShortcutsBackendFactory::GetForProfile(browser()->GetProfile());
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

  RunTestSequence(InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
                  InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
                  FocusOmnibox(), WaitTillOmniboxViewFocus(), EnterText(u"ne"),
                  WaitForPopupShow(),
                  InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_W)),
                  WaitTillInlineComplete("new", "s weather"),
                  WaitTillAdditionalText(" - www.cnn.com/index.html"),
                  InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
                  WaitTillOmniboxViewText("new"), WaitTillAdditionalText(""),
                  InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
                  WaitTillOmniboxViewText("ne"), WaitTillAdditionalText(""));
}

// Use Ctrl-Alt-Enter to append www. and .com to URL and open it in new tab.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, Modifiers) {
  FAILS_IN_MODE(Mode::kFull,
                "Full popup doesn't appear to do Ctrl-Enter when oneline."
                "See crbug.com/549864613");
  RunTestSequence(InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
                  InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
                  EnsureNoPopup(), FocusOmnibox(),
                  // Shouldn't have a popup visible yet.
                  EnsureNoPopup(),
                  // Type some text, it should show up. We include the schema to
                  // make sure we always end up with https://.
                  EnterText(u"https://google"), WaitForPopupShow(),
                  WaitTillOmniboxViewText("https://google"),
                  // Omnibox needs to see Ctrl pressed down, not just as
                  // modifier, to append stuff around it.
                  FakeKeyDown("Control"), InstrumentNextTab(kSecondTabId),
                  FakeKeyDown("Enter",
                              /*shift=*/false, /*control=*/true,
                              /*alt=*/true, /*command=*/false),
                  WaitForWebContentsNavigation(kSecondTabId,
                                               GURL("https://www.google.com")));
}

// Clicking the location icon should show the Page Info bubble.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, ClickLocationIcon) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId,
                  {"toolbar-app", "location-bar", "location-icon", "#button"},
                  "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
}

// Clicking the location icon should still show the Page Info bubble if we
// unelide.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest,
                       ClickLocationIconAfterUnelide) {
  FAILS_IN_MODE(Mode::kFull,
                "Can't click location icon in full, unelide or not."
                "crbug.com/550544530");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(),
      // about:blank will conveniently give us focus.
      WaitTillOmniboxViewFocus(),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic),
      SendKeyPress(kWebUIToolbarId, ui::VKEY_LEFT),
      WaitTillOmniboxViewText("https://local.test"),
      // Close the popup
      RemoveFocusFromPopup(),
      // Now the location icon should be clickable.
      ExecuteJsAt(kWebUIToolbarId,
                  {"toolbar-app", "location-bar", "location-icon", "#button"},
                  "el => el.click()"),
      WaitForShow(PageInfoBubbleViewBase::kPageInfoBubbleElementIdentifier));
}

// Interact with @tabs search keyword.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, SearchAtKeyword) {
  FAILS_IN_MODE(Mode::kFull,
                "@tabs doesn't work w/full popup."
                "See crbug.com/550562197");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(), WaitTillOmniboxViewFocus(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text.
      EnterText(u"@tab"), WaitForPopupShow(), WaitTillOmniboxViewText("@tab"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_S)),
      WaitTillOmniboxViewText("@tabs"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_SPACE)),
      // Omnibox text should should become empty, and a keyword chip
      // should show up.
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Tabs"),
      WaitTillOmniboxViewPlaceholder(u"Enter a word or two"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_S)),
      WaitTillOmniboxViewText("s"), WaitTillSearchKeywordText("Search Tabs"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Tabs"),
      // Backspace with only chip present converts it back to plain text.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
      WaitTillOmniboxViewText("@tabs "),
      EnsureNotPresent(InputWebContents(), SearchKeyword()));
}

// Interact with 'google.com' as a search keyword.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, SearchKeyword) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      EnsureNoPopup(), FocusOmnibox(), WaitTillOmniboxViewFocus(),
      // Shouldn't have a popup visible yet.
      EnsureNoPopup(),
      // Type some text.
      EnterText(u"google.com"), WaitForPopupShow(),
      WaitTillOmniboxViewText("google.com"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_SPACE)),
      // Omnibox text should should become empty, and a keyword chip
      // should show up.
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Google"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_S)),
      WaitTillOmniboxViewText("s"), WaitTillSearchKeywordText("Search Google"),
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
      WaitTillOmniboxViewText(""), WaitTillSearchKeywordText("Search Google"),
      // Backspace with only chip present converts it back to plain text.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_BACK)),
      WaitTillOmniboxViewText("google.com "),
      EnsureNotPresent(InputWebContents(), SearchKeyword()));
}

// Tests that click-focusing the omnibox selects all (and accidentally
// default focus behavior for about:blank pages).
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, ClickSelectsAll) {
  PORT_UNFINISHED(Mode::kFull,
                  "WebContents for popup re-showing up to ElementTracker is "
                  "broken somehow");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(),
      // The browser will focus the location bar automatically since it's
      // about-blank; and since it didn't have focus before, it should
      // select-all.
      WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Clear selection.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_LEFT)),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Transfer the focus to contents.
      FocusTab(),
      If([&]() { return mode() == Mode::kFull; },
         Then(InAnyContext(WaitForHide(kClassicPopupWebViewId)))),
      // Now click the omnibox; the contents should get selected again.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      // Make sure the element for popup WebContents shows up so again we can
      // poll it.
      If([&]() { return mode() == Mode::kFull; },
         Then(InAnyContext(WaitForShow(kClassicPopupWebViewId)))),
      InAnyContext(WaitTillOmniboxViewFocus()),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)));
}

IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, Placeholder) {
  FAILS_IN_MODE(Mode::kFull, "Full doesn't seem to have the AIM placeholder");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      // The browser will focus the location bar automatically since it's
      // about-blank; and since it didn't have focus before, it should
      // select-all.
      WaitTillOmniboxViewFocus(),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Delete everything
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_DELETE)),
      WaitTillOmniboxViewText(""),
      WaitTillOmniboxViewPlaceholder(
          u"\u21E5 Press tab then enter to ask AI Mode"),
      // Transfer the focus to contents.
      FocusTab(),
      // Now we should get the regular search placeholder, not AIM one.
      WaitTillOmniboxViewPlaceholder(u"Ask Google or type a URL"));
}

// Click when already focused doesn't select all.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest,
                       SecondClickDoesNotSelectAll) {
#if BUILDFLAG(IS_MAC)
  PORT_UNFINISHED(Mode::kFull, "Something wrong on Mac");
#endif

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      // The browser will focus the location bar automatically since it's
      // about-blank; and since it didn't have focus before, it should
      // select-all.
      WaitTillOmniboxViewFocus(),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Clear selection.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_LEFT)),
      WaitTillOmniboxViewSelection("", gfx::Range()),
      // Now click the omnibox; should not select-all; and since it
      // clicked in the middle and the URL is pretty short, the caret should be
      // at the end.
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      WaitTillOmniboxViewSelection("", gfx::Range(11)));
}

// Test that pressing home triggers unelision.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, UnelideHome) {
  PORT_UNFINISHED(Mode::kFull, "Needs debugging");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusTab(),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic),
      // Click to focus location bar.
      MoveMouseTo(kOmniboxElementId), ClickMouse(), WaitTillOmniboxViewFocus(),
      // Selected, but not unelided yet.
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("local.test", gfx::Range(10, 0)),
      // Press home. This should trigger unelision.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_HOME)),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(0)));
}

// Tests that if initial interaction just selected-all and didn't unelide
// that moving the caret will unelide.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, UnelideCaretMove) {
  FAILS_IN_MODE(Mode::kFull, "Gets wrong caret position at the end (minor)");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusTab(),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic),
      // Click to focus location bar.
      MoveMouseTo(kOmniboxElementId), ClickMouse(), WaitTillOmniboxViewFocus(),
      // Selected, but not unelided yet.
      WaitTillOmniboxViewText("local.test"),
      WaitTillOmniboxViewSelection("local.test", gfx::Range(10, 0)),
      // Press left-arrow. This should trigger unelision (and put
      // the caret after the scheme).
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_LEFT)),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("", gfx::Range(8)));
}

// Test of Ctrl-K focus omnibox + activates default search shortcut.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, FocusSearch) {
  PORT_UNFINISHED(Mode::kFull,
                  "Somehow search keywords get prepended; OK in manual test;"
                  "possibly a race from going too fast?");

  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_SEARCH, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewFocus(),
      InAnyContext(SendAccelerator(InputWebContents(), accelerator)),
      // Since the user didn't change text, it should be cleared.
      WaitTillOmniboxViewFocus(), WaitTillOmniboxViewText(""),
      WaitTillSearchKeywordText("Search Google"),
      // Enter a character.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_S)),
      WaitTillOmniboxViewText("s"),
      // Pressing the accel again should select-all
      InAnyContext(SendAccelerator(InputWebContents(), accelerator)),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"),
      // Transfer the focus to contents. Search keyword should still be active.
      FocusTab(),
      // Wait a bit to get things a chance to screw up if we're doing the wrong
      // thing here.
      DoWaitForTime(base::Milliseconds(100)), WaitTillOmniboxViewText("s"),
      WaitTillSearchKeywordText("Search Google"));
}

// Test of Ctrl-K focus omnibox when the user has edited the text.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, FocusSearch2) {
  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_SEARCH, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      // Since it's about:blank we should have focus.
      WaitTillOmniboxViewFocus(),
      // Enter a character.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_S)),
      WaitTillOmniboxViewText("s"),
      // Ctrl-K with custom input should preserve it (and select it).
      InAnyContext(SendAccelerator(InputWebContents(), accelerator)),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"),
      // Cancel selection.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_LEFT)),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Ctrl-K again will reapply the select-all.
      InAnyContext(SendAccelerator(InputWebContents(), accelerator)),
      WaitTillOmniboxViewText("s"),
      WaitTillOmniboxViewSelection("s", gfx::Range(0, 1)),
      WaitTillSearchKeywordText("Search Google"));
}

// Test of Ctrl-L (and others) focus location bar.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, FocusLocation) {
  ui::Accelerator accelerator;
  EXPECT_TRUE(
      AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
          IDC_FOCUS_LOCATION, &accelerator));

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      // Unfocus, since we want to test us focusing.
      FocusTab(),
      // Need a URL that will get trigger elision to test this
      // (about:blank won't).
      NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic),
      // Press Ctrl-L; it should focus, unelide, and select-all. Also should
      // not add a search chip, since that's a separate accel.
      SendAccelerator(kBrowserViewElementId, accelerator),
      WaitTillOmniboxViewFocus(), WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("https://local.test", gfx::Range(18, 0)),
      EnsureNotPresent(InputWebContents(), SearchKeyword()),
      // Clear selection.
      InAnyContext(SendKeyPress(InputWebContents(), ui::VKEY_LEFT)),
      WaitTillOmniboxViewSelection("", gfx::Range(0)),
      // Ctrl-L again should reapply the select-all.
      InAnyContext(SendAccelerator(InputWebContents(), accelerator)),
      WaitTillOmniboxViewText("https://local.test"),
      WaitTillOmniboxViewSelection("https://local.test", gfx::Range(18, 0)),
      EnsureNotPresent(InputWebContents(), SearchKeyword()));
}

IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, TypeWithMouseDown) {
  PORT_UNFINISHED(Mode::kFull, "Needs debugging");
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
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, DoubleClick) {
#if BUILDFLAG(IS_MAC)
  // Mac likes to make selections non-directional by default, and this test
  // has it setting one rather than our code.
  const bool expect_no_dir = true;
  PORT_UNFINISHED(Mode::kFull, "Something wrong on mac (no mouse fwd)?");
#else
  const bool expect_no_dir = false;
#endif

  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusTab(), NavigateWebContents(kTabId, GURL("https://local.test")),
      // Navigation will deactivate any full popup
      WaitTillOmniboxViewText("local.test", View::kStatic),
      WaitTillOmniboxViewSelection(
          "", mode() == Mode::kFull ? gfx::Range(0) : gfx::Range(10),
          View::kStatic),
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
      WaitTillOmniboxViewSelection("local", gfx::Range(8, 13), View::kEditable,
                                   expect_no_dir));
}

// Test of selecting a word portion of URL with double-click select.
// This arranges for unelision to have happened on first click and not second;
// and selects the last word.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, DoubleClick2) {
#if BUILDFLAG(IS_MAC)
  PORT_UNFINISHED(Mode::kFull, "Something wrong on mac (no mouse fwd)?");
#endif
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusTab(), NavigateWebContents(kTabId, GURL("https://local.test")),
      WaitTillOmniboxViewText("local.test", View::kStatic),
      WaitTillOmniboxViewSelection(
          "", mode() == Mode::kFull ? gfx::Range(0) : gfx::Range(10),
          View::kStatic),
      // Focus location bar. This is important since if it's already focused
      // it won't try to select-all on first click. Also we do it with
      // JS and not Ctrl-L since that would unelide.
      FocusWebContents(kWebUIToolbarId),
      ExecuteJsAt(kWebUIToolbarId, kOmniboxInputDeepQuery, "el => el.focus()"),
      WaitTillOmniboxViewFocus(View::kStatic),
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

// The context menu tests don't appear to work on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ContextMenu DISABLED_ContextMenu
#define MAYBE_ContextMenu2 DISABLED_ContextMenu2
#else
#define MAYBE_ContextMenu ContextMenu
#define MAYBE_ContextMenu2 ContextMenu2
#endif

// Test of location bar context menu; uses the 'Copy' item.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, MAYBE_ContextMenu) {
  FAILS_IN_MODE(Mode::kFull,
                "Selection lost on right-click when uneliding; possibly "
                "the same as crbug.com/549791744");
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusTab(), NavigateWebContents(kTabId, GURL("https://local.test")),
      // Navigation will deactivate any full popup
      WaitTillOmniboxViewText("local.test", View::kStatic),
      MoveMouseTo(kOmniboxElementId),
      // Click to select text, so we have something to copy.
      ClickMouse(),
      WaitTillOmniboxViewSelection("local.test", gfx::Range(10, 0)),
      // Make sure it's actually focused, so the selection ops work
      WaitTillOmniboxViewFocus(),
      // Open context menu.
      MoveMouseTo(kOmniboxElementId), ClickMouse(ui_controls::RIGHT),
      // Copy item should work, and restore the schema.
      InAnyContext(WaitForShow(OmniboxContextMenuMixinBase::kCopyMenuItem)),
      InSameContext(SelectMenuItem(OmniboxContextMenuMixinBase::kCopyMenuItem)),
      ObserveState(kClipboardText,
                   []() { return ui::ClipboardMonitor::GetInstance(); }),
      WaitForState(kClipboardText, u"https://local.test/"),
      StopObservingState(kClipboardText));
}

// Test of location bar context menu; uses the 'always show full URLs' item.
IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, MAYBE_ContextMenu2) {
  RunTestSequence(
      InstrumentTab(kTabId), WaitForWebContentsReady(kTabId),
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      FocusTab(), NavigateWebContents(kTabId, GURL("https://local.test")),
      // Navigation will deactivate any full popup
      WaitTillOmniboxViewText("local.test", View::kStatic),
      // Open context menu.
      MoveMouseTo(kOmniboxElementId), ClickMouse(ui_controls::RIGHT),
      // Tell it should show full urls.
      InAnyContext(
          WaitForShow(OmniboxContextMenuMixinBase::kShowFullUrlsMenuItem)),
      InSameContext(
          SelectMenuItem(OmniboxContextMenuMixinBase::kShowFullUrlsMenuItem)),
      // This transfers controls back to location bar since it RevertAll()s
      WaitTillOmniboxViewText("https://local.test", View::kStatic));
}

IN_PROC_BROWSER_TEST_P(WebUILocationBarInteractiveUiTest, PasteSanitizesText) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"  javascript:javascript:alert(1)\r\nhello world\n");
  }

  ui::Accelerator paste_accel(ui::VKEY_V,
#if BUILDFLAG(IS_MAC)
                              ui::EF_COMMAND_DOWN
#else
                              ui::EF_CONTROL_DOWN
#endif
  );

  RunTestSequence(
      InstrumentNonTabWebView(kWebUIToolbarId, GetToolbarWebView()),
      HandleAutofocus(), WaitTillOmniboxViewText("about:blank"),
      WaitTillOmniboxViewSelection("about:blank", gfx::Range(11, 0)),
      InAnyContext(SendAccelerator(InputWebContents(), paste_accel)),
      WaitTillOmniboxViewText("alert(1) hello world"));
}

#if BUILDFLAG(IS_WIN)
// The test fixture sets up ui_test_utils::BringBrowserWindowToFront to run
// early on, and that hangs the tests with Full Popup on Windows. Unfortunately
// disabling it seems like a no-go, since it's done for exactly the sort of
// things our tests do
#define MODES_TO_TEST Mode::kCutout
#else
#define MODES_TO_TEST Mode::kCutout, Mode::kFull
#endif

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebUILocationBarInteractiveUiTest,
    ::testing::Values(MODES_TO_TEST),
    [](const testing::TestParamInfo<Mode>& info) {
      return ModeToString(info.param);
    });

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebUILocationBarIMEInteractiveUiTest,
    ::testing::Values(MODES_TO_TEST),
    [](const testing::TestParamInfo<Mode>& info) {
      return ModeToString(info.param);
    });
