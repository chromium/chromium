// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/textfield/textfield.h"
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

const DeepQuery kAimInput = {"omnibox-aim-app", "cr-composebox", "#input"};
const DeepQuery kVoiceSearch = {"omnibox-aim-app", "cr-composebox",
                                "#voiceSearch"};
const DeepQuery kCancelIcon = {"omnibox-aim-app", "cr-composebox",
                               "#cancelIcon"};
const DeepQuery kAimSubmit = {"omnibox-aim-app", "cr-composebox",
                              "#submitContainer"};
}  // namespace

class OmniboxWebUiInteractiveTestBase
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  OmniboxWebUiInteractiveTestBase() = default;
  ~OmniboxWebUiInteractiveTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      bool enable_aim_popup,
      std::optional<bool> auto_submit_voice = std::nullopt) {
    std::vector<base::test::FeatureRefAndParams> features = {
        {omnibox::kWebUIOmniboxPopup, {}}};
    if (enable_aim_popup) {
      base::FieldTrialParams aim_params = {
          {omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
           "below_results"}};
      if (auto_submit_voice.has_value()) {
        aim_params[omnibox::kAutoSubmitVoiceSearchQuery.name] =
            auto_submit_voice.value() ? "true" : "false";
      }
      features.emplace_back(omnibox::internal::kWebUIOmniboxAimPopup,
                            aim_params);
      features.emplace_back(omnibox::kAiModeOmniboxEntryPoint,
                            base::FieldTrialParams());
      features.emplace_back(
          features::kPageActionsMigration,
          base::FieldTrialParams(
              {{features::kPageActionsMigrationAiMode.name, "true"}}));
    }
    return features;
  }

  auto WaitForGoogleSearch(const ui::ElementIdentifier& tab_id,
                           const std::string& query) {
    return Steps(
        WaitForWebContentsNavigation(tab_id),
        CheckResult(
            [this]() {
              return browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL()
                  .spec();
            },
            testing::StartsWith("https://www.google.com/search?q=" + query)));
  }
};

class OmniboxWebUiInteractiveTest : public OmniboxWebUiInteractiveTestBase {
 public:
  OmniboxWebUiInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*enable_aim_popup=*/false), {});
  }

 protected:
  // Returns the currently visible `OmniboxPopupWebUIContent`. An
  // `OmniboxPopupView` may host multiple content views, but only one is
  // visible at any given time.
  auto GetActivePopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar_view()
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
// TODO(crbug.com/459704336): Re-enable after de-flaking.
#if BUILDFLAG(IS_MAC)
#define MAYBE_GeminiHidesNullMatch DISABLED_GeminiHidesNullMatch
#else
#define MAYBE_GeminiHidesNullMatch GeminiHidesNullMatch
#endif
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest,
                       MAYBE_GeminiHidesNullMatch) {
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
      WaitForGoogleSearch(kNewTab, "%40gemini&oq=%40gemini"));
}

