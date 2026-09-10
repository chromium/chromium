// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPopupWebView);
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
const DeepQuery kFirstSuggestionMatchContents = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='1']", "#contents"};
}  // namespace

class FullWebUIOmniboxInteractiveTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  FullWebUIOmniboxInteractiveTestBase() = default;
  ~FullWebUIOmniboxInteractiveTestBase() override = default;

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

  auto WaitForPopupReady() {
    return Steps(
        InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
        InAnyContext(
            InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))));
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

  auto WaitForOmniboxFocus(bool expected_focus) {
    return PollUntil(
        [this, expected_focus]() -> bool {
          auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
          if (!browser_view || !browser_view->GetLocationBarView() ||
              !browser_view->GetLocationBarView()->omnibox_view()) {
            return false;
          }
          return browser_view->GetLocationBarView()
                     ->omnibox_view()
                     ->HasFocus() == expected_focus;
        },
        "WaitForOmniboxFocus");
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
                               return gfx::Point(bounds.right() - 20,
                                                 bounds.bottom() - 20);
                             })),
                 ClickMouse());
  }

  auto SwitchTabAndRestorePopup(ui::ElementIdentifier tab_strip,
                                int tab_index,
                                ui::ElementIdentifier tab_id = kTab1) {
    return Steps(SelectTab(tab_strip, tab_index),
                 WaitForPopupTransitionLockout(), Do([this]() {
                   if (auto* popup_view = BrowserWindow::FromBrowser(browser())
                                              ->GetLocationBar()
                                              ->GetOmniboxPopupView()) {
                     if (popup_view->presenter()) {
                       popup_view->presenter()->RequestFocus();
                     }
                   }
                 }),
                 WaitForPopupReady());
  }

  auto OpenInitialTabAndFocusOmnibox(ui::ElementIdentifier tab_id,
                                     const GURL& url) {
    return Steps(Do([this]() {
                   ASSERT_TRUE(
                       ui_test_utils::BringBrowserWindowToFront(browser()));
                 }),
                 AddInstrumentedTab(tab_id, url),
                 WaitForWebContentsReady(tab_id),
                 WaitForPopupTransitionLockout(), Do([this]() {
                   if (auto* popup_view = BrowserWindow::FromBrowser(browser())
                                              ->GetLocationBar()
                                              ->GetOmniboxPopupView()) {
                     popup_view->OnFocus(/*query_zps=*/true);
                   }
                 }),
                 WaitForPopupReady());
  }
};

class FullWebUIOmniboxInteractiveTest
    : public FullWebUIOmniboxInteractiveTestBase {
 public:
  FullWebUIOmniboxInteractiveTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{omnibox::kWebUIOmniboxFullPopup},
        /*disabled_features=*/{omnibox::internal::kWebUIOmniboxPopup});
  }
  ~FullWebUIOmniboxInteractiveTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the draft and focus are restored after typing an uncommitted draft
// in one tab, switching away to another tab, and switching back to the original
// tab.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       ActiveUncommittedDraft) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("ffffff"),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      // Tab 1 is index 1 (Tab 0 is the default startup tab)
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      // Verify the WebUI input text is "ffffff".
      WaitForWebUIInputValue("ffffff"),
      // Verify the WebUI input has keyboard focus.
      CheckWebUIInputFocus(true));
}

// Verifies that highlighting a match, switching tabs, and switching back
// preserves the highlighted match text.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, HighlightAndSwitchTab) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      // Wait for the first suggestion to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),
      // Send ArrowDown key to highlight a match.
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_DOWN, ui::EF_NONE)),
      // Verify the WebUI input text is "suggestion-1".
      WaitForWebUIInputValue("suggestion-1"),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      // Verify the WebUI input text is still "suggestion-1".
      WaitForWebUIInputValue("suggestion-1"),
      // Verify the WebUI input has keyboard focus.
      CheckWebUIInputFocus(true));
}

