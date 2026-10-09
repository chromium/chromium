// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/omnibox/webui_readonly_omnibox.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAimPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kPopupSearchbox = {"omnibox-full-app",
                                   "omnibox-popup-searchbox"};
const DeepQuery kWebUIInput = {"omnibox-full-app", "omnibox-popup-searchbox",
                               "cr-searchbox-input", "#input"};
const DeepQuery kFirstSuggestionMatch = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='1']"};
const DeepQuery kFirstSuggestionMatchPrimaryText = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='1']", "#primaryText"};
// The keyword chip on the default match, e.g. "Search kw".
const DeepQuery kDefaultMatchKeywordChip = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='0']", "#keyword"};
const DeepQuery kComposeButton = {"omnibox-full-app", "omnibox-popup-searchbox",
                                  "cr-searchbox-compose-button",
                                  "#composeButton"};

const DeepQuery kAimInput = {"omnibox-aim-app", "#composebox",
                             "cr-composebox-input", "#input"};
const DeepQuery kCancelIcon = {"omnibox-aim-app", "#composebox",
                               "cr-composebox-input", "#cancelIcon"};
const DeepQuery kAimSubmit = {"omnibox-aim-app", "#composebox",
                              "cr-composebox-submit", "#submitContainer"};
}  // namespace

class FullWebUIOmniboxInteractiveTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  FullWebUIOmniboxInteractiveTestBase() {
    // Start on the NTP, like a real new window, rather than about:blank. Tests
    // that need a different first page navigate there explicitly.
    set_open_about_blank_on_browser_launch(false);
    // By default, `interactive_ui_tests` asserts `BringBrowserWindowToFront()`
    // succeeds during setup. However, this can return false when the WebUI
    // popup already holds foreground focus in its separate widget, so we
    // explicitly activate the browser and ignore the return value.
    set_global_browser_set_up_function(
        [](const BrowserWindowInterface* browser) {
          BrowserView::GetBrowserViewForBrowser(browser)->Activate();
          (void)ui_test_utils::BringBrowserWindowToFront(browser);
          return true;
        });
  }
  ~FullWebUIOmniboxInteractiveTestBase() override = default;

  void TearDownOnMainThread() override {
    // `second_browser_` is owned by the browser list and destroyed in
    // `InProcessBrowserTest::QuitBrowsers()`, which runs after this. Clear it
    // first so the `raw_ptr` does not dangle.
    second_browser_ = nullptr;
    SearchboxInteractiveTestMixin<WebUiInteractiveTestMixin<
        InteractiveBrowserTest>>::TearDownOnMainThread();
  }

 protected:
  auto GetActivePopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      if (!browser_view || !browser_view->GetLocationBar()) {
        return nullptr;
      }
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          browser_view->GetLocationBar()->GetOmniboxPopupView());
      if (!popup_view || !popup_view->presenter()) {
        return nullptr;
      }
      return popup_view->presenter()->GetWebUIContent();
    });
  }

  auto CheckWebUIInputFocus(bool expected_focus) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWebUIInputFocusChanged);
    StateChange focus_changed;
    focus_changed.event = kWebUIInputFocusChanged;
    focus_changed.where = kWebUIInput;
    focus_changed.test_function = base::StringPrintf(
        R"((el) => {
              let is_focused = false;
              if (el && el.ownerDocument.hasFocus()) {
                let active = el.ownerDocument.activeElement;
                while (active?.shadowRoot?.activeElement) {
                  active = active.shadowRoot.activeElement;
                }
                is_focused = (active === el);
              }
              return is_focused === %s;
            })",
        expected_focus ? "true" : "false");
    focus_changed.continue_across_navigation = true;
    return WaitForStateChange(kPopupWebView, focus_changed);
  }

  // Returns the `OmniboxController` for the active browser, or null. Routed
  // through `LocationBar` rather than `LocationBarView` so this also works
  // when the WebUI toolbar is enabled and `GetLocationBarView()` is null.
  OmniboxController* GetOmniboxControllerForTest() {
    auto* browser_window = BrowserWindow::FromBrowser(browser());
    auto* location_bar =
        browser_window ? browser_window->GetLocationBar() : nullptr;
    return location_bar ? location_bar->GetOmniboxController() : nullptr;
  }

  auto WaitForPopupState(OmniboxPopupState expected_state) {
    return PollUntil(
        [this, expected_state]() -> bool {
          auto* controller = GetOmniboxControllerForTest();
          return controller && controller->popup_state_manager() &&
                 controller->popup_state_manager()->popup_state() ==
                     expected_state;
        },
        "WaitForPopupState");
  }

  // One-shot counterpart to `WaitForPopupState()`. Use it to assert that the
  // state hasn't changed (e.g. the popup stays open), where polling would hide
  // a transient change and turn a failure into a timeout.
  auto CheckPopupState(OmniboxPopupState expected_state) {
    return CheckResult(
        [this]() {
          return GetOmniboxControllerForTest()
              ->popup_state_manager()
              ->popup_state();
        },
        expected_state, "CheckPopupState");
  }

  auto CheckUserInputInProgress(bool expected) {
    return CheckResult(
        [this]() {
          return GetOmniboxControllerForTest()
              ->edit_model()
              ->user_input_in_progress();
        },
        expected, "CheckUserInputInProgress");
  }

  // Adds an active search engine with keyword `keyword`, so that typing exactly
  // `keyword` shows a tab-to-search keyword chip on the default match.
  void AddKeywordSearchEngine(const std::u16string& keyword) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    TemplateURLData data;
    data.SetShortName(keyword);
    data.SetKeyword(keyword);
    data.SetURL("https://example.com/?q={searchTerms}");
    data.is_active = TemplateURLData::ActiveStatus::kTrue;
    template_url_service->Add(std::make_unique<TemplateURL>(data));
  }

  // Waits until `OmniboxEditModel::is_keyword_selected()` matches `expected`.
  auto WaitForEditModelKeywordSelected(bool expected) {
    return PollUntil(
        [this, expected]() -> bool {
          auto* controller = GetOmniboxControllerForTest();
          return controller && controller->edit_model() &&
                 controller->edit_model()->is_keyword_selected() == expected;
        },
        "WaitForEditModelKeywordSelected");
  }

  // Waits until the WebUI searchbox's keyword mode matches `expected`.
  auto WaitForWebUIKeywordMode(bool expected) {
    return WaitForJsConditionAt(
        kPopupWebView, kPopupSearchbox,
        base::StringPrintf(
            "(el) => !!el && el.keywordModeManager.isInKeywordMode === %s",
            expected ? "true" : "false"));
  }

  // Waits until `OmniboxEditModel::has_focus()` matches `expected_focus`.
  //
  // In Full WebUI mode, user text input lives in the WebUI popup, and
  // `LocationBarView::UpdateFocusBehavior()` pins `OmniboxViewViews` to
  // `FocusBehavior::NEVER` once ready. Therefore, native Views focus on
  // `OmniboxViewViews` is permanently false and cannot reflect logical focus or
  // blur. Similarly, when the WebUI toolbar is active, `WebUIReadOnlyOmnibox`
  // sets its own focus state to false during the focus handoff to the popup.
  // The cross-widget `OmniboxEditModel` is the single source of truth for
  // logical omnibox focus across both toolbar architectures and all platforms.
  auto WaitForEditModelFocus(bool expected_focus) {
    return PollUntil(
        [this, expected_focus]() -> bool {
          auto* controller = GetOmniboxControllerForTest();
          return controller && controller->edit_model() &&
                 controller->edit_model()->has_focus() == expected_focus;
        },
        "WaitForEditModelFocus");
  }

  // One-shot counterpart to `WaitForEditModelFocus()`. Use it to assert that
  // focus hasn't changed, where polling would hide a transient change and turn
  // a failure into a timeout.
  auto CheckEditModelFocus(bool expected_focus) {
    return CheckResult(
        [this]() {
          return GetOmniboxControllerForTest()->edit_model()->has_focus();
        },
        expected_focus, "CheckEditModelFocus");
  }

  // Checks that the native Views omnibox does not have focus, since in Full
  // WebUI mode text input lives in the popup. With the WebUI toolbar there is
  // no `LocationBarView` or Views omnibox (and `kOmniboxElementId` identifies a
  // `ui::TrackedElementWebUI`), so there is nothing to check. This checks for
  // the view rather than the feature, since some platforms (e.g. ChromeOS)
  // keep the Views toolbar even when `features::kWebUIToolbar` is enabled.
  auto CheckNativeOmniboxViewUnfocused() {
    return CheckResult(
        [this]() {
          auto* location_bar_view =
              BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetLocationBarView();
          if (!location_bar_view) {
            return false;
          }
          CHECK(location_bar_view->omnibox_view());
          return location_bar_view->omnibox_view()->HasFocus();
        },
        false, "CheckNativeOmniboxViewUnfocused");
  }

  // Returns the Views-side view that hosts the location bar. With the WebUI
  // toolbar there is no `LocationBarView`; the location bar lives inside the
  // toolbar's WebView, so that WebView is the closest Views equivalent. This
  // checks for the view rather than the feature, since some platforms (e.g.
  // ChromeOS) keep the Views toolbar even when `features::kWebUIToolbar` is
  // enabled.
  views::View* GetLocationBarHostView() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (!browser_view) {
      return nullptr;
    }
    if (auto* webui_toolbar = browser_view->toolbar_button_provider()
                                  ->GetWebUIToolbarViewForTesting()) {
      return webui_toolbar;
    }
    return browser_view->GetLocationBarView();
  }

  // Asserts coherent omnibox focus across backend edit model, native views,
  // and WebUI DOM input.
  auto WaitForOmniboxFocus(bool expected_focus) {
    if (expected_focus) {
      return Steps(WaitForEditModelFocus(true),
                   InAnyContext(CheckWebUIInputFocus(true)),
                   CheckNativeOmniboxViewUnfocused());
    }
    return Steps(WaitForEditModelFocus(false),
                 CheckNativeOmniboxViewUnfocused());
  }

  // Asserts that an open/visible popup is unfocused (e.g. when an uncommitted
  // draft keeps the popup header visible after clicking outside):
  // 1. The C++ edit model has released logical focus.
  // 2. The WebUI searchbox DOM input has lost focus (caret no longer blinks).
  // 3. The native OmniboxViewViews textfield does not retain focus.
  auto WaitForVisibleOmniboxUnfocused() {
    return Steps(WaitForEditModelFocus(false),
                 InAnyContext(CheckWebUIInputFocus(false)),
                 CheckNativeOmniboxViewUnfocused());
  }

  // Counterparts to `WaitForOmniboxFocus()` and
  // `WaitForVisibleOmniboxUnfocused()` that check the edit model focus once
  // instead of polling. Use them to assert that focus hasn't changed. The WebUI
  // DOM focus lives in the renderer, so that part still waits.
  auto CheckOmniboxFocus(bool expected_focus) {
    if (expected_focus) {
      return Steps(CheckEditModelFocus(true),
                   InAnyContext(CheckWebUIInputFocus(true)),
                   CheckNativeOmniboxViewUnfocused());
    }
    return Steps(CheckEditModelFocus(false), CheckNativeOmniboxViewUnfocused());
  }

  auto CheckVisibleOmniboxUnfocused() {
    return Steps(CheckEditModelFocus(false),
                 InAnyContext(CheckWebUIInputFocus(false)),
                 CheckNativeOmniboxViewUnfocused());
  }

  auto WaitForPopupReady() {
    return Steps(
        InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
        InAnyContext(
            InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))),
        WaitForOmniboxFocus(true));
  }

  auto WaitForWebUIInputValue(const std::string& expected_value) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWebUIInputValueChanged);
    StateChange value_changed;
    value_changed.event = kWebUIInputValueChanged;
    value_changed.where = kWebUIInput;
    value_changed.test_function = base::StringPrintf(
        "(el) => el && el.value === '%s'", expected_value.c_str());
    value_changed.continue_across_navigation = true;
    return WaitForStateChange(kPopupWebView, value_changed);
  }

  auto InputWebUIText(const std::string& text) {
    return Steps(InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                                          base::StringPrintf(R"(el => {
               const fullText = '%s';
               for (let i = 0; i < fullText.length; i++) {
                 el.value = fullText.substring(0, i + 1);
                 el.setSelectionRange(i + 1, i + 1);
                 el.dispatchEvent(new Event('input'));
                 document.dispatchEvent(new Event('selectionchange'));
               }
             })",
                                                             text.c_str()))),
                 InAnyContext(WaitForWebUIInputValue(text)));
  }

  auto PasteWebUIText(const std::string& text) {
    return Steps(InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                                          base::StringPrintf(R"(el => {
                const data = new DataTransfer();
                data.setData('text/plain', '%s');
                const event = new ClipboardEvent('paste', {
                  clipboardData: data,
                  bubbles: true,
                  cancelable: true,
                  composed: true,
                });
                el.dispatchEvent(event);
              })",
                                                             text.c_str()))),
                 InAnyContext(WaitForWebUIInputValue(text)));
  }

  auto CopyWebUIText() {
    return Steps(InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput, R"(el => {
      el.select();
      const event = new ClipboardEvent('copy', {
        bubbles: true,
        cancelable: true,
        composed: true,
      });
      el.dispatchEvent(event);
    })")));
  }

  auto CutWebUIText() {
    return Steps(InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput, R"(el => {
      el.select();
      const event = new ClipboardEvent('cut', {
        bubbles: true,
        cancelable: true,
        composed: true,
      });
      el.dispatchEvent(event);
    })")));
  }

  auto ClearWebUIText() {
    return Steps(InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                                          R"(el => {
               el.value = '';
               el.dispatchEvent(new Event('input'));
             })")),
                 InAnyContext(WaitForWebUIInputValue("")));
  }

  auto SelectAllWebUIInput() {
    return InAnyContext(
        ExecuteJsAt(kPopupWebView, kWebUIInput, "el => el.select()"));
  }

  auto CheckWebUIInputSelection(int expected_start, int expected_end) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWebUIInputSelectionChanged);
    StateChange selection_changed;
    selection_changed.event = kWebUIInputSelectionChanged;
    selection_changed.where = kWebUIInput;
    selection_changed.test_function = base::StringPrintf(
        "(el) => el && el.selectionStart === %d && el.selectionEnd === %d",
        expected_start, expected_end);
    selection_changed.continue_across_navigation = true;
    return WaitForStateChange(kPopupWebView, selection_changed);
  }

  auto WaitForOmniboxText(const std::u16string& expected_text) {
    return PollUntil(
        [this, expected_text]() {
          auto* browser_window = BrowserWindow::FromBrowser(browser());
          if (!browser_window || !browser_window->GetLocationBar() ||
              !browser_window->GetLocationBar()->GetOmniboxView()) {
            return false;
          }
          return browser_window->GetLocationBar()
                     ->GetOmniboxView()
                     ->GetText() == expected_text;
        },
        "WaitForOmniboxText");
  }

  // Waits for the 100ms popup transition state lock that
  // `LocationBarView::OnPopupStateChanged` uses to suppress rapid popup
  // transitions.
  // TODO(b/514810983): Remove this helper once the transition lock is removed
  // from `LocationBarView`.
  auto WaitForPopupTransitionLockout(
      base::TimeDelta delay = base::Milliseconds(150)) {
    return Do([delay]() {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), delay);
      run_loop.Run();
    });
  }

  auto SwitchTab(ui::ElementIdentifier tab_strip, int tab_index) {
    return Steps(SelectTab(tab_strip, tab_index),
                 WaitForPopupTransitionLockout());
  }

  auto ClickWebPageBody(ui::ElementIdentifier tab_id) {
    return Steps(MoveMouseTo(ContentsWebView::kContentsWebViewElementId,
                             base::BindOnce([](ui::TrackedElement* el) {
                               gfx::Rect bounds = el->GetScreenBounds();
                               return gfx::Point(bounds.x() + 20,
                                                 bounds.bottom() - 20);
                             })),
                 ClickMouse());
  }

  // Returns `target_browser`'s popup widget, or null if there is none (it is
  // destroyed on hide when `kOmniboxFullWebUIDestroyWidgetOnHide` is enabled).
  static views::Widget* GetPopupWidget(BrowserWindowInterface* target_browser) {
    auto* view = BrowserView::GetBrowserViewForBrowser(target_browser);
    auto* location_bar = view ? view->GetLocationBar() : nullptr;
    auto* popup_view =
        location_bar ? location_bar->GetOmniboxPopupView() : nullptr;
    auto* presenter = popup_view ? popup_view->presenter() : nullptr;
    return presenter ? presenter->GetWidget() : nullptr;
  }

  // Waits until the popup widget is hidden and the edit model has released
  // logical omnibox focus.
  auto WaitForPopupDismissed() {
    return PollUntil(
        [this]() -> bool {
          auto* widget = GetPopupWidget(browser());
          if (widget && widget->IsVisible()) {
            return false;
          }
          auto* controller = GetOmniboxControllerForTest();
          return controller && !controller->edit_model()->has_focus();
        },
        "WaitForPopupDismissed");
  }

  // Clicks the top of the web contents, just below the collapsed popup, in the
  // area the popup's bottom shadow margin would cover if it were not dropped
  // while collapsed.
  auto ClickJustBelowCollapsedPopup() {
    return Steps(
        MoveMouseTo(
            ContentsWebView::kContentsWebViewElementId,
            base::BindLambdaForTesting([this](ui::TrackedElement* el) {
              const gfx::Rect contents_bounds = el->GetScreenBounds();
              const gfx::Point point(contents_bounds.CenterPoint().x(),
                                     contents_bounds.y() + 2);
              views::Widget* widget = GetPopupWidget(browser());
              CHECK(widget);
              const gfx::Rect popup_bounds = widget->GetWindowBoundsInScreen();
              // The point is outside the collapsed popup, but inside
              // the band a full bottom shadow margin would cover.
              EXPECT_FALSE(popup_bounds.Contains(point));
              EXPECT_LT(
                  point.y(),
                  popup_bounds.bottom() +
                      RoundedOmniboxResultsFrame::GetShadowInsets().bottom());
              return point;
            })),
        ClickMouse());
  }

