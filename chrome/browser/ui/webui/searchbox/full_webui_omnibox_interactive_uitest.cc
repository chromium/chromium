// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kWebUIInput = {"omnibox-full-app", "omnibox-popup-searchbox",
                               "cr-searchbox-input", "#input"};
const DeepQuery kFirstSuggestionMatch = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='1']"};
const DeepQuery kFirstSuggestionMatchContents = {
    "omnibox-full-app", "omnibox-popup-searchbox", "cr-searchbox-dropdown",
    "cr-searchbox-match[match-index='1']", "#contents"};
}  // namespace

class FullWebUIOmniboxInteractiveTest
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  FullWebUIOmniboxInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kWebUIOmniboxFullPopup, {}}},
        {});
  }
  ~FullWebUIOmniboxInteractiveTest() override = default;

 protected:
  auto GetActivePopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar_view()
              ->GetOmniboxPopupView());
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
    return Steps(InSameContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                                           base::StringPrintf(R"(el => {
               const fullText = '%s';
               for (let i = 0; i < fullText.length; i++) {
                 el.value = fullText.substring(0, i + 1);
                 el.dispatchEvent(new Event('input'));
               }
             })",
                                                              text.c_str()))),
                 InAnyContext(WaitForWebUIInputValue(text)));
  }

  auto ClearWebUIText() {
    return Steps(InSameContext(ExecuteJsAt(kPopupWebView, kWebUIInput,
                                           R"(el => {
               el.value = '';
               el.dispatchEvent(new Event('input'));
             })")),
                 InAnyContext(WaitForWebUIInputValue("")));
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
    DEFINE_LOCAL_POLLING_VIEW_PROPERTY_STATE_IDENTIFIER(views::View, HasFocus,
                                                        kOmniboxHasFocusState);
    return Steps(PollViewProperty(kOmniboxHasFocusState, kOmniboxElementId),
                 WaitForState(kOmniboxHasFocusState, expected_focus),
                 StopObservingState(kOmniboxHasFocusState));
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
    return Steps(FocusWebContents(tab_id),
                 ExecuteJsAt(tab_id, DeepQuery{"body"}, "el => el.focus()"),
                 WaitForPopupTransitionLockout());
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
    return Steps(AddInstrumentedTab(tab_id, url),
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
// TODO(b/504668292): Re-enable after de-flaking on Windows/ChromeOS.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       DISABLED_ActiveUnfocusedDraft) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      InputWebUIText("ffffff"),
      // Click on the webpage body of Tab 1 to blur the Omnibox.
      ClickWebPageBody(kTab1),
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
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"chrome://version"),
      // Verify the WebUI popup is open and focused (focus is restored to
      // Omnibox on tab switch back.).
      CheckWebUIInputFocus(true));
}

// Verifies switching to a tab where the webpage body is focused and has no
// omnibox draft to verify the popup remains closed.
// TODO(b/504668292): Re-enable after de-flaking on Windows/ChromeOS.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest, DISABLED_BlurredPage) {
  RunTestSequence(
      // Setup Tab 1.
      AddInstrumentedTab(kTab1, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab1),
      // Navigate to a normal webpage on Tab 2 (Omnibox is blurred, webpage
      // has focus).
      AddInstrumentedTab(kTab2, GURL("chrome://version/")),
      WaitForWebContentsReady(kTab2), ClickWebPageBody(kTab2),
      WaitForOmniboxFocus(false),
      // Switch to Tab 1 (focused).
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      CheckWebUIInputFocus(true),
      // Switch back to Tab 2.
      SwitchTab(kTabStripElementId, 2), FocusWebContents(kTab2),
      // Verify the native Omnibox displays the page's permanent URL.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"chrome://version"),
      // Verify keyboard focus remains on the webpage body (Omnibox is
      // unfocused).
      WaitForOmniboxFocus(false),
      // Verify the WebUI popup is closed.
      InAnyContext(WaitForHide(OmniboxPopupPresenter::kRoundedResultsFrame)));
}

// TODO(b/504668292): Re-enable after de-flaking on Windows/ChromeOS.
// Verifies clearing omnibox then manual blurring in one tab then switching to
// another and back.
IN_PROC_BROWSER_TEST_F(FullWebUIOmniboxInteractiveTest,
                       DISABLED_ClearAndManualBlur) {
  RunTestSequence(
      // Open Tab 1 and focus Omnibox to open WebUI popup.
      OpenInitialTabAndFocusOmnibox(kTab1, GURL("chrome://version/")),
      // Type "ffffff" into the WebUI.
      InputWebUIText("ffffff"),
      // Clear the input text.
      ClearWebUIText(),
      // Verify the native Omnibox text is also empty.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text, u""),
      // Click the webpage body (triggering blur).
      ClickWebPageBody(kTab1),
      // Verify Omnibox popup remains open, the text is empty, and the
      // Omnibox is unfocused.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text, u""),
      WaitForWebUIInputValue(""), WaitForOmniboxFocus(false),
      // Click back inside the Omnibox (triggering focus).
      Do([this]() {
        if (auto* popup_view = BrowserWindow::FromBrowser(browser())
                                   ->GetLocationBar()
                                   ->GetOmniboxPopupView()) {
          popup_view->OnFocus(/*query_zps=*/true);
        }
      }),
      // Verify popup is open, text remains empty, and WebUI input is
      // focused.
      InAnyContext(WaitForShow(OmniboxPopupPresenter::kRoundedResultsFrame)),
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text, u""),
      WaitForWebUIInputValue(""), CheckWebUIInputFocus(true));
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
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text, u""),
      // Switch to Tab 2.
      AddInstrumentedTab(kTab2, GURL("about:blank")),
      WaitForWebContentsReady(kTab2), UninstrumentWebContents(kPopupWebView),
      // Switch back to Tab 1.
      SwitchTabAndRestorePopup(kTabStripElementId, 1, kTab1),
      // Verify that SaveStateToTab reverted the cleared draft, restoring the
      // permanent URL of Tab 1 instead of an empty string.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"chrome://version"));
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
      // Click the first suggestion.
      InSameContext(ClickElement(kPopupWebView, kFirstSuggestionMatch)),
      // Verify navigation occurs.
      WaitForGoogleSearch(kTab1, {{"q", "suggestion-1"}}));
}