// Verifies that clicking outside on the webpage body while an active user draft
// exists keeps the popup open while shifting focus to the webpage.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, ActiveUnfocusedDraft) {
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
      CheckWebUIInputFocus(false),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2),
      // Switch back to Tab 1.
      SwitchTab(kTabStripElementId, 1),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      // Verify the WebUI input text is "ffffff".
      WaitForWebUIInputValue("ffffff"),
      // Verify the WebUI input remains unfocused.
      CheckWebUIInputFocus(false));
}

// Verifies focusing the omnibox without typing a draft, switching away to
// another tab, and switching back restores focus to the omnibox.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, FocusOnlyNtp) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox (popup opens, but no draft exists yet).
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      // Verify the native Omnibox displays the page's permanent URL (since
      // no draft).
      WaitForOmniboxText(u"chrome://version"),
      // Verify the WebUI popup is open and focused (focus is restored to
      // Omnibox on tab switch back.).
      CheckWebUIInputFocus(true));
}

// Verifies switching to a tab where the webpage body is focused and has no
// omnibox draft to verify the popup remains closed.
// TODO(crbug.com/504668292): Failing on Windows, Linux, and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_BlurredPage DISABLED_BlurredPage
#else
#define MAYBE_BlurredPage BlurredPage
#endif
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, MAYBE_BlurredPage) {
  RunTestSequence(
      // Setup Tab 1 with open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Navigate to a normal webpage on Tab 2 (Omnibox is blurred, webpage
      // has focus).
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2), ClickWebPageBody(kTab2),
      WaitForOmniboxFocus(false), UninstrumentWebContents(kPopupWebView),
      // Switch to Tab 1 (focused).
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      CheckWebUIInputFocus(true),
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

// Verifies clearing omnibox then manual blurring.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, ClearAndManualBlur) {
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
      CheckWebUIInputFocus(true),
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, ClearAndSwitchTab) {
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
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      // Verify that SaveStateToTab reverted the cleared draft, restoring the
      // permanent URL of Tab 1 instead of an empty string.
      WaitForOmniboxText(u"chrome://version"));
}

// Verifies that opening multiple New Tab Pages (NTPs) consecutively focuses the
// WebUI Omnibox input right away for each new tab, and that non-NTPs do not
// have lingering focus.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       OmniboxFocusDoesNotLingerAcrossTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab3);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab4);

  RunTestSequence(
      // Open NTP Tab 1.
      WaitForPopupTransitionLockout(),
      AddInstrumentedTab(kTab1, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab1),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure webui input is focused.
      CheckWebUIInputFocus(true),

      // Open NTP Tab 2.
      UninstrumentWebContents(kPopupWebView),
      AddInstrumentedTab(kTab2, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure webui input is focused.
      CheckWebUIInputFocus(true),

      // Open non-NTP, expect focus to not linger.
      UninstrumentWebContents(kPopupWebView),
      AddInstrumentedTab(kTab3, GURL("about:blank")),
      WaitForWebContentsReady(kTab3),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      // Ensure omnibox is not focused.
      WaitForOmniboxFocus(false),

      // Open a third NTP.
      AddInstrumentedTab(kTab4, GURL(chrome::kChromeUINewTabURL)),
      WaitForWebContentsReady(kTab4),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      // Ensure webui input is focused.
      CheckWebUIInputFocus(true));
}

// Verifies that clicking a match navigates to the suggestion.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, ClickMatch) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      // Wait for the first suggestion to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),
      InAnyContext(
          WaitForElementToRender(kPopupWebView, kFirstSuggestionMatch)),
      // Click the first suggestion.
      InSameContext(ClickElement(kPopupWebView, kFirstSuggestionMatch)),
      // Verify navigation occurs.
      WaitForGoogleSearch(kTab1, {{"q", "suggestion-1"}}));
}