class OmniboxAimWebUiInteractiveTestBase
    : public PageActionInteractiveTestMixin<OmniboxWebUiInteractiveTestBase> {
 public:
  OmniboxAimWebUiInteractiveTestBase() = default;
  ~OmniboxAimWebUiInteractiveTestBase() override = default;

 protected:
  auto GetActiveAimPopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* aim_presenter = static_cast<OmniboxPopupAimPresenter*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar_view()
              ->GetOmniboxPopupAimPresenter());
      return aim_presenter->GetWebUIContent();
    });
  }

  auto WaitForAimPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(
            InstrumentNonTabWebView(kPopupWebView, GetActiveAimPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kPopupWebView, GURL(chrome::kChromeUIOmniboxPopupAimURL))));
  }

  // Opens the AIM popup by clicking the page action icon.
  auto OpenAimPopup() {
    return Steps(
        WaitForPageActionChipVisible(kActionAiMode),
        FocusElement(kOmniboxElementId),
        PressButton(kAiModePageActionIconElementId), WaitForAimPopupReady(),
        InAnyContext(WaitForElementToRender(kPopupWebView, kAimInput)));
  }

  // Opens the AIM popup in a new tab to ensure a clean state.
  auto OpenAimPopupInNewTab() {
    return Steps(AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUINewTabURL)),
                 OpenAimPopup());
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
    return WaitForStateChange(contents_id, value_changed);
  }

  auto InputAimPopupText(const std::string& text) {
    return Steps(
        InSameContext(ExecuteJsAt(
            kPopupWebView, kAimInput,
            base::StringPrintf("el => { el.value = '%s'; "
                               "el.dispatchEvent(new Event('input')); }",
                               text.c_str()))),
        InAnyContext(WaitForAimInputValue(kPopupWebView, kAimInput, text)));
  }

  auto RemoveFocusFromPopup() {
    return Steps(InAnyContext(MoveMouseTo(kToolbarAppMenuButtonElementId)),
                 InSameContext(ClickMouse()),
                 InAnyContext(WaitForHide(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame)));
  }

  auto TriggerAimVoiceSearch(const std::string& result,
                             bool auto_submit = false) {
    auto steps = Steps(InSameContext(ExecuteJsAt(
        kPopupWebView, kVoiceSearch,
        base::StringPrintf("el => el.dispatchEvent(new CustomEvent("
                           "'voice-search-final-result', "
                           "{detail: '%s', bubbles: true, composed: true}))",
                           result.c_str()))));
    if (!auto_submit) {
      steps +=
          InSameContext(WaitForAimInputValue(kPopupWebView, kAimInput, result));
    }
    return steps;
  }
};

class OmniboxAimWebUiInteractiveTest
    : public OmniboxAimWebUiInteractiveTestBase {
 public:
  OmniboxAimWebUiInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*enable_aim_popup=*/true),
        {omnibox::kAimServerEligibilityEnabled});
  }

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest,
                       RestoresFocusToLocationBarOnHideWithScreenReader) {
  RunTestSequence(
      // Enable screen reader mode.
      Do([this]() {
        scoped_accessibility_mode_ =
            content::BrowserAccessibilityState::GetInstance()
                ->CreateScopedModeForProcess(ui::AXMode::kScreenReader);
      }),
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Verify popup's web contents have focus.
      CheckJsResult(kPopupWebView, "() => document.hasFocus()", true),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false),
      // Hide the popup.
      InAnyContext(ExecuteJsAt(kPopupWebView, kCancelIcon, "el => el.click()")),
      InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Verify location bar has focus.
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest,
                       FocusesWebContentsOnNavigationWithScreenReader) {
  RunTestSequence(
      // Enable screen reader mode.
      Do([this]() {
        scoped_accessibility_mode_ =
            content::BrowserAccessibilityState::GetInstance()
                ->CreateScopedModeForProcess(ui::AXMode::kScreenReader);
      }),
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Verify web contents have focus.
      CheckJsResult(kPopupWebView, "() => document.hasFocus()", true),
      // Trigger a search.
      InputAimPopupText("foo"),
      InSameContext(ClickElement(kPopupWebView, kAimSubmit)),
      WaitForGoogleSearch(kNewTab, "foo"),
      // Verify tab has focus and not the location bar.
      CheckJsResult(kNewTab, "() => document.hasFocus()", true),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest, TextTransfersOnDismiss) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Write something into the input field.
      InputAimPopupText("foo"),
      // Close the popup by removing focus from it.
      RemoveFocusFromPopup(),
      // Ensure text transfers to the Omnibox.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text, u"foo"));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest, TextTransfersOnEscape) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Write something into the input field.
      InputAimPopupText("foo bar"),
      // Press Escape to close the popup.
      SendKeyPress(kOmniboxElementId, ui::VKEY_ESCAPE),
      InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Ensure text transfers to the Omnibox.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"foo bar"));
}