#if defined(USE_AURA)
  // Clicks midway into the popup's transparent bottom shadow margin, which is
  // inside the popup widget's bounds but outside its content.
  auto ClickPopupShadowMargin() {
    return Steps(
        InAnyContext(MoveMouseTo(
            OmniboxPopupPresenter::kRoundedResultsFrame,
            base::BindOnce([](ui::TrackedElement* el) {
              const gfx::Rect bounds = el->GetScreenBounds();
              const int bottom =
                  RoundedOmniboxResultsFrame::GetShadowInsets().bottom();
              CHECK_GT(bottom, 1);
              return gfx::Point(bounds.CenterPoint().x(),
                                bounds.bottom() - bottom / 2);
            }))),
        InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                        ClickMouse()));
  }
#endif  // defined(USE_AURA)

  // Clicks the tab strip's empty drag region.
  auto ClickTabStrip() {
    return Steps(MoveMouseTo(kTabStripFrameGrabHandleElementId), ClickMouse());
  }

  // Returns whether `target_browser`'s window or its omnibox popup widget is
  // active.
  static bool IsBrowserOrPopupActive(BrowserWindowInterface* target_browser) {
    auto* view = BrowserView::GetBrowserViewForBrowser(target_browser);
    if (!view || !view->GetWidget()) {
      return false;
    }
    if (view->GetWidget()->IsActive()) {
      return true;
    }
    // When the full WebUI omnibox is shown and focused, native activation
    // transfers to the popup widget.
    auto* popup_widget = GetPopupWidget(target_browser);
    return popup_widget && popup_widget->IsActive();
  }

  // Brings `window` to the front. Callers activate and wait separately, so
  // failure is ignored.
  static void ShowAndFocusWindow(gfx::NativeWindow window) {
    // Allow blocking, since on Windows a failed attempt saves a desktop
    // snapshot to disk.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::ignore = ui_test_utils::ShowAndFocusNativeWindow(window);
  }

  // Opens and activates a second browser window so the original browser and its
  // popup lose activation.
  auto OpenAndActivateSecondBrowserWindow() {
    return Steps(Do([this]() {
                   second_browser_ = CreateBrowser(browser()->GetProfile());
                   ASSERT_TRUE(second_browser_);
                   auto* second_view =
                       BrowserView::GetBrowserViewForBrowser(second_browser_);
                   ASSERT_TRUE(second_view);
                   // Offset the new window so the original window's tab strip
                   // stays uncovered and clickable.
                   const gfx::Rect first_bounds =
                       BrowserView::GetBrowserViewForBrowser(browser())
                           ->GetWidget()
                           ->GetWindowBoundsInScreen();
                   second_view->GetWidget()->SetBounds(gfx::Rect(
                       first_bounds.x() + 100, first_bounds.y() + 200,
                       first_bounds.width() / 2, first_bounds.height() / 2));
                   ShowAndFocusWindow(second_view->GetNativeWindow());
                   second_view->Activate();
                 }),
                 PollUntil(
                     [this]() -> bool {
                       return IsBrowserOrPopupActive(second_browser_);
                     },
                     "WaitForSecondBrowserActive"));
  }

  // Switches to the tab at `tab_index` and waits for the popup to be restored
  // and focused by the tab-restore path itself (no explicit refocus), so that
  // focus and selection assertions reflect real restoration behavior.
  auto SwitchTabAndRestorePopup(ui::ElementIdentifier tab_strip,
                                int tab_index) {
    return Steps(SelectTab(tab_strip, tab_index),
                 WaitForPopupTransitionLockout(), WaitForPopupReady());
  }

  // Reactivates the original browser window and waits for activation to settle.
  auto ReactivateBrowserWindow() {
    return Steps(
        Do([this]() {
          auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
          ASSERT_TRUE(browser_view && browser_view->GetWidget());
          ShowAndFocusWindow(browser_view->GetNativeWindow());
          browser_view->Activate();
        }),
        PollUntil(
            [this]() -> bool { return IsBrowserOrPopupActive(browser()); },
            "WaitForBrowserActive"));
  }

  // Reactivates the original browser window via its popup widget.
  auto ReactivatePopupWidget() {
    return Steps(Do([this]() {
                   auto* popup_widget = GetPopupWidget(browser());
                   ASSERT_TRUE(popup_widget);
                   ShowAndFocusWindow(popup_widget->GetNativeWindow());
                   popup_widget->Activate();
                 }),
                 PollUntil(
                     [this]() -> bool {
                       auto* popup_widget = GetPopupWidget(browser());
                       return popup_widget && popup_widget->IsActive();
                     },
                     "WaitForPopupWidgetActive"));
  }

  // Waits for the popup to be fully open and coherently focused: visible,
  // `kFull`, logically focused, and holding native widget activation.
  auto WaitForPopupActive() {
    return Steps(
        InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
        WaitForPopupState(OmniboxPopupState::kFull),
        WaitForEditModelFocus(true),
        PollUntil(
            [this]() -> bool {
              auto* widget = GetPopupWidget(browser());
              return widget && widget->IsActive();
            },
            "WaitForPopupWidgetActive"));
  }

  auto WaitForBrowserActive() {
    return Do([this]() {
      browser()->GetWindow()->Activate();
      (void)ui_test_utils::BringBrowserWindowToFront(browser());
      BrowserView::GetBrowserViewForBrowser(browser())->Activate();
    });
  }

  auto FocusOmniboxAndWaitForPopupReady() {
    return Steps(WaitForPopupTransitionLockout(), Do([this]() {
                   if (auto* popup_view = BrowserWindow::FromBrowser(browser())
                                              ->GetLocationBar()
                                              ->GetOmniboxPopupView()) {
                     popup_view->OnFocus(/*query_zps=*/true);
                   }
                 }),
                 WaitForPopupReady());
  }

  auto OpenInitialTabAndFocusOmnibox(ui::ElementIdentifier tab_id,
                                     const GURL& url) {
    return Steps(WaitForBrowserActive(), AddInstrumentedTab(tab_id, url),
                 WaitForWebContentsReady(tab_id),
                 FocusOmniboxAndWaitForPopupReady());
  }

  // Instruments the initial tab (index 0), which starts on the NTP, and focuses
  // the omnibox.
  auto FocusOmniboxOnInitialNtp(ui::ElementIdentifier tab_id) {
    return Steps(WaitForBrowserActive(), InstrumentTab(tab_id, 0),
                 WaitForWebContentsReady(tab_id),
                 FocusOmniboxAndWaitForPopupReady());
  }

  // The browser opened by `OpenAndActivateSecondBrowserWindow()`, if any. Owned
  // by the browser list.
  raw_ptr<BrowserWindowInterface> second_browser_ = nullptr;
};

class FullWebUIOmniboxInteractiveTest
    : public FullWebUIOmniboxInteractiveTestBase,
      public testing::WithParamInterface<bool> {
 public:
  FullWebUIOmniboxInteractiveTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        omnibox::internal::kWebUIOmniboxFullPopup};
    std::vector<base::test::FeatureRef> disabled_features = {
        omnibox::internal::kWebUIOmniboxPopup};
    if (IsWebUIToolbarEnabled()) {
      enabled_features.push_back(features::kInitialWebUI);
      enabled_features.push_back(features::kWebUIToolbar);
    } else {
      disabled_features.push_back(features::kWebUIToolbar);
      disabled_features.push_back(features::kWebUILocationBar);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~FullWebUIOmniboxInteractiveTest() override = default;

  bool IsWebUIToolbarEnabled() const { return GetParam(); }

 protected:
  FindBar* GetFindBar() {
    return FindBarController::From(browser())->find_bar();
  }

  // Presses Ctrl/Cmd+F and waits for the find bar to show with focus. The key
  // press goes to the popup, which holds activation while the omnibox is
  // focused, as a real one would. On macOS, the popup only redispatches an
  // unhandled key event to the browser if the event's window is key, so a key
  // press sent to the inactive browser window would be dropped. Opening the
  // find bar closes the popup unless a draft keeps it open.
  auto OpenFindBarFromKeyboard() {
    return Steps(
        InAnyContext(SendKeyPress(OmniboxPopupPresenter::kRoundedResultsFrame,
                                  ui::VKEY_F, ui::EF_PLATFORM_ACCELERATOR)
                         .SetMustRemainVisible(false)),
        PollUntil(
            [this]() {
              return GetFindBar()->IsFindBarVisible() &&
                     GetFindBar()->HasFocus();
            },
            "WaitForFindBarFocused"));
  }

  // Waits for the browser window, rather than the popup, to hold activation,
  // so that key presses reach its focused view.
  auto WaitForBrowserWidgetActive() {
    return PollUntil(
        [this]() {
          return BrowserView::GetBrowserViewForBrowser(browser())
              ->GetWidget()
              ->IsActive();
        },
        "WaitForBrowserWidgetActive");
  }

  auto CheckFindBarFocused() {
    return CheckResult([this]() { return GetFindBar()->HasFocus(); }, true,
                       "CheckFindBarFocused");
  }

  // Waits until the active tab's find session searches for `text` and has
  // found at least one match.
  auto WaitForFindMatch(const std::u16string& text) {
    return PollUntil(
        [this, text]() {
          auto* find_tab_helper = find_in_page::FindTabHelper::FromWebContents(
              browser()->tab_strip_model()->GetActiveWebContents());
          return find_tab_helper && find_tab_helper->find_text() == text &&
                 find_tab_helper->find_result().number_of_matches() > 0;
        },
        "WaitForFindMatch");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the WebUI Omnibox input element aligns with the native
// LocationBarView bounds.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       AlignmentMatchesLocationBar) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "el => !!el.shadowRoot.querySelector('#input')"),
      Do([this]() {
        auto* location_bar =
            BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBar();
        const gfx::Rect location_bar_bounds = location_bar->BoundsInScreen();
        auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
            location_bar->GetOmniboxPopupView());
        auto* webui = popup_view->presenter()->GetWebUIContent();
        content::EvalJsResult result =
            content::EvalJs(webui->GetWebContents(), R"(
              const r = document.querySelector('omnibox-full-app')
                  ?.shadowRoot?.querySelector('omnibox-popup-searchbox')
                  ?.shadowRoot?.querySelector('#input')?.getBoundingClientRect();
              [r.x, r.y, r.width, r.height];
            )");
        const auto& rect = result.ExtractList();
        ASSERT_EQ(rect.size(), 4u);
        const gfx::Point origin = webui->GetBoundsInScreen().origin();
        EXPECT_EQ(std::round(origin.x() + rect[0].GetDouble()),
                  location_bar_bounds.x());
        EXPECT_EQ(std::round(origin.y() + rect[1].GetDouble()),
                  location_bar_bounds.y());
        EXPECT_EQ(std::round(rect[2].GetDouble()), location_bar_bounds.width());
        EXPECT_EQ(std::round(rect[3].GetDouble()),
                  location_bar_bounds.height());
      }));
}

// Verifies the draft, selection range, and focus are restored after typing an
// uncommitted draft in one tab, switching away to another tab, and switching
// back to the original tab.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ActiveUncommittedDraft) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("hello world"),
      InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput, R"((el) => {
        el.setSelectionRange(2, 7);
        el.ownerDocument.dispatchEvent(new Event('selectionchange'));
      })")),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "2|7")),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      // Verify the WebUI input text is "hello world".
      WaitForWebUIInputValue("hello world"),
      // Verify the selection range is still (2, 7).
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "2|7")));
}