// Verifies ESC key staged unwinding parity across all 4 stages:
// Stage 1: Revert temporary text (kRevertTemporaryText)
// Stage 2: Close open suggestion popup (kClosePopup)
// Stage 3: Clear user input / revert to active page URL (kClearUserInput)
// Stage 4: Clear focus / blur Omnibox (kBlur)
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, EscapeStagedUnwinding) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      // Open Tab 1 at permanent URL chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Replace the permanent URL with "a" and select the first suggestion.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_DOWN, ui::EF_NONE)),
      WaitForWebUIInputValue("suggestion-1"), CheckWebUIInputFocus(true),

      // Stage 1: Send ESC to revert temporary text back to "a".
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForWebUIInputValue("a"),
      // Verify popup frame remains visible.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      CheckWebUIInputFocus(true), Do([&]() {
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
      CheckWebUIInputFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/2 /* kClosePopup */, 1);
      }),

      // Stage 3: Send ESC to clear user input draft and restore permanent page
      // URL.
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      // Wait for input value to revert to permanent page URL.
      WaitForWebUIInputValue("chrome://version"), CheckWebUIInputFocus(true),
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       EscapeStagedUnwinding_EmptyPermanentUrl) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      // Open Tab 1 at NTP (empty permanent URL) and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL(chrome::kChromeUINewTabURL)),
      // Type "a" into the WebUI input field.
      InputWebUIText("a"),
      // Wait for suggestion-1 match to appear.
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),

      // Stage 2: Send ESC to close open suggestion popup while retaining typed
      // text "a".
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && !el.dropdownIsVisible"),
      CheckWebUIInputFocus(true), Do([&]() {
        histogram_tester.ExpectBucketCount("Omnibox.Escape",
                                           /*sample=*/2 /* kClosePopup */, 1);
      }),

      // Stage 3: Send ESC to clear typed draft "a" and restore empty URL (NTP).
      WaitForPopupTransitionLockout(),
      InAnyContext(SendKeyPress(kPopupWebView, ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForWebUIInputValue(""), CheckWebUIInputFocus(true), Do([&]() {
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       TabSwitchDoesNotReopenDropdown) {
  RunTestSequence(
      // 1. Open Tab 1 at chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // 2. Type "a" into the WebUI input to open suggestions.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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

// Verifies that clicking a bookmark button in the bookmarks bar situated
// directly below the Omnibox while the Full WebUI Omnibox popup is open
// and focused dismisses the popup and navigates to the bookmarked URL.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       ClickBookmarksBarWhenOmniboxFocused) {
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
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      CheckWebUIInputFocus(true),
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, OnPaste) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      ClearWebUIText(), PasteWebUIText("example.com"),
      WaitForWebUIInputValue("example.com"), CheckWebUIInputFocus(true),
      Do([&]() { histogram_tester.ExpectBucketCount("Omnibox.Paste", 1, 1); }));
}

// Verifies that clicking on the webpage content area closes the popup.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       ClickWebpageClosesPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      ClearWebUIText(), ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// Verifies that clicking the location bar keeps the popup open.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       ClickLocationBarKeepsPopupOpen) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      MoveMouseTo(kOmniboxElementId), ClickMouse(),
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

#if !BUILDFLAG(IS_MAC)
// Verifies that pressing Shift+Arrow keys after Select All adjusts the
// selection character-by-character on Windows and Linux rather than collapsing
// all text at once. On macOS, selections created by Select All are undirected
// by OS convention and do not adjust character-by-character.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       AltEnterOpensForegroundTabUnfocusedOmnibox) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       AltShiftEnterOpensBackgroundTabUnfocusedOmnibox) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
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
      CheckWebUIInputFocus(true));
}

// Verifies that pressing Shift+Enter on a match opens the result in a new
// window and resets the original window's omnibox popup state to steady state.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       ShiftEnterOpensNewWindowAndResetsOmnibox) {
  RunTestSequence(
      // 1. Open Tab 1 at chrome://version/ and focus Omnibox.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // 2. Type "a" into the WebUI input to open suggestions.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, OnCopy) {
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       EnterSubmitsVerbatimUrlOnOpenPage) {
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
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       AltEnterOpensInNewForegroundTab) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"), InstrumentNextTab(kTab2),
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN, ui::EF_ALT_DOWN),
      WaitForWebContentsReady(kTab2),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForOmniboxFocus(false));
}