class OmniboxAimNoAutoSubmitVoiceTest
    : public OmniboxAimWebUiInteractiveTestBase {
 public:
  OmniboxAimNoAutoSubmitVoiceTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*enable_aim_popup=*/true,
                           /*auto_submit_voice=*/false),
        {omnibox::kAimServerEligibilityEnabled});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAimNoAutoSubmitVoiceTest,
                       VoiceTextClearsOnCancel) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Simulate a voice search result event.
      TriggerAimVoiceSearch("foo"),
      // Click the cancel button to clear the query.
      InSameContext(
          ExecuteJsAt(kPopupWebView, kCancelIcon, "el => el.click()")),
      // Ensure input field is cleared in the popup.
      InSameContext(WaitForAimInputValue(kPopupWebView, kAimInput, "")),
      // Click the cancel button again to close the popup.
      InSameContext(
          ExecuteJsAt(kPopupWebView, kCancelIcon, "el => el.click()")),
      InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Ensure text didn't transfer to the Omnibox.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          std::u16string()));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimNoAutoSubmitVoiceTest,
                       TextTransfersOnDismiss) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Simulate a voice search result event.
      TriggerAimVoiceSearch("foo bar"),
      // Close the popup by removing focus from it.
      RemoveFocusFromPopup(),
      // Ensure text transfers to the Omnibox.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"foo bar"));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimNoAutoSubmitVoiceTest, TextTransfersOnEscape) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Simulate a voice search result event.
      TriggerAimVoiceSearch("foo bar baz"),
      // Press Escape to close the popup.
      SendKeyPress(kOmniboxElementId, ui::VKEY_ESCAPE),
      InAnyContext(
          WaitForHide(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
      // Ensure text transfers to the Omnibox.
      WaitForViewProperty(kOmniboxElementId, views::Textfield, Text,
                          u"foo bar baz"));
}

struct AimSearchParam {
  bool is_voice = false;
  bool submit_via_keyboard = false;
  bool auto_submit_voice = false;
};

class OmniboxAimSearchFulfillmentTest
    : public OmniboxAimWebUiInteractiveTestBase,
      public testing::WithParamInterface<AimSearchParam> {
 public:
  OmniboxAimSearchFulfillmentTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*enable_aim_popup=*/true,
                           /*auto_submit_voice=*/GetParam().auto_submit_voice),
        {omnibox::kAimServerEligibilityEnabled});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxAimSearchFulfillmentTest,
    testing::Values(
        AimSearchParam{},
        AimSearchParam{.submit_via_keyboard = true},
        AimSearchParam{.is_voice = true},
        AimSearchParam{.is_voice = true, .submit_via_keyboard = true},
        AimSearchParam{.is_voice = true, .auto_submit_voice = true}),
    [](const testing::TestParamInfo<AimSearchParam>& info) {
      if (info.param.auto_submit_voice) {
        return std::string("VoiceAutoSubmit");
      }
      return base::StringPrintf(
          "%s%s", info.param.is_voice ? "Voice" : "Typed",
          info.param.submit_via_keyboard ? "Keyboard" : "Click");
    });

IN_PROC_BROWSER_TEST_P(OmniboxAimSearchFulfillmentTest,
                       SearchNavigatesOnSubmit) {
  const AimSearchParam& param = GetParam();
  const std::string query = "query";
  RunTestSequence(
      // Open a new tab to ensure a clean state.
      AddInstrumentedTab(kNewTab, GURL(chrome::kChromeUINewTabURL)),
      // Open the AIM popup.
      OpenAimPopup(),
      // Write something into the input field.
      param.is_voice ? TriggerAimVoiceSearch(query, param.auto_submit_voice)
                     : InputAimPopupText(query),
      // Submit query by pressing enter key or by clicking the submit button.
      // Skip this step if voice search auto-submits.
      If([&]() { return !param.is_voice || !param.auto_submit_voice; },
         Then(param.submit_via_keyboard
                  ? InAnyContext(
                        SendKeyPress(kOmniboxElementId, ui::VKEY_RETURN))
                  : InSameContext(ClickElement(kPopupWebView, kAimSubmit)))),
      // Ensure tab navigates to a Google search results page.
      InAnyContext(WaitForGoogleSearch(kNewTab, query)));
}