// Verifies that a custom selection range on a steady-state URL is preserved
// when switching away from the tab and returning to it.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       PermanentUrlSelectionRangePreservedAcrossTabSwitch) {
  RunTestSequence(
      // Open Tab 1 at chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Select a sub-range of the permanent URL without typing a draft.
      InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput, R"((el) => {
        el.setSelectionRange(9, 16);
        el.ownerDocument.dispatchEvent(new Event('selectionchange'));
      })")),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "9|16")),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      WaitForWebUIInputValue("chrome://version"),
      WaitForPopupTransitionLockout(),
      // Verify the sub-range on the permanent URL is still (9, 16).
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "9|16")));
}

// Verifies that a selection set on the native Views omnibox while the popup is
// closed is transferred correctly to the WebUI popup input when it opens, and
// that subsequent native selections update properly rather than reusing stale
// WebUI state.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       NativeSelectionTransfersToWebUIOnPopupOpen) {
  RunTestSequence(
      // Open Tab 1 at chrome://version/ and set a WebUI selection (9, 16).
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput, R"((el) => {
        el.setSelectionRange(9, 16);
        el.ownerDocument.dispatchEvent(new Event('selectionchange'));
      })")),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "9|16")),
      // Close the popup.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false), UninstrumentWebContents(kPopupWebView),
      // Set a different selection (0, 6) on the native view and open popup.
      Do([this]() {
        // Go through `LocationBar` rather than `LocationBarView`: with the
        // WebUI toolbar there is no `LocationBarView`, and
        // `BrowserView::GetLocationBarView()` returns null.
        auto* location_bar =
            BrowserWindow::FromBrowser(browser())->GetLocationBar();
        location_bar->GetOmniboxView()->SetSelectionBounds(gfx::Range(0, 6));
        if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/true);
        }
      }),
      // Verify popup opens and the WebUI input receives (0, 6) instead of (9,
      // 16).
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      WaitForWebUIInputValue("chrome://version"),
      WaitForPopupTransitionLockout(),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart}|${el.selectionEnd}`", "0|6")));
}

// Verifies that highlighting a match, switching tabs, and switching back
// preserves the highlighted match text.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, HighlightAndSwitchTab) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      // Wait for the first suggestion to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      // Send ArrowDown key to highlight a match.
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_DOWN, ui::EF_NONE)),
      // Verify the WebUI input text is "suggestion-1".
      WaitForWebUIInputValue("suggestion-1"),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      // Verify the WebUI input text is still "suggestion-1".
      WaitForWebUIInputValue("suggestion-1"));
}

// Verifies that clicking outside on the webpage body while an active user draft
// exists keeps the popup open while shifting focus to the webpage.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, ActiveUnfocusedDraft) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378976): With the WebUI toolbar, a "
                    "delayed focus request reopens the full popup after a "
                    "tab switch.";
  }
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("ffffff"),
      // Click on the webpage body of Tab 1 to blur the Omnibox.
      ClickWebPageBody(kTab1),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      // Verify popup remains open, but focus shifts to the webpage.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForVisibleOmniboxUnfocused(),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2),
      // Switch back to Tab 1.
      SwitchTab(kTabStripElementId, 1),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      // Verify the WebUI input text is "ffffff".
      WaitForWebUIInputValue("ffffff"),
      // Verify the WebUI input remains unfocused.
      WaitForVisibleOmniboxUnfocused());
}

// Verifies that after typing a draft and clicking outside (leaving the popup
// visible but unfocused), clicking back into the WebUI input and typing again
// reopens the results dropdown, and pressing Enter navigates to a search for
// the most recently typed text. All within a single tab.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       RetypeAfterUnfocusedDraftNavigatesToLatestQuery) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/571988849): With the WebUI toolbar, "
                    "MoveMouseTo's WaitForWebContentsPainted on the popup "
                    "times out.";
  }
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Type a draft and verify the results dropdown shows.
      InputWebUIText("ffffff"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      // Click on the webpage body to blur the Omnibox.
      ClickWebPageBody(kTab1),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      // Verify popup remains visible but unfocused.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForVisibleOmniboxUnfocused(),
      // Click back into the visible-but-unfocused WebUI input to refocus it.
      InAnyContext(MoveMouseTo(kPopupWebView, kWebUIInput)),
      InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                      ClickMouse()),
      WaitForOmniboxFocus(true),
      // Type a new query and verify the results dropdown shows again.
      InputWebUIText("hello"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      // Press Enter and verify navigation is a search for the latest query.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN, ui::EF_NONE),
      WaitForGoogleSearch(kTab1, {{"q", "hello"}}));
}

// Verifies that entering keyword mode by clicking the keyword chip on the NTP
// and then clicking outside on the webpage body keeps keyword mode in both the
// WebUI and the edit model, and keeps the popup open. Keyword mode is an
// uncommitted draft even though the user text is empty.
//
// Clicking the page both reactivates the browser window, which restores Views
// focus to the location bar (`OmniboxViewViews::SetFocus()`), and blurs the
// omnibox (`OmniboxPopupFullPresenter::DeactivatePopupAndKillFocus()`). Neither
// may exit keyword mode.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       KeywordModeViaChipClickSurvivesBlur) {
  AddKeywordSearchEngine(u"kw");
  RunTestSequence(
      // Open an NTP, whose permanent text is empty, and wait for the popup.
      WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab1, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab1), WaitForPopupReady(),
      // Type the keyword and wait for its chip on the default match.
      InputWebUIText("kw"),
      InAnyContext(
          WaitForElementToRender(kPopupWebView, kDefaultMatchKeywordChip)),
      // Click the chip to enter keyword mode.
      InSameContext(ClickElement(kPopupWebView, kDefaultMatchKeywordChip)),
      WaitForWebUIKeywordMode(true), WaitForEditModelKeywordSelected(true),
      // Click on the webpage body of Tab 1 to blur the Omnibox.
      ClickWebPageBody(kTab1), WaitForVisibleOmniboxUnfocused(),
      // Verify keyword mode is kept and the popup remains open.
      WaitForEditModelKeywordSelected(true), WaitForWebUIKeywordMode(true),
      WaitForPopupState(OmniboxPopupState::kFull),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that entering keyword mode via Tab also enters keyword mode in the
// edit model, and that clicking outside on the webpage body then keeps keyword
// mode in both the WebUI and the edit model, and keeps the popup open.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       KeywordModeViaTabSurvivesBlur) {
  AddKeywordSearchEngine(u"kw");
  RunTestSequence(
      // Open an NTP, whose permanent text is empty, and wait for the popup.
      WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab1, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab1), WaitForPopupReady(),
      // Type the keyword and wait for its chip on the default match.
      InputWebUIText("kw"),
      InAnyContext(
          WaitForElementToRender(kPopupWebView, kDefaultMatchKeywordChip)),
      // Press Tab to select the chip and enter keyword mode.
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_TAB, ui::EF_NONE)),
      WaitForWebUIKeywordMode(true),
      // The WebUI tells the edit model to enter keyword mode too.
      WaitForEditModelKeywordSelected(true),
      // Click on the webpage body of Tab 1 to blur the Omnibox.
      ClickWebPageBody(kTab1), WaitForVisibleOmniboxUnfocused(),
      // Verify keyword mode is kept and the popup remains open.
      WaitForEditModelKeywordSelected(true), WaitForWebUIKeywordMode(true),
      WaitForPopupState(OmniboxPopupState::kFull),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies focusing the omnibox without typing a draft, selecting a portion of
// the URL text, switching away to another tab, and switching back restores
// focus to the omnibox and preserves the selection range without opening
// the suggestions dropdown.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, FocusOnlyNtp) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox (popup opens, but no draft exists yet).
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      // Verify the suggestions dropdown is not visible when focused without
      // user input.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      // Select a portion of the text in the WebUI searchbox (e.g. "chrome").
      InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                               R"(el => {
                                    el.setSelectionRange(0, 6);
                                    el.dispatchEvent(new Event('select'));
                                    document.dispatchEvent(
                                        new Event('selectionchange'));
                                  })")),
      CheckWebUIInputSelection(0, 6),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      // Verify the native Omnibox displays the page's permanent URL (since
      // no draft).
      WaitForOmniboxText(u"chrome://version"),
      // Verify the partial selection range [0, 6] is preserved across tab
      // switch.
      CheckWebUIInputSelection(0, 6),
      // Verify suggestions dropdown remains closed after tab restoration.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"));
}

// Verifies switching to a tab where the webpage body is focused and has no
// omnibox draft to verify the popup remains closed.
// TODO(crbug.com/504668292): Failing on Windows, Linux, and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_BlurredPage DISABLED_BlurredPage
#else
#define MAYBE_BlurredPage BlurredPage
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, MAYBE_BlurredPage) {
  RunTestSequence(
      // Setup Tab 1 with open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Navigate to a normal webpage on Tab 2 (Omnibox is blurred, webpage
      // has focus).
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2), ClickWebPageBody(kTab2),
      WaitForOmniboxFocus(false), UninstrumentWebContents(kPopupWebView),
      // Switch to Tab 1 (focused).
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      // Switch back to Tab 2.
      UninstrumentWebContents(kPopupWebView), SwitchTab(kTabStripElementId, 2),
      FocusWebContents(kTab2),
      // Verify the native Omnibox displays the page's permanent URL.
      WaitForOmniboxText(u"chrome://version"),
      // Verify keyboard focus remains on the webpage body (Omnibox is
      // unfocused).
      WaitForOmniboxFocus(false),
      // Verify the WebUI popup is closed.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that clicking outside the popup on Tab 1, switching away to Tab 2,
// and switching back to Tab 1 keeps the Omnibox unfocused on Tab 1.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickOutsideThenSwitchTabsDoesNotRefocus) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      // Click outside on webpage body of Tab 1.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2),
      // Switch back to Tab 1.
      SwitchTab(kTabStripElementId, 1),
      // Verify Omnibox remains unfocused and popup remains hidden on Tab 1.
      CheckOmniboxFocus(false),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies clearing omnibox then manual blurring.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, ClearAndManualBlur) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Type "ffffff" into the WebUI.
      InputWebUIText("ffffff"),
      // Clear the input text.
      ClearWebUIText(),
      // Verify the native Omnibox text is also empty.
      WaitForOmniboxText(u""),
      // Click the webpage body (triggering blur).
      ClickWebPageBody(kTab1),
      // Verify WebUI Omnibox popup is closed, uncommitted cleared draft remains
      // empty, and Omnibox is unfocused.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), FocusWebContents(kTab1),
      WaitForOmniboxText(u""), WaitForOmniboxFocus(false),
      // Focus the Omnibox.
      Do([this]() {
        if (auto* popup_view = BrowserWindow::FromBrowser(browser())
                                   ->GetLocationBar()
                                   ->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/true);
        }
      }),
      // Verify popup is open, and WebUI input is empty.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      WaitForJsConditionAt(kPopupWebView, kWebUIInput,
                           "(el) => el && el.value === ''"),
      WaitForOmniboxFocus(true),
      // Unfocus the Omnibox.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), FocusWebContents(kTab1),
      WaitForOmniboxFocus(false),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2),
      // Switch back to Tab 1.
      SwitchTab(kTabStripElementId, 1), FocusWebContents(kTab1),
      // Verify the WebUI popup remains closed.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxText(u"chrome://version"), WaitForOmniboxFocus(false));
}

// Verifies that after typing a draft and clearing the input in one tab,
// switching away to another tab and switching back reverts the empty draft
// to restore the page's permanent URL (`chrome://version`).
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, ClearAndSwitchTab) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Type "ffffff" into the WebUI.
      InputWebUIText("ffffff"),
      // Clear the input text (triggers OnInputCleared).
      ClearWebUIText(),
      // Verify the native Omnibox text is empty.
      WaitForOmniboxText(u""),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1),
      // Verify that SaveStateToTab reverted the cleared draft, restoring the
      // permanent URL of Tab 1 instead of an empty string.
      WaitForOmniboxText(u"chrome://version"));
}

// Verifies that opening multiple New Tab Pages (NTPs) consecutively focuses the
// WebUI Omnibox input right away for each new tab, and that non-NTPs do not
// have lingering focus.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       OmniboxFocusDoesNotLingerAcrossTabs) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378976): With the WebUI toolbar, a "
                    "delayed focus request reopens the full popup after a "
                    "tab switch.";
  }
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab3);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab4);

  RunTestSequence(
      // Tab 1 is the initial NTP.
      InstrumentTab(kTab1, 0), WaitForWebContentsReady(kTab1),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure omnibox is focused.
      WaitForOmniboxFocus(true),

      // Open NTP Tab 2.
      UninstrumentWebContents(kPopupWebView), WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure omnibox is focused.
      WaitForOmniboxFocus(true),

      // Open non-NTP, expect focus to not linger.
      UninstrumentWebContents(kPopupWebView), WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab3, GURL("about:blank")),
      WaitForWebContentsReady(kTab3),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      // Ensure omnibox is not focused.
      WaitForOmniboxFocus(false),

      // Open a third NTP.
      WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab4, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab4),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure omnibox is focused.
      WaitForOmniboxFocus(true));
}

// Verifies that reloading the page while the Full WebUI Omnibox popup is open
// hides the popup cleanly without crashing or triggering DCHECK failures.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ReloadPageWhileOmniboxIsOpen) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Click the reload button while the popup is active and visible.
      PressButton(kReloadButtonElementId),
      // Verify that the popup hides cleanly without crashes.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that clicking a match navigates to the suggestion.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, ClickMatch) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      // Wait for the first suggestion to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      InAnyContext(
          WaitForElementToRender(kPopupWebView, kFirstSuggestionMatch)),
      // Click the first suggestion. This uses a JS click because
      // `ClickElement()` first waits for the popup's first paint event, and
      // that wait flakily times out (b/567193967). The click navigates and
      // hides the popup, so don't wait for the script to finish.
      InAnyContext(ExecuteJsAt(kPopupWebView, kFirstSuggestionMatch,
                               "el => el.click()",
                               ExecuteJsMode::kFireAndForget)),
      // Verify navigation occurs.
      WaitForGoogleSearch(kTab1, {{"q", "suggestion-1"}}));
}