// Verifies that pressing Ctrl+L / Cmd+L while typing a query selects the typed
// text and keeps the suggestions dropdown open without interruption.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       RefocusWhileTypingPreservesDropdownAndSelectsText) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Type "a" and wait for suggestions dropdown.
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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
      InAnyContext(CheckWebUIInputFocus(true)),
      // Verify suggestions dropdown remains open.
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"));
}

// Verifies that navigating to the Omnibox via Tab traversal opens and focuses
// the full WebUI popup instead of retaining focus in the native textfield.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       TabTraversalOpensAndFocusesWebUIPopup) {
  RunTestSequence(
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      WaitForWebUIInputValue("chrome://version"),

      // Blur and close the Omnibox popup by clicking the webpage body.
      ClickWebPageBody(kTab1),
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)),
      UninstrumentWebContents(kPopupWebView), WaitForOmniboxFocus(false),
      WaitForPopupTransitionLockout(),

      // Focus the view directly before the Omnibox.
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* omnibox_view = browser_view->GetLocationBarView()->omnibox_view();
        auto* focus_manager = browser_view->GetFocusManager();
        auto* prev_view = focus_manager->GetNextFocusableView(
            omnibox_view, nullptr, /*reverse=*/true,
            /*dont_loop=*/false);
        CHECK(prev_view);
        prev_view->RequestFocus();
      }),

      // Traverse focus into the Omnibox using Tab.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_TAB, ui::EF_NONE),

      // Verify the WebUI popup opens and gains focus.
      InAnyContext(
          InstrumentNonTabWebView(kPopupWebView, GetActivePopupWebView())),
      InSameContext(WaitForWebContentsReady(
          kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))),
      InAnyContext(CheckWebUIInputFocus(true)),
      // Verify that the native omnibox textfield does not retain focus.
      WaitForOmniboxFocus(false));
}

class FullWebUIOmniboxAimInteractiveTestBase
    : public FullWebUIOmniboxInteractiveTestBase {
 public:
  FullWebUIOmniboxAimInteractiveTestBase() = default;
  ~FullWebUIOmniboxAimInteractiveTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      bool force_enable_aim) {
    std::vector<base::test::FeatureRefAndParams> features = {
        {omnibox::kWebUIOmniboxFullPopup, {}},
        {omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady, {}}};
    if (force_enable_aim) {
      features.emplace_back(omnibox::internal::kWebUIOmniboxAimPopup,
                            base::FieldTrialParams());
      base::FieldTrialParams simplification_params = {
          {omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
           "below_results"},
          {omnibox::kHideClassicContextButton.name, "false"},
          {omnibox::kShowLensSearchChip.name, "true"}};
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
};

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
            {"Omnibox_ContextButtonShapeIsOblong", "true"},
            {"Omnibox_ContextButtonShowSuggestionLabel", "true"}});
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {omnibox::internal::kWebUIOmniboxPopup,
                           omnibox::kAimServerEligibilityEnabled,
                           omnibox::kAimFuseboxEligibilityCheckEnabled});
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
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
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
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      InAnyContext(WaitForElementToRender(kPopupWebView, kContextButton)),
      InAnyContext(WaitForStateChange(kPopupWebView, style_applied)));
}

IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxSimplificationInteractiveTest,
                       HasSuggestionLabel) {
  const DeepQuery kSuggestionLabel = {
      "omnibox-full-app",
      "omnibox-popup-searchbox",
      "omnibox-popup-contextual-entrypoint",
      "#context",
      "cr-composebox-contextual-entrypoint-button",
      "#description"};
  std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL);
  RunTestSequence(
      SetAimEligibleResponse(),
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InAnyContext(WaitForOmniboxAimStateReady(kPopupWebView)),
      InputWebUIText("a"),
      WaitForMatch(kPopupWebView, kFirstSuggestionMatchContents,
                   "suggestion-1"),
      WaitForJsConditionAt(kPopupWebView, kPopupSearchbox,
                           "(el) => el && el.dropdownIsVisible"),
      InAnyContext(WaitForElementToRender(kPopupWebView, kSuggestionLabel)),
      InSameContext(CheckJsResultAt(kPopupWebView, kSuggestionLabel,
                                    "el => el.textContent.trim()",
                                    base::UTF16ToUTF8(expected_text))));
}
