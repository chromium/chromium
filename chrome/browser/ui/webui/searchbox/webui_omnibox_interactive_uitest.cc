// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/webview/webview.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kDropdownContent = {"omnibox-popup-app",
                                    "cr-searchbox-dropdown", "#content"};
const DeepQuery kMatch = {"omnibox-popup-app", "cr-searchbox-dropdown",
                          "cr-searchbox-match"};
const DeepQuery kMatchText = {"omnibox-popup-app", "cr-searchbox-dropdown",
                              "cr-searchbox-match", "#suggestion"};
}  // namespace

class OmniboxWebUiInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  OmniboxWebUiInteractiveTest() {
    feature_list_.InitAndEnableFeature(omnibox::kWebUIOmniboxPopup);
  }
  ~OmniboxWebUiInteractiveTest() override = default;

 protected:
  // Returns the currently visible `OmniboxPopupWebUIContent`. An
  // `OmniboxPopupView` may host multiple content views, but only one is
  // visible at any given time.
  auto GetActivePopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar()
              ->GetOmniboxPopupViewForTesting());
      return popup_view->presenter_->GetWebUIContent();
    });
  }

  auto WaitForPopupReady() {
    return Steps(InAnyContext(WaitForShow(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame)),
                 InAnyContext(InstrumentNonTabWebView(kPopupWebView,
                                                      GetActivePopupWebView())),
                 InSameContext(WaitForWebContentsReady(
                     kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))));
  }

  // Enters Gemini mode in the omnibox and waits for the popup to be ready.
  auto EnterGeminiMode() {
    return Steps(FocusElement(kOmniboxElementId),
                 EnterText(kOmniboxElementId, u"@gemini"),
                 SendKeyPress(kOmniboxElementId, ui::VKEY_TAB),
                 WaitForPopupReady());
  }

  auto WaitForElementToHide(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHides);
    StateChange element_hides;
    element_hides.event = kElementHides;
    element_hides.where = element;
    element_hides.test_function =
        "(el) => { let rect = el.getBoundingClientRect(); return rect.width "
        "=== 0 && rect.height === 0; }";
    return WaitForStateChange(contents_id, element_hides);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensures dropdown resurfaces if it goes away during an Omnibox session.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, PopupResurfaces) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUINewTabURL)),
      EnterGeminiMode(),
      // With a query entered, no matches should show.
      EnterText(kOmniboxElementId, u"q"),
      InAnyContext(WaitForElementToHide(kPopupWebView, kDropdownContent)),
      // Pressing backspace should surface matches.
      SendKeyPress(kOmniboxElementId, ui::VKEY_BACK),
      InAnyContext(WaitForElementToRender(kPopupWebView, kMatchText)));
}

// Ensures matches show in Gemini mode when there is input, and that
// pressing enter still navigates to Gemini.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, GeminiHidesVerbatimMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUINewTabURL)),
      EnterGeminiMode(),
      // With a query entered, no suggestion match should be shown.
      EnterText(kOmniboxElementId, u"query"),
      InAnyContext(WaitForElementToHide(kPopupWebView, kDropdownContent)),
      // Confirming should navigate to the Gemini URL.
      Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(
          kNewTab, GURL(OmniboxFieldTrial::kGeminiUrlOverride.Get())));
}

// Ensures Gemini mode's null match; e.g. "<Type search term>" is hidden, and
// that clicking the default search suggestion navigates correctly.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, GeminiHidesNullMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUINewTabURL)),
      EnterGeminiMode(),
      // Ensure the initial match is the default search suggestion.
      InAnyContext(WaitForElementToRender(kPopupWebView, kMatchText)),
      InSameContext(CheckJsResultAt(kPopupWebView, kMatchText,
                                    "(el) => el.textContent.replace(/\\s+/g, ' "
                                    "').trim() === '@gemini - Google Search'")),
      // Clicking the top match should navigate to a Google search results page.
      InSameContext(ClickElement(kPopupWebView, kMatch)),
      WaitForWebContentsNavigation(kNewTab),
      CheckResult(
          [this]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL()
                .spec();
          },
          testing::StartsWith(
              "https://www.google.com/search?q=%40gemini&oq=%40gemini")));
}