// Verifies ESC key staged unwinding parity across all 4 stages:
// Stage 1: Revert temporary text (kRevertTemporaryText)
// Stage 2: Close open suggestion popup (kClosePopup)
// Stage 3: Clear user input / revert to active page URL (kClearUserInput)
// Stage 4: Clear focus / blur Omnibox (kBlur)
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, EscapeStagedUnwinding) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378107): With the WebUI toolbar, the "
                    "omnibox keeps focus after the full popup closes.";
  }
  base::HistogramTester histogram_tester;

  RunTestSequence(
      // Open Tab 1 at permanent URL chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Replace the permanent URL with "a" and select the first suggestion.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_DOWN, ui::EF_NONE)),
      WaitForWebUIInputValue("suggestion-1"), CheckWebUIInputFocus(true),

      // Stage 1: Send ESC to revert temporary text back to "a".
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForWebUIInputValue("a"),
      // Verify popup frame remains visible.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount(
            "Omnibox.Escape", /*sample=*/1 /* kRevertTemporaryText */, 1);
      }),

      // Stage 2: Send ESC to close open suggestion popup while retaining typed
      // text.
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForWebUIInputValue("a"),
      // Verify dropdown suggestion list is no longer visible.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      WaitForOmniboxFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/2 /* kClosePopup */, 1);
      }),

      // Stage 3: Send ESC to clear user input draft and restore permanent page
      // URL.
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      // Wait for input value to revert to permanent page URL.
      WaitForWebUIInputValue("chrome://version"), WaitForOmniboxFocus(true),
      Do([&]() {
        histogram_tester.ExpectBucketCount(
            "Omnibox.Escape", /*sample=*/3 /* kClearUserInput */, 1);
      }),

      // Stage 4: Send ESC to blur Omnibox focus.
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      // Wait for popup frame to hide and Omnibox to lose focus.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/5 /* kBlur */, 1);
      }));
}

// Verifies ESC key Stage 3 clears user input and closes the popup UI when
// the permanent URL is empty (on NTP) and input is empty.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       EscapeStagedUnwinding_EmptyPermanentUrl) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378107): With the WebUI toolbar, the "
                    "omnibox keeps focus after the full popup closes.";
  }
  base::HistogramTester histogram_tester;

  RunTestSequence(
      // Focus the Omnibox on the initial NTP tab (empty permanent URL).
      FocusOmniboxOnInitialNtp(kTab1),
      // Type "a" into the WebUI input field.
      InputWebUIText("a"),
      // Wait for suggestion-1 match to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),

      // Stage 2: Send ESC to close open suggestion popup while retaining typed
      // text "a".
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      WaitForOmniboxFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/2 /* kClosePopup */, 1);
      }),

      // Stage 3: Send ESC to clear typed draft "a" and restore empty URL (NTP).
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForWebUIInputValue(""), WaitForOmniboxFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount(
            "Omnibox.Escape", /*sample=*/3 /* kClearUserInput */, 1);
      }),

      // Stage 4: Send ESC on clean input to blur Omnibox and close UI.
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      // Wait for popup frame to hide and Omnibox to lose focus.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/5 /* kBlur */, 1);
      }));
}

// Verifies that switching away from a tab with an active draft and returning to
// it restores the draft text in the searchbox without reopening the suggestion
// dropdown.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       TabSwitchDoesNotReopenDropdown) {
  RunTestSequence(
      // 1. Open Tab 1 at chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // 2. Type "a" into the WebUI input to open suggestions.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      // 3. Open Tab 2 and switch to it (index 2 because the browser starts off
      // with a tab before we added one in the first step).
      AddInstrumentedTab(kTab2, GURL("chrome://about/")),
      SelectTab(kTabStripElementId, 2),
      // 4. Switch back to Tab 1 (index 1).
      SelectTab(kTabStripElementId, 1),
      // 5. Verify the restored draft text "a" is present in the searchbox
      // input.
      WaitForJsConditionAt(kPopupWebView, kWebUIInput,
                           "(el) => el && el.value === 'a'"),
      // 6. Verify that the suggestion dropdown remains closed on tab
      // restoration.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"));
}

// Verifies that switching between two tabs that both show the popup keeps it on
// screen for the whole switch. Restoring the newly active tab's state reverts
// the omnibox, which used to close the popup before `OnTabChanged()` applied
// the target state; hiding tears the popup widget down, so the user saw the
// popup flicker on every tab switch.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, TabSwitchKeepsPopup) {
  std::vector<OmniboxPopupState> observed_states;
  base::CallbackListSubscription subscription;
  auto* const popup_state_manager = BrowserWindow::FromBrowser(browser())
                                        ->GetLocationBar()
                                        ->GetOmniboxController()
                                        ->popup_state_manager();

  RunTestSequence(
      // Open Tab 1 at chrome://version/ and focus the Omnibox, which shows the
      // popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Open Tab 2 and focus its Omnibox too, so the popup is shown on both
      // tabs.
      AddInstrumentedTab(kTab2, GURL("chrome://about/")),
      WaitForWebContentsReady(kTab2), WaitForPopupTransitionLockout(),
      Do([this]() {
        BrowserWindow::FromBrowser(browser())
            ->GetLocationBar()
            ->GetOmniboxPopupView()
            ->OnFocus(/*query_zps=*/true);
      }),
      WaitForPopupTransitionLockout(),
      // Start recording popup state changes.
      Do([&]() {
        subscription = popup_state_manager->AddPopupStateChangedCallback(
            base::BindLambdaForTesting(
                [&observed_states](OmniboxPopupState /*old_state*/,
                                   OmniboxPopupState new_state) {
                  observed_states.push_back(new_state);
                }));
      }),
      // Switch back to Tab 1 (index 1, since the browser starts off with a tab
      // before the two added above).
      SwitchTab(kTabStripElementId, 1),
      // The popup is shown on both tabs, so the switch must not have moved
      // through `kNone` and hidden it.
      CheckResult([&]() { return observed_states; },
                  std::vector<OmniboxPopupState>(), "PopupStateChanges"),
      CheckPopupState(OmniboxPopupState::kFull),
      CheckResult(
          [this]() {
            auto* popup_view = BrowserWindow::FromBrowser(browser())
                                   ->GetLocationBar()
                                   ->GetOmniboxPopupView();
            return popup_view->presenter() &&
                   popup_view->presenter()->IsShown();
          },
          true, "PopupIsShown"));
}

// Verifies that opening a new tab (which auto-focuses the omnibox) keeps the
// popup on screen without closing or recreating it. Restoring display texts
// when navigation commits used to revert the omnibox and close the popup to
// kNone, destroying the popup widget and causing a visible flicker.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, NewTabKeepsPopup) {
  std::vector<OmniboxPopupState> observed_states;
  base::CallbackListSubscription subscription;
  auto* const popup_state_manager = BrowserWindow::FromBrowser(browser())
                                        ->GetLocationBar()
                                        ->GetOmniboxController()
                                        ->popup_state_manager();

  RunTestSequence(
      // Open Tab 1 at chrome://version/ and focus the Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Start recording popup state changes before opening a new tab.
      Do([&]() {
        subscription = popup_state_manager->AddPopupStateChangedCallback(
            base::BindLambdaForTesting(
                [&observed_states](OmniboxPopupState /*old_state*/,
                                   OmniboxPopupState new_state) {
                  observed_states.push_back(new_state);
                }));
      }),
      // Open a new tab (NTP), which auto-focuses the omnibox.
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2),
      // Ensure that throughout opening the new tab, the popup state did not
      // transition through `kNone` and hide it.
      CheckResult(
          [&]() {
            return std::ranges::find(observed_states,
                                     OmniboxPopupState::kNone) !=
                   observed_states.end();
          },
          false, "NoTransitionToNone"),
      CheckPopupState(OmniboxPopupState::kFull),
      CheckResult(
          [this]() {
            auto* popup_view = BrowserWindow::FromBrowser(browser())
                                   ->GetLocationBar()
                                   ->GetOmniboxPopupView();
            return popup_view->presenter() &&
                   popup_view->presenter()->IsShown();
          },
          true, "PopupIsShown"));
}

// Verifies that clicking a bookmark button in the bookmarks bar situated
// directly below the Omnibox while the Full WebUI Omnibox popup is open
// and focused dismisses the popup and navigates to the bookmarked URL.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickBookmarksBarWhenOmniboxFocused) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378107): With the WebUI toolbar, the "
                    "omnibox keeps focus after the full popup closes.";
  }
  // Disable slide animations and ensure the bookmarks bar is always visible.
  BookmarkBarView::DisableAnimationsForTesting(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowBookmarkBar, true);
  browser()->GetProfile()->GetPrefs()->SetInteger(
      bookmarks::prefs::kBookmarkBarVisibilityState,
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
  // Populate the bookmark model with a test URL.
  auto* const model =
      BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const GURL bookmark_url("chrome://version/");
  const std::u16string bookmark_title = u"TestBookmark";
  model->AddNewURL(model->bookmark_bar_node(), 0, bookmark_title, bookmark_url);
  // Track the dynamic bookmark button and preserve the browser window context
  // (since focusing the omnibox switches Kombucha's active context to the
  // popup).
  constexpr char kBookmarkButtonName[] = "BookmarkButton";
  const ui::ElementContext browser_context =
      BrowserView::GetBrowserViewForBrowser(browser())->GetElementContext();

  RunTestSequence(
      // Open initial tab and verify Full WebUI Omnibox is open and focused.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("about:blank")),
      // Ensure bookmarks bar is visible and name the target bookmark button.
      InContext(browser_context, WaitForShow(kBookmarkBarElementId)),
      InContext(
          browser_context,
          NameViewRelative(
              kBookmarkBarElementId, kBookmarkButtonName,
              base::BindLambdaForTesting(
                  [bookmark_title](views::View* view) -> views::View* {
                    auto* const bookmark_bar =
                        views::AsViewClass<BookmarkBarView>(view);
                    if (!bookmark_bar) {
                      return nullptr;
                    }
                    for (views::View* child : bookmark_bar->children()) {
                      if (auto* button =
                              views::AsViewClass<views::LabelButton>(child)) {
                        if (button->GetText() == bookmark_title) {
                          return button;
                        }
                      }
                    }
                    return nullptr;
                  }))),
      // Click the bookmark button situated directly beneath the Omnibox.
      InContext(browser_context, MoveMouseTo(kBookmarkButtonName)),
      InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                      ClickMouse()),
      // Verify the popup closes, navigation occurs, and Omnibox loses focus.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InContext(browser_context,
                WaitForWebContentsNavigation(kTab1, bookmark_url)),
      WaitForOmniboxFocus(false));
  // Reset the process-wide animation state for subsequent tests.
  BookmarkBarView::DisableAnimationsForTesting(false);
}

// Verifies that pasting text into the full WebUI Omnibox records the
// Omnibox.Paste metric and opens autocomplete suggestions.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, OnPaste) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      ClearWebUIText(), PasteWebUIText("example.com"),
      WaitForWebUIInputValue("example.com"), CheckWebUIInputFocus(true),
      Do([&]() { histogram_tester.ExpectBucketCount("Omnibox.Paste", 1, 1); }));
}

// Verifies that clicking on the webpage content area closes the popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickWebpageClosesPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      ClearWebUIText(), ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that clicking the location bar keeps the popup open.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickLocationBarKeepsPopupOpen) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that clicking on top chrome when the omnibox has draft text keeps
// omnibox focus and preserves the cursor / selection position rather than
// selecting all text, and that typing afterwards still goes into the omnibox
// at that position.
// TODO(b/567944511): Enable on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ClickTopChromeWithDraftPreservesCursorPosition \
  DISABLED_ClickTopChromeWithDraftPreservesCursorPosition
#else
#define MAYBE_ClickTopChromeWithDraftPreservesCursorPosition \
  ClickTopChromeWithDraftPreservesCursorPosition
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       MAYBE_ClickTopChromeWithDraftPreservesCursorPosition) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(crbug.com/564919981): Clicking top chrome doesn't "
                    "restore selection.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      ClearWebUIText(), InputWebUIText("example text"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(ExecuteJsAt(
          kPopupWebView, kWebUIInput,
          "el => { el.setSelectionRange(7, 7); "
          "document.dispatchEvent(new Event('selectionchange')); }")),
      CheckWebUIInputSelection(7, 7), ClickTabStrip(),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      CheckWebUIInputSelection(7, 7), WaitForOmniboxFocus(true),
      // Send a real key press to the browser window, which is where the OS
      // sends it after a click on the window.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_X, ui::EF_NONE),
      WaitForWebUIInputValue("examplex text"));
}

// Verifies that switching to another browser window dismisses the popup and
// reactivating the original window restores omnibox focus and selection.
// TODO(b/567944511): Enable on Windows, Linux, and ChromeOS once omnibox focus
// is restored on window reactivation there. See
// `OmniboxPopupFullPresenter::BlurForWindowDeactivation()`.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReactivatingWindowRestoresOmniboxFocus \
  ReactivatingWindowRestoresOmniboxFocus
#else
#define MAYBE_ReactivatingWindowRestoresOmniboxFocus \
  DISABLED_ReactivatingWindowRestoresOmniboxFocus
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       MAYBE_ReactivatingWindowRestoresOmniboxFocus) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP()
        << "TODO(crbug.com/567926983): With the WebUI toolbar, the location "
           "bar's focus restore view is the toolbar WebView. Restoring focus "
           "to it on window reactivation gives the edit model focus but "
           "doesn't reactivate the popup widget, so the popup's input never "
           "gets document focus.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), WaitForOmniboxFocus(true),
      CheckWebUIInputSelection(0, 16), OpenAndActivateSecondBrowserWindow(),
      WaitForPopupDismissed(),
      // Reactivate the original browser window.
      ReactivateBrowserWindow(),
      // Omnibox logical and WebUI input focus should be restored, and selection
      // range should remain preserved.
      WaitForOmniboxFocus(true), CheckWebUIInputSelection(0, 16));
}

// Verifies that switching to another browser window dismisses the popup, and
// reactivating the original window focuses the web contents without reopening
// the popup.
// TODO(b/567944511): Replace with `ReactivatingWindowRestoresOmniboxFocus` once
// omnibox focus is restored on window reactivation on Windows, Linux, and
// ChromeOS.
#if !BUILDFLAG(IS_MAC)
// TODO(crbug.com/569788689): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ReactivatingWindowFocusesWebContents \
  DISABLED_ReactivatingWindowFocusesWebContents
#else
#define MAYBE_ReactivatingWindowFocusesWebContents \
  ReactivatingWindowFocusesWebContents
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       MAYBE_ReactivatingWindowFocusesWebContents) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), WaitForOmniboxFocus(true),
      OpenAndActivateSecondBrowserWindow(), WaitForPopupDismissed(),
      WaitForPopupState(OmniboxPopupState::kNone),
      // Reactivate the original browser window.
      ReactivateBrowserWindow(),
      // Focus should be restored to the web contents rather than the omnibox.
      PollUntil(
          [this]() {
            auto* browser_view =
                BrowserView::GetBrowserViewForBrowser(browser());
            if (!browser_view || !browser_view->GetFocusManager()) {
              return false;
            }
            auto* focused_view =
                browser_view->GetFocusManager()->GetFocusedView();
            return focused_view &&
                   focused_view == browser_view->GetActiveContentsWebView();
          },
          "WaitForWebContentsFocused"),
      // The popup must stay closed and unfocused.
      CheckPopupState(OmniboxPopupState::kNone), CheckOmniboxFocus(false));
}
#endif  // !BUILDFLAG(IS_MAC)

// Verifies that reactivating a browser window with an unfocused draft keeps the
// draft visible without stealing focus back to the omnibox.
// TODO(crbug.com/569788689): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ReactivatingWindowWithUnfocusedDraftDoesNotStealFocus \
  DISABLED_ReactivatingWindowWithUnfocusedDraftDoesNotStealFocus
#else
#define MAYBE_ReactivatingWindowWithUnfocusedDraftDoesNotStealFocus \
  ReactivatingWindowWithUnfocusedDraftDoesNotStealFocus
#endif
IN_PROC_BROWSER_TEST_P(
    FullWebUIOmniboxInteractiveTest,
    MAYBE_ReactivatingWindowWithUnfocusedDraftDoesNotStealFocus) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      ClearWebUIText(), InputWebUIText("example text"),
      // Blur the omnibox by clicking the webpage body.
      ClickWebPageBody(kTab1),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForVisibleOmniboxUnfocused(),
      // Deactivate the browser window by activating a second browser window.
      OpenAndActivateSecondBrowserWindow(),
      // Reactivate the original browser window.
      ReactivateBrowserWindow(),
      // Omnibox draft should remain visible, but must remain unfocused. Focus
      // must not be stolen back from the webpage.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForWebUIInputValue("example text"), CheckVisibleOmniboxUnfocused());
}

// Verifies that reactivating the popup widget directly restores omnibox focus.
// Enabled only on macOS, the only platform that restores native activation
// directly to the popup's child window.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReactivatingPopupWidgetRestoresOmniboxFocus \
  ReactivatingPopupWidgetRestoresOmniboxFocus
#else
#define MAYBE_ReactivatingPopupWidgetRestoresOmniboxFocus \
  DISABLED_ReactivatingPopupWidgetRestoresOmniboxFocus
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       MAYBE_ReactivatingPopupWidgetRestoresOmniboxFocus) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      ClearWebUIText(), InputWebUIText("example text"),
      WaitForOmniboxFocus(true), OpenAndActivateSecondBrowserWindow(),
      WaitForVisibleOmniboxUnfocused(),
      // Reactivate directly via the popup widget.
      ReactivatePopupWidget(),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForWebUIInputValue("example text"), WaitForOmniboxFocus(true));
}

// Verifies that clicking the tab strip of an inactive browser window with an
// uncommitted draft activates the window and restores omnibox focus.
// Disabled on Linux because Xvfb has no window manager, so clicks don't
// activate windows.
// TODO(b/552482504): Fix this test on Windows.
// TODO(b/567944511): Fix this test on ChromeOS, where clicking the tab strip
// of the reactivated window doesn't restore omnibox focus.
// TODO(crbug.com/569045384): Fix this test on Mac.
IN_PROC_BROWSER_TEST_P(
    FullWebUIOmniboxInteractiveTest,
    DISABLED_ClickInactiveWindowWithDraftRestoresOmniboxFocus) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP()
        << "TODO(crbug.com/567926983): With the WebUI toolbar, the location "
           "bar's focus restore view is the toolbar WebView. Restoring focus "
           "to it on window reactivation gives the edit model focus but "
           "doesn't reactivate the popup widget, so the popup's input never "
           "gets document focus.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      ClearWebUIText(), InputWebUIText("example text"),
      WaitForWebUIInputValue("example text"), WaitForOmniboxFocus(true),
      OpenAndActivateSecondBrowserWindow(), WaitForVisibleOmniboxUnfocused(),
      // Click the tab strip area of the inactive first window to activate it.
      // Wait on polled state first, since the popup widget can be replaced
      // while the window reactivates.
      ClickTabStrip(), WaitForOmniboxFocus(true),
      WaitForWebUIInputValue("example text"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

#if BUILDFLAG(IS_MAC)
// Verifies that a window deactivated while a tab switch restores omnibox focus
// (e.g. when a tab is dragged out of it) doesn't take activation back, and that
// reactivating it restores omnibox focus. Only macOS Views doesn't activate
// windows on focus changes.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       WindowSwitchDuringTabSwitchDoesNotStealActivation) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/571988349): With the WebUI toolbar, "
                    "reactivating the browser window does not reopen the "
                    "popup.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForOmniboxFocus(true), OpenAndActivateSecondBrowserWindow(),
      WaitForPopupDismissed(), ReactivateBrowserWindow(), WaitForPopupActive(),
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2), WaitForPopupDismissed(),
      PollUntil([this]() { return IsBrowserOrPopupActive(browser()); },
                "WaitForBrowserActive"),
      // Activate the second window before the popup's deferred focus request
      // for tab 1 runs.
      Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(1);
        BrowserView::GetBrowserViewForBrowser(second_browser_)->Activate();
      }),
      WaitForPopupTransitionLockout(),
      CheckResult([this]() { return IsBrowserOrPopupActive(second_browser_); },
                  true, "SecondBrowserStaysActive"),
      WaitForPopupDismissed(), ReactivateBrowserWindow(), WaitForPopupActive(),
      WaitForOmniboxFocus(true));
}

// Verifies that a single click on the web page blurs the omnibox in fullscreen
// and after leaving fullscreen with the omnibox focused. Only macOS moves the
// location bar into an overlay widget in fullscreen.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickWebPageInFullscreenDismissesPopup) {
  RunTestSequence(
      WaitForBrowserActive(),
      AddInstrumentedTab(kTab1, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab1),
      Do([this]() { ui_test_utils::ToggleFullscreenModeAndWait(browser()); }),
      WaitForPopupTransitionLockout(),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_L,
                   ui::EF_PLATFORM_ACCELERATOR),
      WaitForPopupReady(), WaitForPopupActive(), ClickWebPageBody(kTab1),
      WaitForPopupDismissed(), WaitForPopupTransitionLockout(),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_L,
                   ui::EF_PLATFORM_ACCELERATOR),
      WaitForPopupActive(),
      Do([this]() { ui_test_utils::ToggleFullscreenModeAndWait(browser()); }),
      WaitForPopupActive(), ClickWebPageBody(kTab1), WaitForPopupDismissed());
}
#endif  // BUILDFLAG(IS_MAC)

// Verifies that while the popup is collapsed (no dropdown), it does not extend
// below the location bar, so clicking just beneath it reaches the browser
// window and dismisses the popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickBelowCollapsedPopupDismissesPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), CheckWebUIInputFocus(true),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      ClickJustBelowCollapsedPopup(), WaitForPopupDismissed());
}

#if defined(USE_AURA)
// Verifies that when the dropdown is expanded, clicking the popup's
// transparent bottom shadow margin (which the window targeter passes through
// to the browser window) dismisses the popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickExpandedPopupShadowMarginDismissesPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), CheckWebUIInputFocus(true),
      // Type the page's permanent URL so the dropdown expands with matches
      // while clicking outside still reverts and dismisses the popup.
      InputWebUIText("chrome://version"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      // Wait for the widget to grow and regain its bottom shadow margin.
      PollUntil(
          [this]() {
            views::Widget* widget = GetPopupWidget(browser());
            auto* location_bar =
                BrowserWindow::FromBrowser(browser())->GetLocationBar();
            if (!widget || !location_bar) {
              return false;
            }
            const int collapsed_widget_height =
                location_bar->BoundsInScreen().height() +
                FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets()
                    .height() +
                RoundedOmniboxResultsFrame::GetShadowInsets().top();
            return widget->GetWindowBoundsInScreen().height() >
                   collapsed_widget_height;
          },
          "WaitForExpandedPopupWidgetBounds"),
      ClickPopupShadowMargin(), WaitForPopupDismissed());
}
#endif  // defined(USE_AURA)

// Verifies that switching to another window while a bubble anchored to the
// browser window is active dismisses the popup and blurs the omnibox. Neither
// the browser widget nor the popup widget is active when the window switch
// happens, so neither gets an activation change, and only the browser's
// paint-as-active change can trigger the blur.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       WindowSwitchWhileBubbleActiveDismissesPopup) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP() << "TODO(b/571715601): On Wayland, activating the bubble "
                    "already blurs the omnibox, so the test's precondition "
                    "doesn't hold.";
  }
#endif
  // The bubble delegate must outlive the bubble widget, so declare it first.
  std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate;
  std::unique_ptr<views::Widget> bubble_widget;

  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForOmniboxFocus(true), Do([&]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        ASSERT_TRUE(browser_view);
        // With the WebUI toolbar there is no `LocationBarView`, so anchor to
        // the top container, which is also in the browser widget.
        views::View* anchor = browser_view->GetLocationBarView();
        if (!anchor) {
          anchor = browser_view->top_container();
        }
        ASSERT_TRUE(anchor);
        bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
            anchor, views::BubbleBorder::TOP_LEFT);
        // Keep the bubble open when the other window is activated, so that its
        // closing doesn't change activation.
        bubble_delegate->set_close_on_deactivate(false);
        auto label_button = std::make_unique<views::LabelButton>(
            views::Button::PressedCallback(), u"Test Bubble");
        label_button->SetPreferredSize(gfx::Size(100, 30));
        bubble_delegate->SetContentsView(std::move(label_button));
        bubble_widget =
            views::BubbleDialogDelegate::CreateBubble(bubble_delegate.get());
        bubble_widget->Show();
      }),
      PollUntil([&]() { return bubble_widget && bubble_widget->IsActive(); },
                "WaitForBubbleActive"),
      // Let any checks posted by the popup losing activation run.
      WaitForPopupTransitionLockout(),
      // Precondition: activating the bubble must not blur the omnibox, and
      // must leave neither the browser widget nor the popup widget active.
      CheckEditModelFocus(true), CheckPopupState(OmniboxPopupState::kFull),
      CheckResult([this]() { return IsBrowserOrPopupActive(browser()); }, false,
                  "BrowserAndPopupInactiveWhileBubbleActive"),
      OpenAndActivateSecondBrowserWindow(), WaitForPopupDismissed(),
      WaitForEditModelFocus(false));
}

// Verifies that focusing the native Omnibox with an active selection range
// (e.g. from double-clicking or dragging in Views) preserves the exact
// selection bounds when synchronizing state to the WebUI popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ViewsSelectionPreservedOnInitialHandoff) {
  RunTestSequence(
      WaitForBrowserActive(),
      AddInstrumentedTab(kTab1, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab1), WaitForPopupTransitionLockout(),
      Do([this]() {
        // Go through `LocationBar` rather than `LocationBarView`: with the
        // WebUI toolbar there is no `LocationBarView`, and
        // `BrowserView::GetLocationBarView()` returns null.
        auto* location_bar =
            BrowserWindow::FromBrowser(browser())->GetLocationBar();
        auto* omnibox_view = location_bar->GetOmniboxView();
        ASSERT_TRUE(omnibox_view);
        // Set word selection range [0, 6] ("chrome").
        omnibox_view->SetSelectionBounds(gfx::Range(0, 6));
        if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/false);
        }
      }),
      WaitForPopupReady(), WaitForWebUIInputValue("chrome://version"),
      CheckWebUIInputSelection(0, 6));
}

// Verifies that opening the App Menu from both empty and typed Omnibox states
// opens the menu (without ZPS collision dismissing it) and closes suggestions,
// and closing the App Menu restores keyboard focus and typing ability to the
// Full WebUI Omnibox.
// TODO(b/552482504): Fix this test.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       DISABLED_AppMenuOpenAndCloseLifecycle) {
  const ui::ElementContext browser_context =
      BrowserView::GetBrowserViewForBrowser(browser())->GetElementContext();

  RunTestSequence(
      // --- Part 1: Empty Omnibox / NTP (verifies no ZPS collision) ---
      FocusOmniboxOnInitialNtp(kTab1),

      // Open App Menu from empty state.
      InContext(browser_context, MoveMouseTo(kToolbarAppMenuButtonElementId)),
      InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                      ClickMouse()),
      InAnyContext(WaitForShow(AppMenuModel::kMoreToolsMenuItem)),

      // Close App Menu via Escape and verify focus restoration.
      InAnyContext(
          SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(AppMenuModel::kMoreToolsMenuItem)),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(true),

      // --- Part 2: Active Draft & Suggestions (verifies dropdown hiding &
      // typing restoration) ---
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),

      // Re-open App Menu while suggestions are showing.
      InContext(browser_context, MoveMouseTo(kToolbarAppMenuButtonElementId)),
      InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                      ClickMouse()),
      InAnyContext(WaitForShow(AppMenuModel::kMoreToolsMenuItem)),

      // Suggestions should collapse while rounded results frame remains
      // visible.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),

      // Close App Menu again and verify we can continue typing.
      InAnyContext(
          SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(AppMenuModel::kMoreToolsMenuItem)),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(true), InputWebUIText("ab"),
      WaitForWebUIInputValue("ab"));
}

// Verifies that dragging the mouse across the WebUI Omnibox input
// produces a non-empty text selection highlight.
// TODO(b/552482504): Fix this test.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       DISABLED_MouseDragHighlightsInputText) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("dragandhighlight"),
      WaitForWebUIInputValue("dragandhighlight"),
      // Move native mouse to the start of the input text within the popup.
      InAnyContext(
          MoveMouseTo(kPopupWebView, base::BindOnce([](ui::TrackedElement* el) {
                        gfx::Rect bounds = el->GetScreenBounds();
                        return gfx::Point(bounds.x() + 65, bounds.y() + 24);
                      }))),
      // Drag mouse to the right across the input text.
      InSameContext(DragMouseTo(base::BindOnce([]() -> gfx::Point {
        return display::Screen::Get()->GetCursorScreenPoint() +
               gfx::Vector2d(100, 0);
      }))),
      // Verify that non-empty text selection was created.
      InAnyContext(WaitForJsConditionAt(
          kPopupWebView, kWebUIInput,
          "(el) => el && Math.abs(el.selectionEnd - el.selectionStart) > 0")),
      // Verify input value remains intact.
      WaitForWebUIInputValue("dragandhighlight"));
}

#if !BUILDFLAG(IS_MAC)
// Verifies that pressing Shift+Arrow keys after Select All adjusts the
// selection character-by-character on Windows and Linux rather than collapsing
// all text at once. On macOS, selections created by Select All are undirected
// by OS convention and do not adjust character-by-character.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ShiftArrowSelectionModification) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("hello"), CheckWebUIInputFocus(true),
      SelectAllWebUIInput(), CheckWebUIInputSelection(0, 5),
      // Shift+ArrowRight at the end of selection should stay at [0, 5].
      InAnyContext(
          SendKeyPress(kPopupWebView, ui::VKEY_RIGHT, ui::EF_SHIFT_DOWN)),
      CheckWebUIInputSelection(0, 5),
      // Shift+ArrowLeft should unselect the last character [0, 4].
      InAnyContext(
          SendKeyPress(kPopupWebView, ui::VKEY_LEFT, ui::EF_SHIFT_DOWN)),
      CheckWebUIInputSelection(0, 4),
      // Without Shift, ArrowLeft should collapse selection to the beginning
      // [0, 0].
      SelectAllWebUIInput(), CheckWebUIInputSelection(0, 5),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_LEFT, ui::EF_NONE)),
      CheckWebUIInputSelection(0, 0));
}
#endif  // !BUILDFLAG(IS_MAC)

// Verifies that opening a suggestion in a new foreground tab via Alt+Enter
// leaves the Omnibox on the new tab unfocused and the popup closed.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       AltEnterOpensForegroundTabUnfocusedOmnibox) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      InstrumentNextTab(kTab2),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN, ui::EF_ALT_DOWN),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that opening a suggestion in a background tab via Alt+Shift+Enter
// and subsequently switching to it leaves the Omnibox unfocused and the popup
// closed on the new tab.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       AltShiftEnterOpensBackgroundTabUnfocusedOmnibox) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      InstrumentNextTab(kTab2),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN,
                   ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      WaitForWebContentsReady(kTab2),
      // Verify popup remains open on Tab 1.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      // Switch to the newly opened background tab (index 2).
      SelectTab(kTabStripElementId, 2), WaitForPopupTransitionLockout(),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that opening a New Tab Page focuses the WebUI Omnibox popup by
// default even when the previous tab had web contents focused.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       NewTabPageOpensWithOmniboxFocused) {
  RunTestSequence(
      // Tab 1 with web contents focused.
      AddInstrumentedTab(kTab1, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab1), ClickWebPageBody(kTab1),
      WaitForOmniboxFocus(false),
      // Open a New Tab Page (Tab 2).
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      WaitForOmniboxFocus(true));
}

// Verifies that opening a new window / tab on NTP focuses the WebUI Omnibox
// popup and that focus is sustained without being stolen by subsequent layout
// passes or `views::FocusManager::AdvanceFocusIfNecessary()`.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       NewWindowOpensAndFocusesWebUIOmniboxOnNTP) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/567196577): Ensure this test works properly when "
                    "WebUI toolbar is enabled.";
  }
  RunTestSequence(
      WaitForBrowserActive(),
      // Instrument the initial tab 0, which starts on the NTP.
      InstrumentTab(kTab1, 0), WaitForWebContentsReady(kTab1),
      // Verify WebUI popup opens automatically on the NTP
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Verify WebUI searchbox has DOM focus and focus is not stolen
      WaitForOmniboxFocus(true),
      // Wait to ensure all asynchronous tasks, timers, and animations settle.
      WaitForPopupTransitionLockout(base::Seconds(1)),
      // Force an explicit layout pass on BrowserView (which re-evaluates
      // LocationBarView::UpdateFocusBehavior() and would trigger
      // AdvanceFocusIfNecessary if OmniboxViewViews had focus).
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        browser_view->InvalidateLayout();
        browser_view->GetWidget()->LayoutRootViewIfNecessary();
      }),
      // Verify Omnibox is still showing and focused.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      CheckOmniboxFocus(true));
}

// Verifies that pressing Shift+Enter on a match opens the result in a new
// window and resets the original window's omnibox popup state to steady state.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ShiftEnterOpensNewWindowAndResetsOmnibox) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378107): With the WebUI toolbar, the "
                    "omnibox keeps focus after the full popup closes.";
  }
  RunTestSequence(
      // 1. Open Tab 1 at chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // 2. Type "a" into the WebUI input to open suggestions.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      // 3. Send Shift+Enter to open the suggestion in a new window.
      InAnyContext(
          SendKeyPress(kPopupWebView, ui::VKEY_RETURN, ui::EF_SHIFT_DOWN)),
      // 4. Verify that in the original window, the full popup frame is hidden
      // and Omnibox focus is cleared.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that copying text in the full WebUI Omnibox records the
// Omnibox.CutOrCopyAllText metric.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest, OnCopy) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      CopyWebUIText(), CheckWebUIInputFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount(
            OmniboxEditModel::kCutOrCopyAllTextHistogram, 1, 1);
      }));
}

// Verifies that pressing Enter on an open page (without modifying the URL)
// submits the verbatim URL (reloads/navigates) and closes the popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       EnterSubmitsVerbatimUrlOnOpenPage) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/568378107): With the WebUI toolbar, the "
                    "omnibox keeps focus after the full popup closes.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN, ui::EF_NONE),
      WaitForWebContentsNavigation(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that pressing Alt+Enter on an open page (without modifying the URL)
// opens the verbatim URL in a new foreground tab.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       AltEnterOpensInNewForegroundTab) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), InstrumentNextTab(kTab2),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN, ui::EF_ALT_DOWN),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that pressing Ctrl+N / Cmd+N while the WebUI Omnibox is focused
// opens a new browser window.
// TODO(b/552482504): Fix this test.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       DISABLED_NewWindowShortcutWithWebUIOmniboxFocused) {
  ui_test_utils::BrowserCreatedObserver observer;
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      InAnyContext(
          SendKeyPress(kPopupWebView, ui::VKEY_N, ui::EF_PLATFORM_ACCELERATOR)),
      Do([&observer]() { observer.Wait(); }), Check([]() {
        return GlobalBrowserCollection::GetInstance()->GetSize() == 2;
      }));
}

// Verifies that pressing Ctrl+L / Cmd+L while typing a query selects the typed
// text and keeps the suggestions dropdown open without interruption.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       RefocusWhileTypingPreservesDropdownAndSelectsText) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Type "a" and wait for suggestions dropdown.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),

      // Press Ctrl+L / Cmd+L from keyboard.
      WaitForPopupTransitionLockout(),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_L,
                   ui::EF_PLATFORM_ACCELERATOR),

#if !BUILDFLAG(IS_MAC)
      // Verify typed text "a" is fully selected. Text selection
      // modification/inspection on macOS inputs follows different platform
      // conventions.
      InAnyContext(CheckWebUIInputSelection(0, 1)),
#endif
      WaitForOmniboxFocus(true),
      // Verify suggestions dropdown remains open.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"));
}

// Verifies that navigating to the Omnibox via Tab traversal opens and focuses
// the full WebUI popup instead of retaining focus in the native textfield,
// and that suggestions dropdown is not opened.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       TabTraversalOpensAndFocusesWebUIPopup) {
  if (features::IsWebUILocationBarEnabled()) {
    GTEST_SKIP()
        << "TODO(b/567196600): Ensure this test works properly when WebUI "
           "toolbar is enabled.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Blur and close the Omnibox popup by clicking the webpage body.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), WaitForOmniboxFocus(false),
      WaitForPopupTransitionLockout(),

      // Advance focus forward into the LocationBarView using focus traversal.
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* location_bar_view = browser_view->GetLocationBarView();
        auto* focus_manager = browser_view->GetFocusManager();
        auto* prev_view = focus_manager->GetNextFocusableView(
            location_bar_view, nullptr, /*reverse=*/true,
            /*dont_loop=*/false);
        CHECK(prev_view);
        prev_view->RequestFocus();
        focus_manager->AdvanceFocus(/*reverse=*/false);
        CHECK_EQ(focus_manager->GetFocusedView(), location_bar_view);
      }),

      // Verify the WebUI popup opens and gains focus.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      InSameContext(WaitForWebContentsReady(
          kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))),
      // Verify suggestions dropdown is not visible.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      // Verify omnibox is coherently focused across edit model, WebUI input,
      // and native textfield.
      WaitForOmniboxFocus(true));
}

// Verifies that clicking outside on the webpage body when text is selected
// in the WebUI Omnibox closes the popup on the first click and shifts focus
// to the webpage.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ClickOutsideDismissesPopupWithSelectedText) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      // Select a portion of the text in the WebUI searchbox.
      InAnyContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                               R"(el => {
                                    el.setSelectionRange(0, 6);
                                    el.dispatchEvent(new Event('select'));
                                    document.dispatchEvent(
                                        new Event('selectionchange'));
                                  })")),
      CheckWebUIInputSelection(0, 6),
      // Click on the webpage body to blur and dismiss.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that navigating to the Omnibox via Shift+Tab reverse traversal opens
// and focuses the full WebUI popup instead of retaining focus in the native
// textfield.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ShiftTabTraversalOpensAndFocusesWebUIPopup) {
  if (features::IsWebUILocationBarEnabled()) {
    GTEST_SKIP() << "TODO(b/567196736): Ensure this test works properly when "
                    "WebUI toolbar is enabled.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Blur and close the Omnibox popup by clicking the webpage body.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), WaitForOmniboxFocus(false),
      WaitForPopupTransitionLockout(),

      // Advance focus in reverse into the LocationBarView using focus
      // traversal.
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* location_bar_view = browser_view->GetLocationBarView();
        auto* focus_manager = browser_view->GetFocusManager();
        auto* next_view = focus_manager->GetNextFocusableView(
            location_bar_view, nullptr, /*reverse=*/false,
            /*dont_loop=*/false);
        CHECK(next_view);
        next_view->RequestFocus();
        focus_manager->AdvanceFocus(/*reverse=*/true);
        CHECK_EQ(focus_manager->GetFocusedView(), location_bar_view);
      }),

      // Verify the WebUI popup opens and gains focus.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      InSameContext(WaitForWebContentsReady(
          kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))),
      // Verify suggestions dropdown is not visible.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      // Ensure omnibox is focused.
      WaitForOmniboxFocus(true));
}

// Verifies that tabbing past the Omnibox closes the full WebUI popup.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       TabPastOmniboxClosesWebUIPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Traverse focus past the Omnibox popup.
      Do([this]() {
        auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
            BrowserWindow::FromBrowser(browser())
                ->GetLocationBar()
                ->GetOmniboxPopupView());
        CHECK(popup_view && popup_view->presenter());
        auto* base_content = popup_view->presenter()->GetWebUIContent();
        CHECK(base_content);
        base_content->AdvanceFocus(/*reverse=*/false);
      }),

      // Verify the WebUI popup closes and loses focus.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), WaitForOmniboxFocus(false),

      // Verify that focus moved outside the view hosting the location bar.
      PollUntil(
          [this]() {
            auto* browser_view =
                BrowserView::GetBrowserViewForBrowser(browser());
            if (!browser_view || !browser_view->GetFocusManager()) {
              return false;
            }
            auto* focused_view =
                browser_view->GetFocusManager()->GetFocusedView();
            auto* location_bar_host = GetLocationBarHostView();
            return focused_view && location_bar_host &&
                   !location_bar_host->Contains(focused_view);
          },
          "WaitForFocusOutsideLocationBar"));
}

// Verifies that the native LocationBarView focus ring remains hidden
// across all popup state transitions in Full WebUI mode.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       LocationBarFocusRingHiddenInFullWebUIMode) {
  if (IsWebUIToolbarEnabled()) {
    // This test covers the focus ring on the Views `LocationBarView`. The
    // WebUI toolbar has no `LocationBarView`, so there is nothing to check.
    GTEST_SKIP() << "Not applicable: the WebUI toolbar has no LocationBarView.";
  }
  auto check_focus_ring = [this](bool should_paint) {
    return Do([this, should_paint]() {
      auto* location_bar = BrowserView::GetBrowserViewForBrowser(browser())
                               ->toolbar()
                               ->location_bar_view();
      if (!location_bar) {
        return;
      }
      auto* focus_ring = views::FocusRing::Get(location_bar);
      if (focus_ring) {
        EXPECT_EQ(focus_ring->ShouldPaintForTesting(), should_paint);
      }
    });
  };

  RunTestSequence(
      // 1. Open tab and focus WebUI popup -> native focus ring should not
      // paint.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), check_focus_ring(false),
      // 2. Click webpage body to dismiss -> focus ring should still not paint.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false), check_focus_ring(false));
}

// Verifies that Shift+Tabbing past the Omnibox closes the full WebUI popup
// and advances focus to the preceding view (e.g. reload button).
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ShiftTabPastOmniboxClosesWebUIPopup) {
  if (IsWebUIToolbarEnabled()) {
    GTEST_SKIP() << "TODO(b/567215920): Ensure this test works properly when "
                    "WebUI toolbar is enabled.";
  }
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),
      // Reverse traverse focus past the Omnibox popup.
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
            browser_view->GetLocationBar()->GetOmniboxPopupView());
        CHECK(popup_view && popup_view->presenter());
        auto* base_content = popup_view->presenter()->GetWebUIContent();
        CHECK(base_content);
        base_content->AdvanceFocus(/*reverse=*/true);
      }),

      // Verify the WebUI popup closes and loses focus.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false),

      // Verify that focus landed on a view outside LocationBarView preceding
      // it.
      Check([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* focused_view = browser_view->GetFocusManager()->GetFocusedView();
        return focused_view &&
               !browser_view->GetLocationBarView()->Contains(focused_view);
      }));
}

// Verifies that typing into the WebUI Omnibox, clicking outside on the webpage
// body to blur, and then refocusing the location bar (via Ctrl+L) selects all
// text in the Omnibox.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       FocusLocationBarAfterBlurSelectsAll) {
  RunTestSequence(
      // 1. Open the WebUI omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // 2. Type into it.
      InputWebUIText("hello"), WaitForWebUIInputValue("hello"),
      // 3. Click outside of it.
      ClickWebPageBody(kTab1),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      WaitForVisibleOmniboxUnfocused(),
      // 4. Focus the location bar to reopen/refocus it (via Ctrl+L).
      WaitForPopupTransitionLockout(),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_L,
                   ui::EF_PLATFORM_ACCELERATOR),
      WaitForOmniboxFocus(true),
      // 5. Verifies that everything is selected.
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.selectionStart},${el.selectionEnd}`", "0,5")),
      InAnyContext(CheckWebUIInputSelection(0, 5)));
}

// Verifies that input entered into the native Omnibox during startup
// before popup readiness is seamlessly transferred to WebUI upon readiness.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ColdStartTypingHandoffToWebUI) {
  RunTestSequence(
      WaitForBrowserActive(), InstrumentTab(kTab1), Do([this]() {
        // Go through `LocationBar` rather than `LocationBarView`: with the
        // WebUI toolbar there is no `LocationBarView`, and
        // `BrowserView::GetLocationBarView()` returns null.
        auto* location_bar =
            BrowserWindow::FromBrowser(browser())->GetLocationBar();
        location_bar->FocusLocation(/*is_user_initiated=*/false,
                                    /*clear_focus_if_failed=*/false);
        location_bar->GetOmniboxView()->SetUserText(u"chromium");
        if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
          popup_view->SyncNativeStateToWebUI(/*query_zps=*/false);
        }
      }),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      InAnyContext(WaitForWebUIInputValue("chromium")),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.value}|${el.selectionStart}|${el.selectionEnd}`",
          "chromium|8|8")));
}

// Verifies that text selection highlighted in native Omnibox during handoff
// is preserved when transferred to WebUI.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       ColdStartSelectionHandoffToWebUI) {
  RunTestSequence(
      WaitForBrowserActive(), InstrumentTab(kTab1), Do([this]() {
        // Go through `LocationBar` rather than `LocationBarView`: with the
        // WebUI toolbar there is no `LocationBarView`, and
        // `BrowserView::GetLocationBarView()` returns null.
        auto* location_bar =
            BrowserWindow::FromBrowser(browser())->GetLocationBar();
        location_bar->FocusLocation(/*is_user_initiated=*/false,
                                    /*clear_focus_if_failed=*/false);
        auto* omnibox_view = location_bar->GetOmniboxView();
        omnibox_view->SetUserText(u"chromium");
        omnibox_view->SetSelectionBounds(gfx::Range(0, 4));
        if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
          popup_view->SyncNativeStateToWebUI(/*query_zps=*/false);
        }
      }),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      InAnyContext(WaitForWebUIInputValue("chromium")),
      InAnyContext(CheckJsResultAt(
          kPopupWebView, kWebUIInput,
          "el => `${el.value}|${el.selectionStart}|${el.selectionEnd}`",
          "chromium|0|4")));
}

// Verifies that when a user types a draft in the omnibox and then navigates the
// active page (e.g. by clicking a link), the popup is dismissed, user input in
// progress is cleared, and the omnibox displays the new page's URL.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       PageNavigationWithDraftDismissesPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  const GURL url_b = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Open Tab 1 at page A and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, url_a),
      // Type a draft into the WebUI input.
      InputWebUIText("foo"),
      // Verify popup is open and user input is in progress.
      WaitForPopupState(OmniboxPopupState::kFull),
      CheckUserInputInProgress(true),
      // Click into the page body to unfocus the Omnibox.
      ClickWebPageBody(kTab1), WaitForVisibleOmniboxUnfocused(),
      // Trigger a renderer-initiated navigation to page B.
      InAnyContext(ExecuteJs(
          kTab1, base::StringPrintf("() => { window.location.href = '%s'; }",
                                    url_b.spec().c_str()))),
      WaitForWebContentsNavigation(kTab1, url_b),
      // Verify browser-side state: popup is dismissed, draft is discarded,
      // and Omnibox text reflects page B.
      WaitForPopupDismissed(), WaitForPopupState(OmniboxPopupState::kNone),
      CheckUserInputInProgress(false),
      CheckResult(
          [this]() {
            return GetOmniboxControllerForTest()
                       ->edit_model()
                       ->GetPermanentDisplayText()
                       .find(u"title2.html") != std::u16string::npos;
          },
          true, "PermanentDisplayTextShowsPageB"),
      CheckResult(
          [this]() {
            auto* location_bar =
                BrowserWindow::FromBrowser(browser())->GetLocationBar();
            return location_bar->GetOmniboxView()->GetText().find(
                       u"title2.html") != std::u16string::npos;
          },
          true, "OmniboxShowsPageB"));
}

// Verifies that when a navigation occurs while the omnibox is focused (the user
// is actively typing), the draft is NOT dismissed, popup remains kFull, and
// user_input_in_progress remains true (b/527512550).
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       PageNavigationWhileOmniboxFocusedKeepsDraft) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  const GURL url_b = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Open Tab 1 at page A and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, url_a),
      // Type a draft into the WebUI input without unfocusing.
      InputWebUIText("foo"),
      // Verify popup is open, draft is in progress, and omnibox is focused.
      WaitForPopupState(OmniboxPopupState::kFull),
      CheckUserInputInProgress(true), WaitForOmniboxFocus(true),
      // Trigger a page navigation from script while omnibox keeps focus.
      InAnyContext(ExecuteJs(
          kTab1, base::StringPrintf("() => { window.location.href = '%s'; }",
                                    url_b.spec().c_str()))),
      WaitForWebContentsNavigation(kTab1, url_b),
      // Verify popup state remains kFull, draft is still in progress, and focus
      // is retained.
      CheckPopupState(OmniboxPopupState::kFull), CheckUserInputInProgress(true),
      CheckOmniboxFocus(true));
}

// Verifies that a same-document navigation (e.g. history.pushState) after
// clicking into the page does NOT dismiss the popup or discard the draft.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       SameDocumentNavigationWithDraftDoesNotDismissPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      // Open Tab 1 at page A and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, url_a),
      // Type a draft into the WebUI input.
      InputWebUIText("foo"),
      // Verify popup is open and user input is in progress.
      WaitForPopupState(OmniboxPopupState::kFull),
      // Click into the page body to unfocus the Omnibox.
      ClickWebPageBody(kTab1), WaitForVisibleOmniboxUnfocused(),
      // Trigger a same-document navigation (history.pushState).
      InAnyContext(ExecuteJs(
          kTab1, "() => { window.history.pushState({}, '', '#fragment'); }")),
      // Wait for the browser to commit the navigation, so the checks below
      // run after it has been processed.
      PollUntil(
          [this, url_a]() {
            return browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetLastCommittedURL() == url_a.Resolve("#fragment");
          },
          "WaitForSameDocumentNavigationCommitted"),
      // Verify popup state remains kFull and user input remains in progress.
      CheckPopupState(OmniboxPopupState::kFull), CheckUserInputInProgress(true),
      CheckVisibleOmniboxUnfocused());
}

// Verifies that opening the find bar (Ctrl/Cmd+F) while the omnibox is focused
// blurs the omnibox, closes the popup, and sends key presses to the find bar,
// as with the Views omnibox. See b/566211508.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       FindBarTakesFocusAndKeyPresses) {
  base::AutoReset<bool> disable_animations =
      FindBarHost::SetEnableAnimationsForTesting(false);

  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      OpenFindBarFromKeyboard(),
      // The omnibox is blurred and the popup closes.
      WaitForPopupDismissed(), WaitForPopupState(OmniboxPopupState::kNone),
      // The browser window, not the popup, holds activation, so key presses
      // reach the find bar.
      WaitForBrowserWidgetActive(), CheckFindBarFocused(),
      // Type into the find bar. chrome://version/ contains an "a".
      SendKeyPress(kBrowserViewElementId, ui::VKEY_A, ui::EF_NONE),
      WaitForFindMatch(u"a"));
}

// Verifies that the find bar keeps focus when opened right after a new tab
// focused the omnibox. On Aura, switching tabs while the popup holds activation
// makes the browser window restore the tab's stored focus when it's next
// activated, which focusing the find bar does.
// TODO(b/571201192): Enable on macOS once `BrowserView` no longer restores the
// tab's focus over the find bar's pending focus request.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FindBarAfterNewTabKeepsFocus DISABLED_FindBarAfterNewTabKeepsFocus
#else
#define MAYBE_FindBarAfterNewTabKeepsFocus FindBarAfterNewTabKeepsFocus
#endif
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       MAYBE_FindBarAfterNewTabKeepsFocus) {
  base::AutoReset<bool> disable_animations =
      FindBarHost::SetEnableAnimationsForTesting(false);

  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Open a new tab, which focuses its omnibox, while the popup is active.
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2), WaitForPopupTransitionLockout(),
      WaitForPopupActive(), OpenFindBarFromKeyboard(), WaitForPopupDismissed(),
      WaitForBrowserWidgetActive(),
      // The find bar still has focus once the activation has settled.
      WaitForPopupTransitionLockout(), CheckFindBarFocused());
}

// Verifies that a draft keeps the popup open, unfocused, while the find bar has
// focus, and that the draft is kept.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       FindBarWithDraftTakesFocusAndKeepsDraft) {
  base::AutoReset<bool> disable_animations =
      FindBarHost::SetEnableAnimationsForTesting(false);

  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("draft"), CheckUserInputInProgress(true),
      OpenFindBarFromKeyboard(),
      // The omnibox is blurred, but the draft keeps the popup open.
      WaitForVisibleOmniboxUnfocused(),
      WaitForPopupState(OmniboxPopupState::kFull), WaitForBrowserWidgetActive(),
      CheckFindBarFocused(), CheckUserInputInProgress(true),
      CheckResult(
          [this]() {
            return GetOmniboxControllerForTest()->edit_model()->user_text();
          },
          std::u16string(u"draft"), "CheckDraftKept"));
}

// Verifies that choosing Find from the app menu while a draft keeps the popup
// open leaves focus on the find bar after the menu closes.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       FindBarFromAppMenuWithDraftKeepsFocus) {
  base::AutoReset<bool> disable_animations =
      FindBarHost::SetEnableAnimationsForTesting(false);
  const ui::ElementContext browser_context =
      BrowserView::GetBrowserViewForBrowser(browser())->GetElementContext();

  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("draft"), CheckUserInputInProgress(true),
      InContext(browser_context, MoveMouseTo(kToolbarAppMenuButtonElementId)),
      InSameContextAs(OmniboxPopupPresenter::kRoundedResultsFrame,
                      ClickMouse()),
      InAnyContext(WaitForShow(AppMenuModel::kMoreToolsMenuItem)),
      // A menu command runs before `AppMenuClosed()`, so show the find bar,
      // then close the menu.
      Do([this]() { FindBarController::From(browser())->Show(); }),
      Check(
          []() {
            views::MenuController* menu =
                views::MenuController::GetActiveInstance();
            if (!menu) {
              return false;
            }
            menu->Cancel(views::MenuController::ExitType::kAll);
            return true;
          },
          "CloseAppMenu"),
      InAnyContext(WaitForHide(AppMenuModel::kMoreToolsMenuItem)),
      // The draft keeps the popup open, but the find bar keeps focus.
      WaitForVisibleOmniboxUnfocused(),
      WaitForPopupState(OmniboxPopupState::kFull), WaitForBrowserWidgetActive(),
      CheckFindBarFocused(), CheckUserInputInProgress(true));
}

// Verifies that switching tabs gives focus back to the find bar in one tab and
// to the omnibox in the other, as `FindBarViewsUiTest.FocusRestoreOnTabSwitch`
// does for the Views omnibox.
IN_PROC_BROWSER_TEST_P(FullWebUIOmniboxInteractiveTest,
                       FindBarFocusRestoreOnTabSwitch) {
  base::AutoReset<bool> disable_animations =
      FindBarHost::SetEnableAnimationsForTesting(false);
  auto show_find_bar_and_search = [this](std::u16string text) {
    return Steps(Do([this]() { FindBarController::From(browser())->Show(); }),
                 PollUntil(
                     [this]() {
                       return GetFindBar()->IsFindBarVisible() &&
                              GetFindBar()->HasFocus();
                     },
                     "WaitForFindBarFocused"),
                 EnterText(FindBarView::kTextField, std::move(text)));
  };

  RunTestSequence(
      // Open tab A (index 1) and search for "a".
      WaitForBrowserActive(),
      AddInstrumentedTab(kTab1, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab1), WaitForPopupTransitionLockout(),
      show_find_bar_and_search(u"a"),
      // Open tab B (index 2), search for "b", then focus its omnibox.
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2), WaitForPopupTransitionLockout(),
      show_find_bar_and_search(u"b"),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_L,
                   ui::EF_PLATFORM_ACCELERATOR),
      WaitForPopupActive(),
      // Select tab A. Its find bar gets focus back, and keeps it.
      SwitchTab(kTabStripElementId, 1), WaitForPopupDismissed(),
      WaitForBrowserWidgetActive(),
      PollUntil([this]() { return GetFindBar()->HasFocus(); },
                "WaitForFindBarFocusedInTabA"),
      WaitForPopupTransitionLockout(), CheckFindBarFocused(),
      // Select tab B. Its omnibox gets focus back, and keeps it.
      SwitchTab(kTabStripElementId, 2), WaitForPopupActive(),
      WaitForPopupTransitionLockout(), WaitForPopupActive());
}

INSTANTIATE_TEST_SUITE_P(All,
                         FullWebUIOmniboxInteractiveTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WebUIToolbarEnabled"
                                             : "WebUIToolbarDisabled";
                         });

class FullWebUIOmniboxAimInteractiveTestBase
    : public FullWebUIOmniboxInteractiveTestBase {
 public:
  FullWebUIOmniboxAimInteractiveTestBase() = default;
  ~FullWebUIOmniboxAimInteractiveTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      bool force_enable_aim) {
    std::vector<base::test::FeatureRefAndParams> features = {
        {omnibox::internal::kWebUIOmniboxFullPopup, {}},
        {omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady, {}}};
    if (force_enable_aim) {
      features.emplace_back(omnibox::internal::kWebUIOmniboxAimPopup,
                            base::FieldTrialParams());
      base::FieldTrialParams simplification_params = {
          {omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
           "below_results"},
          {omnibox::kHideClassicContextButton.name, "false"}};
      features.emplace_back(omnibox::internal::kWebUIOmniboxSimplification,
                            simplification_params);
      features.emplace_back(omnibox::kAimEnabled, base::FieldTrialParams());
    }
    return features;
  }

  auto SetAimEligibleResponse() {
    return Do([this]() {
      auto* profile = browser()->GetProfile();
      auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
      omnibox::AimEligibilityResponse response;
      response.set_is_eligible(true);
      response.set_is_fusebox_eligible(true);
      response.set_is_cobrowse_eligible(true);
      auto* config = response.mutable_searchbox_config();
      config->mutable_rule_set();
      auto* tool_config = config->add_tool_configs();
      tool_config->set_tool(omnibox::TOOL_MODE_DEEP_SEARCH);
      tool_config->mutable_rule()->set_allow_all_input_types(true);

      auto* input_config = config->add_input_type_configs();
      input_config->set_input_type(omnibox::INPUT_TYPE_LENS_IMAGE);

      auto* input_config2 = config->add_input_type_configs();
      input_config2->set_input_type(omnibox::INPUT_TYPE_LENS_FILE);

      auto* input_config3 = config->add_input_type_configs();
      input_config3->set_input_type(omnibox::INPUT_TYPE_BROWSER_TAB);

      std::string serialized;
      response.SerializeToString(&serialized);
      service->SetEligibilityResponseForDebugging(
          base::Base64Encode(serialized));
      ASSERT_TRUE(
          base::test::RunUntil([&]() { return service->IsAimEligible(); }));
    });
  }

  auto WaitForOmniboxAimStateReady(
      const ui::ElementIdentifier& omnibox_context_entrypoint_contents_id) {
    return SearchboxInteractiveTestMixin::WaitForOmniboxAimStateReady(
        omnibox_context_entrypoint_contents_id, kPopupSearchbox);
  }

  // Returns the active `LocationBar`. Works for both `LocationBarView` and
  // `WebUILocationBar` (when WebUIToolbar is enabled,
  // `BrowserView::GetLocationBarView()` returns null).
  LocationBar* GetActiveLocationBar() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetLocationBar() : nullptr;
  }

  // Returns the `OmniboxController` owned by the active `LocationBar`, or null
  // if there is no active location bar.
  OmniboxController* GetActiveOmniboxController() {
    auto* location_bar = GetActiveLocationBar();
    return location_bar ? location_bar->GetOmniboxController() : nullptr;
  }

  // Returns the AIM popup presenter of the active `LocationBar`, obtained via
  // its `OmniboxPopupPresenterDelegate`, or null if unavailable.
  OmniboxPopupAimPresenter* GetActiveAimPresenter() {
    auto* location_bar = GetActiveLocationBar();
    auto* presenter_delegate =
        location_bar ? location_bar->GetPresenterDelegate() : nullptr;
    return presenter_delegate
               ? presenter_delegate->GetOmniboxPopupAimPresenter()
               : nullptr;
  }

  auto GetActiveAimPopupWebView() {
    return base::BindLambdaForTesting([this]() -> views::View* {
      auto* aim_presenter = GetActiveAimPresenter();
      if (!aim_presenter) {
        return nullptr;
      }
      return aim_presenter->GetWebUIContent();
    });
  }

  auto WaitForAimPopupReady() {
    return Steps(
        WaitForPopupState(OmniboxPopupState::kAim),
        PollUntil(
            [this]() -> bool {
              auto* aim_presenter = GetActiveAimPresenter();
              auto* widget =
                  aim_presenter ? aim_presenter->GetWidget() : nullptr;
              auto* content =
                  aim_presenter ? aim_presenter->GetWebUIContent() : nullptr;
              return aim_presenter && aim_presenter->IsShown() && widget &&
                     widget->IsVisible() && content && content->GetVisible() &&
                     content->IsDrawn();
            },
            "WaitForAimPopupViewDrawn"),
        InAnyContext(InstrumentNonTabWebView(kAimPopupWebView,
                                             GetActiveAimPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kAimPopupWebView, GURL(chrome::kChromeUIOmniboxPopupAimURL))));
  }

  auto WaitForAimPopupHidden() {
    return Steps(
        PollUntil(
            [this]() -> bool {
              auto* controller = GetActiveOmniboxController();
              if (!controller) {
                return false;
              }
              return controller->popup_state_manager()->popup_state() !=
                     OmniboxPopupState::kAim;
            },
            "WaitForAimPopupHidden"),
        UninstrumentWebContents(kAimPopupWebView));
  }

  auto WaitForAimInputValue(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element,
                            const std::string& expected_value) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimInputValueChanged);
    StateChange value_changed;
    value_changed.event = kAimInputValueChanged;
    value_changed.where = element;
    value_changed.test_function =
        base::StringPrintf("(el) => el.value === '%s'", expected_value.c_str());
    value_changed.continue_across_navigation = true;
    return WaitForStateChange(contents_id, value_changed);
  }

  auto WaitForAimSubmitEnabled(const ui::ElementIdentifier& contents_id) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimSubmitEnabled);
    StateChange submit_enabled;
    submit_enabled.event = kAimSubmitEnabled;
    submit_enabled.where = DeepQuery{"omnibox-aim-app", "#composebox"};
    submit_enabled.test_function = "(el) => el && el.canSubmitFilesAndInput";
    submit_enabled.continue_across_navigation = true;
    return Steps(WaitForElementToRender(contents_id, kAimSubmit),
                 WaitForStateChange(contents_id, submit_enabled));
  }

  auto InputAimPopupText(const std::string& text) {
    return Steps(
        InSameContext(ExecuteJsAt(kAimPopupWebView, kAimInput,
                                  base::StringPrintf(R"(el => {
              const fullText = '%s';
              for (let i = 0; i < fullText.length; i++) {
                el.value = fullText.substring(0, i + 1);
                el.dispatchEvent(
                    new Event('input', {bubbles: true, composed: true}));
              }
            })",
                                                     text.c_str()))),
        InAnyContext(WaitForAimInputValue(kAimPopupWebView, kAimInput, text)),
        InAnyContext(WaitForAimSubmitEnabled(kAimPopupWebView)));
  }

  // Clicks `element` inside the Full WebUI popup with a real mouse click.
  //
  // Readiness is determined by the popup itself rather than by a first-paint
  // signal: the popup presenter shows the widget only once the renderer's
  // visual state is ready, and callers wait for `element` to render before
  // clicking. The mouse is moved directly to the center of `element`.
  auto ClickPopupElement(const DeepQuery& element) {
    return Steps(
        MoveMouseTo(kPopupWebView, DeepQueryToRelativePosition(element)),
        ClickMouse());
  }

  auto OpenAimPopup() {
    return Steps(
        SetAimEligibleResponse(),
        OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
        InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
        InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
        InSameContext(ClickElement(kPopupWebView, kComposeButton)),
        WaitForAimPopupReady(),
        InAnyContext(WaitForElementToRender(kAimPopupWebView, kAimInput)));
  }
};

class FullWebUIOmniboxAimInteractiveTest
    : public FullWebUIOmniboxAimInteractiveTestBase {
 public:
  FullWebUIOmniboxAimInteractiveTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures(/*force_enable_aim=*/true);
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {omnibox::internal::kWebUIOmniboxPopup,
                           omnibox::kAimServerEligibilityEnabled,
                           omnibox::kAimFuseboxEligibilityCheckEnabled,
                           features::kWebUILocationBar});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Dismissing the AIM popup with Escape should hand the composebox draft back
// to the omnibox along with focus.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       ExitAimRestoresFocusAndInput_Escape) {
  const std::string kQuery = "my test query";
  RunTestSequence(OpenAimPopup(), InputAimPopupText(kQuery),
                  InAnyContext(SendKeyPress(kBrowserViewElementId,
                                            ui::VKEY_ESCAPE, ui::EF_NONE)),
                  WaitForAimPopupHidden(), WaitForWebUIInputValue(kQuery),
                  WaitForOmniboxFocus(true)),
      CheckWebUIInputSelection(kQuery.length(), kQuery.length());
}

// Dismissing the AIM popup with the "X" button when there is no input in the
// composebox should return to the omnibox to its original state.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       ExitAimRestoresFocusAndInput_CancelIcon) {
  const std::string kOriginalUrl = "chrome://version";
  RunTestSequence(
      OpenAimPopup(),
      InAnyContext(WaitForElementToRender(kAimPopupWebView, kCancelIcon)),
      InSameContext(ClickElement(kAimPopupWebView, kCancelIcon)),
      WaitForAimPopupHidden(), WaitForWebUIInputValue(kOriginalUrl),
      WaitForOmniboxFocus(true),
      // TODO(b/564894332): When a URL is restored to the Omnibox, the cursor
      // should be at the end of the text (kQuery.length(), kQuery.length()),
      // not the beginning.
      CheckWebUIInputSelection(0, kOriginalUrl.length()));
}

// Dismissing the AIM popup by clicking outside should hand the composebox
// draft back to the omnibox along with focus.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       ExitAimRestoresFocusAndInput_ClickOutside) {
  const std::string kQuery = "my test query";
  RunTestSequence(
      OpenAimPopup(), InputAimPopupText(kQuery),
      // Dismiss the popup by clicking outside of it, on the webpage body.
      ClickWebPageBody(kTab1), WaitForAimPopupHidden(),
      // The draft must land in the omnibox rather than being dropped, and
      // focus must come back with it.
      WaitForOmniboxText(base::UTF8ToUTF16(kQuery)),
      WaitForPopupState(OmniboxPopupState::kFull),
      WaitForWebUIInputValue(kQuery), WaitForOmniboxFocus(true),
      CheckWebUIInputSelection(kQuery.length(), kQuery.length()));
}

// Opening multiple New Tab Pages rapidly while the Full WebUI omnibox popup is
// already open and focused should keep the AIM entrypoint button visible on
// every new tab (regression test for b/563421197).
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       AimButtonRemainsVisibleAcrossRapidNewTabs) {
  RunTestSequence(
      SetAimEligibleResponse(), FocusOmniboxOnInitialNtp(kTab1),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      Do([this]() {
        for (int i = 0; i < 5; ++i) {
          chrome::AddTabAt(browser(), GURL(chrome::kChromeUINewTabURL), -1,
                           true);
        }
      }),
      WaitForPopupTransitionLockout(),
      WaitForPopupState(OmniboxPopupState::kFull), WaitForOmniboxFocus(true),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      InAnyContext(CheckJsResultAt(kPopupWebView, kPopupSearchbox,
                                   "el => el.aimButtonVisible_", true)));
}

// Switching back and forth between tabs where the omnibox is focused should
// keep the AIM entrypoint button visible in the Full WebUI popup (regression
// test for b/563421197 after keeping the popup on screen across tab switches).
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       AimButtonRemainsVisibleAcrossTabSwitch) {
  RunTestSequence(
      SetAimEligibleResponse(), FocusOmniboxOnInitialNtp(kTab1),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2), WaitForPopupTransitionLockout(),
      WaitForPopupState(OmniboxPopupState::kFull),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      InAnyContext(CheckJsResultAt(kPopupWebView, kPopupSearchbox,
                                   "el => el.aimButtonVisible_", true)),
      // Switch back to Tab 1 (the initial tab, at index 0).
      SwitchTab(kTabStripElementId, 0),
      CheckPopupState(OmniboxPopupState::kFull),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      InAnyContext(CheckJsResultAt(kPopupWebView, kPopupSearchbox,
                                   "el => el.aimButtonVisible_", true)),
      // Switch forward to Tab 2 (index 1) and verify the AIM button remains
      // visible.
      SwitchTab(kTabStripElementId, 1),
      CheckPopupState(OmniboxPopupState::kFull),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      InAnyContext(CheckJsResultAt(kPopupWebView, kPopupSearchbox,
                                   "el => el.aimButtonVisible_", true)));
}

// Disabling "Show AI mode" preference should immediately hide the AIM
// entrypoint button in the Full WebUI omnibox popup without needing to
// defocus/refocus.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxAimInteractiveTest,
                       AimButtonVisibilityUpdatesImmediatelyOnPrefChange) {
  RunTestSequence(
      SetAimEligibleResponse(), FocusOmniboxOnInitialNtp(kTab1),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InAnyContext(WaitForElementToRender(kPopupWebView, kComposeButton)),
      InAnyContext(CheckJsResultAt(kPopupWebView, kPopupSearchbox,
                                   "el => el.aimButtonVisible_", true)),
      // Disable the preference (as happens when unchecking "Show AI mode" in
      // the omnibox context menu).
      Do([this]() {
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            omnibox::kShowAiModeOmniboxButton, false);
      }),
      // Verify AIM button in WebUI popup is immediately hidden without
      // refocusing.
      InAnyContext(WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                                        "el => !el.aimButtonVisible_")),
      // Re-enable the preference.
      Do([this]() {
        browser()->GetProfile()->GetPrefs()->SetBoolean(
            omnibox::kShowAiModeOmniboxButton, true);
      }),
      // Verify AIM button is immediately shown again.
      InAnyContext(WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                                        "el => el.aimButtonVisible_")));
}

class FullWebUIOmniboxSimplificationInteractiveTest
    : public FullWebUIOmniboxAimInteractiveTestBase {
 public:
  FullWebUIOmniboxSimplificationInteractiveTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    for (auto& feature : GetEnabledFeatures(/*force_enable_aim=*/true)) {
      if (feature.feature.get().name !=
          omnibox::internal::kWebUIOmniboxSimplification.name) {
        enabled_features.push_back(feature);
      }
    }
    enabled_features.emplace_back(
        omnibox::internal::kWebUIOmniboxSimplification,
        base::FieldTrialParams{
            {omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
             "below_results"},
            {omnibox::kHideClassicContextButton.name, "false"},
            {"Omnibox_ContextButtonHasBackground", "true"},
            {"Omnibox_ContextButtonShapeIsOblong", "true"}});
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {omnibox::internal::kWebUIOmniboxPopup,
                           omnibox::kAimServerEligibilityEnabled,
                           omnibox::kAimFuseboxEligibilityCheckEnabled,
                           features::kWebUILocationBar});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxSimplificationInteractiveTest,
                       HasBackgroundApplied) {
  const DeepQuery kContextButton = {
      "omnibox-full-app",
      "omnibox-popup-searchbox",
      "omnibox-popup-contextual-entrypoint",
      "#context",
      "cr-composebox-contextual-entrypoint-button",
      "#entrypoint"};
  RunTestSequence(
      SetAimEligibleResponse(),
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      InAnyContext(WaitForElementToRender(kPopupWebView, kContextButton)),
      InSameContext(CheckJsResultAt(
          kPopupWebView, kContextButton,
          "el => window.getComputedStyle(el).backgroundColor !== 'transparent'",
          true)));
}

IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxSimplificationInteractiveTest,
                       OblongShapeApplied) {
  const DeepQuery kContextButton = {
      "omnibox-full-app",
      "omnibox-popup-searchbox",
      "omnibox-popup-contextual-entrypoint",
      "#context",
      "cr-composebox-contextual-entrypoint-button",
      "#entrypoint"};
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kOblongStyleApplied);
  StateChange style_applied;
  style_applied.event = kOblongStyleApplied;
  style_applied.where = kContextButton;
  style_applied.test_function =
      "(el) => el && window.getComputedStyle(el).borderRadius === \"100px\"";
  RunTestSequence(
      SetAimEligibleResponse(),
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchPrimaryText,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      InAnyContext(WaitForElementToRender(kPopupWebView, kContextButton)),
      InAnyContext(WaitForStateChange(kPopupWebView, style_applied)));
}
