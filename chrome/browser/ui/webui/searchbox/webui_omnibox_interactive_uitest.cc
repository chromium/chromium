// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_interactive_test_mixin.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_search/mock_contextual_search_service.h"
#include "components/contextual_search/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClassicPopupWebView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAimPopupWebView);
using HeightObserver = views::test::PollingViewObserver<int, views::View>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(HeightObserver, kAimPopupHeightState);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kClassicContextMenu = {"omnibox-popup-app", "#context"};
const DeepQuery kDropdownContent = {"omnibox-popup-app",
                                    "cr-searchbox-dropdown", "#content"};
const DeepQuery kOmniboxPopup = {"omnibox-popup-app"};
const DeepQuery kClassicMatch = {"omnibox-popup-app", "cr-searchbox-dropdown",
                                 "cr-searchbox-match"};
const DeepQuery kClassicMatchText = {"omnibox-popup-app",
                                     "cr-searchbox-dropdown",
                                     "cr-searchbox-match", "#suggestion"};

const DeepQuery kAimInput = {"omnibox-aim-app", "cr-composebox",
                             "cr-composebox-input", "#input"};
const DeepQuery kVoiceSearch = {"omnibox-aim-app", "cr-composebox",
                                "#voiceSearch"};
const DeepQuery kCancelIcon = {"omnibox-aim-app", "cr-composebox",
                               "cr-composebox-input", "#cancelIcon"};
const DeepQuery kAimSubmit = {"omnibox-aim-app", "cr-composebox",
                              "cr-composebox-submit", "#submitContainer"};
const DeepQuery kComposeboxMatch1 = {"omnibox-aim-app", "cr-composebox",
                                     "#matches", "#match1", "#textContainer"};
const DeepQuery kComposeboxFileThumbnail = {"omnibox-aim-app", "cr-composebox",
                                            "cr-composebox-file-carousel",
                                            "cr-composebox-file-thumbnail"};
}  // namespace

class OmniboxWebUiInteractiveTestBase
    : public SearchboxInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  OmniboxWebUiInteractiveTestBase() = default;
  ~OmniboxWebUiInteractiveTestBase() override = default;

 protected:
  static std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures(
      bool force_enable_aim) {
    std::vector<base::test::FeatureRefAndParams> features = {
        {omnibox::internal::kWebUIOmniboxPopup, {}},
        {omnibox::kOmniboxWebUIDeferShowUntilVisualStateReady, {}}};
    if (force_enable_aim) {
      base::FieldTrialParams aim_params = {
          {omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
           "below_results"},
          {omnibox::kHideClassicContextButton.name, "false"}};
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

  // Returns the currently visible `OmniboxPopupWebUIContent`. An
  // `OmniboxPopupView` may host multiple content views, but only one is
  // visible at any given time.
  auto GetActiveClassicPopupWebView() {
    return base::BindLambdaForTesting([&]() -> views::View* {
      auto* popup_view = static_cast<OmniboxPopupViewWebUI*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->toolbar()
              ->location_bar_view()
              ->GetOmniboxPopupViewForTesting());
      return popup_view->presenter()->GetWebUIContent();
    });
  }

  auto WaitForClassicPopupReady() {
    return Steps(
        InAnyContext(
            WaitForShow(OmniboxPopupPresenterBase::kRoundedResultsFrame)),
        InAnyContext(InstrumentNonTabWebView(kClassicPopupWebView,
                                             GetActiveClassicPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kClassicPopupWebView, GURL(chrome::kChromeUIOmniboxPopupURL))));
  }
};

class OmniboxWebUiInteractiveTest : public OmniboxWebUiInteractiveTestBase {
 public:
  OmniboxWebUiInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*force_enable_aim=*/false), {});
  }

 protected:
  // Enters Gemini mode in the omnibox and waits for the popup to be ready.
  auto EnterGeminiMode() {
    return Steps(FocusElement(kOmniboxElementId),
                 EnterText(kOmniboxElementId, u"@gemini"),
                 SendKeyPress(kOmniboxElementId, ui::VKEY_TAB),
                 WaitForClassicPopupReady());
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
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // With a query entered, no matches should show.
      EnterText(kOmniboxElementId, u"q"),
      InAnyContext(
          WaitForElementToHide(kClassicPopupWebView, kDropdownContent)),
      // Pressing backspace should surface matches.
      SendKeyPress(kOmniboxElementId, ui::VKEY_BACK),
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kClassicMatchText)));
}

// Ensures matches show in Gemini mode when there is input, and that
// pressing enter still navigates to Gemini.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, GeminiHidesVerbatimMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // With a query entered, no suggestion match should be shown.
      EnterText(kOmniboxElementId, u"query"),
      InAnyContext(
          WaitForElementToHide(kClassicPopupWebView, kDropdownContent)),
      // Confirming should navigate to the Gemini URL.
      Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(
          kNewTab, GURL(OmniboxFieldTrial::kGeminiUrlOverride.Get())));
}

// Ensures Gemini mode's null match; e.g. "<Type search term>" is hidden, and
// that clicking the default search suggestion navigates correctly.
// TODO(crbug.com/496926191): Re-enable after de-flaking.
#if BUILDFLAG(IS_MAC)
#define MAYBE_GeminiHidesNullMatch DISABLED_GeminiHidesNullMatch
#else
#define MAYBE_GeminiHidesNullMatch GeminiHidesNullMatch
#endif
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest,
                       MAYBE_GeminiHidesNullMatch) {
  RunTestSequence(
      // Enter Gemini mode in Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      EnterGeminiMode(),
      // Ensure the initial match is the default search suggestion.
      WaitForVerbatimMatch(kClassicPopupWebView, kClassicMatchText, "@gemini"),
      // Clicking the top match should navigate to a Google search results page.
      InSameContext(ClickElement(kClassicPopupWebView, kClassicMatch)),
      WaitForGoogleSearch(kNewTab, {{"q", "@gemini"}, {"oq", "@gemini"}}));
}

// TODO(crbug.com/496926191): Interactive tests involving verbatim matches are
// fickle, especially in the WebUI Omnibox popup. Since we have to wait for the
// `SetPage` call in the `SearchboxHandler` to complete before the popup can
// receive results, we can't guarantee that this test will always pass.

// class OmniboxSubmitInteractiveTest : public OmniboxWebUiInteractiveTestBase,
//                                      public testing::WithParamInterface<bool>
//                                      {
//  public:
//   OmniboxSubmitInteractiveTest() {
//     feature_list_.InitWithFeaturesAndParameters(
//         GetEnabledFeatures(/*force_enable_aim=*/false), {});
//   }

//   bool ClickMatch() const { return GetParam(); }

//  private:
//   base::test::ScopedFeatureList feature_list_;
// };

// INSTANTIATE_TEST_SUITE_P(All, OmniboxSubmitInteractiveTest, testing::Bool());

// IN_PROC_BROWSER_TEST_P(OmniboxSubmitInteractiveTest,
//                        SubmittingInputNavigatesToSearch) {
//   RunTestSequence(
//       // Open the Omnibox with a seeded history result (to avoid flakiness).
//       InstrumentTab(kNewTab), SeedSearchboxResult("a"),
//       FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
//       WaitForClassicPopupReady(),
//       InAnyContext(WaitForVerbatimMatch(kPopupWebView, kClassicMatchText,
//       "a")),
//       // Click the match or press enter depending on the test parameter.
//       If([this]() { return ClickMatch(); },
//          Then(InAnyContext(
//              ExecuteJsAt(kPopupWebView, kClassicMatchText, "el =>
//              el.click()")
//                  .SetMustRemainVisible(false))),
//          Else(SendKeyPress(kOmniboxElementId, ui::VKEY_RETURN))),
//       // Ensure google search occurs.
//       WaitForGoogleSearch(kNewTab, {{"q", "a"}}));
// }
// Ensures that the entrypoint is not shown in the popup whenever the AIM popup
// feature is disabled.
IN_PROC_BROWSER_TEST_F(OmniboxWebUiInteractiveTest, AimEntryPointHidden) {
  RunTestSequence(
      // Open the Omnibox.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
      WaitForClassicPopupReady(),
      // Ensure that there's no context menu button in the popup.
      InAnyContext(
          EnsureNotPresent(kClassicPopupWebView, kClassicContextMenu)));
}

class OmniboxAimWebUiInteractiveTestBase
    : public PageActionInteractiveTestMixin<OmniboxWebUiInteractiveTestBase> {
 public:
  OmniboxAimWebUiInteractiveTestBase() = default;
  ~OmniboxAimWebUiInteractiveTestBase() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    PageActionInteractiveTestMixin<OmniboxWebUiInteractiveTestBase>::
        SetUpBrowserContextKeyedServices(context);
    ContextualSearchServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(BuildMockContextualSearchServiceInstance));
  }

 protected:
  auto SetAimEligibleResponse() {
    return Do([this]() {
      auto* profile = browser()->profile();
      auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
      omnibox::AimEligibilityResponse response;
      response.set_is_eligible(true);
      response.set_is_fusebox_eligible(true);
      response.set_is_cobrowse_eligible(true);
      auto* config = response.mutable_searchbox_config();
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
    });
  }

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
        InAnyContext(InstrumentNonTabWebView(kAimPopupWebView,
                                             GetActiveAimPopupWebView())),
        InSameContext(WaitForWebContentsReady(
            kAimPopupWebView, GURL(chrome::kChromeUIOmniboxPopupAimURL))));
  }

  // Opens the AIM popup by clicking the page action icon.
  auto OpenAimPopup() {
    return Steps(
        WaitForPageActionChipVisible(kActionAiMode),
        FocusElement(kOmniboxElementId),
        PressButton(kAiModePageActionIconElementId), WaitForAimPopupReady(),
        InAnyContext(WaitForElementToRender(kAimPopupWebView, kAimInput)));
  }

  // Opens the AIM popup in a new tab to ensure a clean state.
  auto OpenAimPopupInNewTab() {
    return Steps(AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
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

  auto WaitForAimSubmitEnabled(const ui::ElementIdentifier& contents_id) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimSubmitEnabled);
    StateChange submit_enabled;
    submit_enabled.event = kAimSubmitEnabled;
    submit_enabled.where = DeepQuery{"omnibox-aim-app", "cr-composebox"};
    submit_enabled.test_function = "(el) => el && el.canSubmitFilesAndInput";
    return Steps(WaitForElementToRender(contents_id, kAimSubmit),
                 WaitForStateChange(contents_id, submit_enabled));
  }

  auto InputAimPopupText(const std::string& text) {
    // Simulate character-by-character input to ensure all 'input' events are
    // fired and processed by the WebUI. This prevents flakiness that occurs
    // when setting the value all at once, which might miss intermediate state
    // updates.
    return Steps(
        InSameContext(ExecuteJsAt(kAimPopupWebView, kAimInput,
                                  base::StringPrintf(R"(el => {
              const fullText = '%s';
              for (let i = 0; i < fullText.length; i++) {
                el.value = fullText.substring(0, i + 1);
                el.dispatchEvent(new Event('input'));
              }
            })",
                                                     text.c_str()))),
        InAnyContext(WaitForAimInputValue(kAimPopupWebView, kAimInput, text)),
        InAnyContext(WaitForAimSubmitEnabled(kAimPopupWebView)));
  }

  auto RemoveFocusFromPopup() {
    return Steps(InAnyContext(MoveMouseTo(kToolbarAppMenuButtonElementId)),
                 InSameContext(ClickMouse()),
                 InAnyContext(WaitForHide(
                     OmniboxPopupPresenterBase::kRoundedResultsFrame)));
  }
};

class OmniboxAimWebUiInteractiveTest
    : public OmniboxAimWebUiInteractiveTestBase {
 public:
  OmniboxAimWebUiInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*force_enable_aim=*/true),
        {omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimFuseboxEligibilityCheckEnabled});
  }

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;

 protected:
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
      CheckJsResult(kAimPopupWebView, "() => document.hasFocus()", true),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false),
      // Hide the popup.
      InAnyContext(
          ExecuteJsAt(kAimPopupWebView, kCancelIcon, "el => el.click()")),
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
      CheckJsResult(kAimPopupWebView, "() => document.hasFocus()", true),
      // Trigger a search.
      InputAimPopupText("foo"),
      InSameContext(ClickElement(kAimPopupWebView, kAimSubmit)),
      WaitForGoogleSearch(kNewTab, {{"q", "foo"}}),
      // Verify tab has focus and not the location bar.
      CheckJsResult(kNewTab, "() => document.hasFocus()", true),
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false));
}

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest,
                       ClassicContextMenuOpensDeepSearch) {
  const DeepQuery kDeepSearchChip = {"omnibox-aim-app", "cr-composebox",
                                     "cr-composebox-tool-chip"};
  RunTestSequence(
      SetAimEligibleResponse(),
      // Open the classic popup.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
      WaitForClassicPopupReady(),
      // Wait for the context menu button to render in the popup.
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kClassicContextMenu)),
      MayInvolveNativeContextMenu(
          // Open the context menu and click "Deep Search".
          InSameContext(
              ClickElement(kClassicPopupWebView, kClassicContextMenu)),
          InAnyContext(WaitForShow(
              OmniboxContextMenuController::kDeepResearchIdForTesting)),
          InSameContext(SelectMenuItem(
              OmniboxContextMenuController::kDeepResearchIdForTesting)),
          // Wait for classic popup to hide and AIM popup to show.
          InAnyContext(WaitForHide(kClassicPopupWebView))),
      WaitForAimPopupReady(),
      // Wait for deep search chip to render in AIM popup.
      InAnyContext(WaitForElementToRender(kAimPopupWebView, kDeepSearchChip)));
}

// TODO(crbug.com/509753148): Re-enable after fixing pixel screenshots on all
// platforms.
#define MAYBE_QueryWithTabContext DISABLED_QueryWithTabContext
IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest,
                       MAYBE_QueryWithTabContext) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

  // Force a larger window size to give the popup room to grow.
  browser()->window()->SetBounds(gfx::Rect(0, 0, 1280, 1024));

  browser()->profile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementVisibleInViewport);
  StateChange visible_in_viewport;
  visible_in_viewport.event = kElementVisibleInViewport;
  visible_in_viewport.where = kClassicContextMenu;
  visible_in_viewport.test_function = R"(
    (el) => {
      const rect = el.getBoundingClientRect();
      return rect.top >= 0 && rect.bottom <= window.innerHeight;
    }
  )";

  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAimSubmitVisibleAndLayoutSettled);
  StateChange submit_visible_and_layout_settled;
  submit_visible_and_layout_settled.event = kAimSubmitVisibleAndLayoutSettled;
  submit_visible_and_layout_settled.where = {"omnibox-aim-app", "cr-composebox",
                                             "cr-composebox-submit",
                                             "#submitIcon"};
  submit_visible_and_layout_settled.test_function = R"(
    (el) => {
      const rect = el.getBoundingClientRect();
      return window.getComputedStyle(el).opacity === '1' &&
             rect.width > 0 &&
             rect.bottom <= window.innerHeight &&
             rect.top >= 0 &&
             document.body.scrollHeight <= window.innerHeight;
    }
  )";
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kClassicContextMenuExists);
  StateChange classic_context_menu_exists;
  classic_context_menu_exists.event = kClassicContextMenuExists;
  classic_context_menu_exists.where = kClassicContextMenu;
  classic_context_menu_exists.test_function = "(el) => el !== null";

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      // 1. Open a webpage and NTP in separate tabs.
      AddInstrumentedTab(kFirstTab, GURL("https://www.example.com/")),

      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),

      SetAimEligibleResponse(),
      // 2. Focus the Omnibox and type to trigger popup.
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),

      // 3. Wait for the popup to open and the chip to appear.
      WaitForClassicPopupReady(),
      InAnyContext(WaitForStateChange(kClassicPopupWebView,
                                      classic_context_menu_exists)),
      // Wait for the element to be within the viewport bounds
      InSameContext(
          WaitForStateChange(kClassicPopupWebView, visible_in_viewport)),

      // 4. Click the context menu and select the first tab.
      MayInvolveNativeContextMenu(
#if BUILDFLAG(IS_WIN)
          // On Windows with pixel tests enabled, physical clicks fail because
          // the element is outside the omnibox frame bounds. We call the Mojo
          // method directly via JS to bypass bounds checks.
          InSameContext(ExecuteJsAt(
              kClassicPopupWebView, {"omnibox-popup-app"},
              "el => el.popupPageHandler_.showContextMenu({x: 0, y: 0})")),
#else
          InSameContext(
              ClickElement(kClassicPopupWebView, kClassicContextMenu)),
#endif
          InAnyContext(WaitForShow(
              OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)),
          InSameContext(SelectMenuItem(
              OmniboxContextMenuController::kFirstTabMenuItemIdForTesting)),
          InAnyContext(WaitForHide(kClassicPopupWebView))),

      // 5. Verify that it transitions to AIM popup.
      WaitForAimPopupReady(),
      InAnyContext(WaitForElementToRender(kAimPopupWebView, kAimInput)),
      // Disable animations in the WebUI to ensure screenshot stability.
      // We set the custom property here because the AIM popup WebView is only
      // available after this point, and setting it earlier on the classic popup
      // or initial tab would not persist to this new document.
      InSameContext(ExecuteJsAt(kAimPopupWebView, {}, R"(
        () => {
          const style = document.createElement('style');
          style.textContent = '* { animation: none !important; transition: none !important; }';
          document.head.appendChild(style);
        }
      )")),

      // 6. Type a query and submit.
      InputAimPopupText("foo"),
      InSameContext(WaitForElementToRender(kAimPopupWebView, kAimSubmit)),
      InSameContext(WaitForStateChange(kAimPopupWebView,
                                       submit_visible_and_layout_settled)),
      // Wait for the native view to resize.
      InAnyContext(PollView(
          kAimPopupHeightState, OmniboxPopupPresenterBase::kRoundedResultsFrame,
          [](const views::View* view) { return view->height(); })),
      WaitForState(kAimPopupHeightState, testing::Optional(testing::Gt(170))),
      StopObservingState(kAimPopupHeightState),
      InAnyContext(Screenshot(kAimPopupWebView, "AimPopupQueryWithTabContext",
                              "7751707")),
      // Use JS click to avoid flakiness with simulated mouse clicks on Mac
      // and focus issues on Windows/Linux when sending keys to the Omnibox.
      InAnyContext(ExecuteJsAt(kAimPopupWebView, kAimSubmit, "el => el.click()")
                       .SetMustRemainVisible(false)),

      // 7. Verify navigation to Google Search with the query.
      WaitForGoogleSearch(kNewTab, {{"q", "foo"}}));
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

IN_PROC_BROWSER_TEST_F(OmniboxAimWebUiInteractiveTest,
                       AimPopupTypedSuggestions) {
  RunTestSequence(
      // Open the AIM popup.
      OpenAimPopupInNewTab(),
      // Write something into the input field.
      InputAimPopupText("foo bar"),
      // Wait for suggestion to appear.
      WaitForMatch(kAimPopupWebView, kComposeboxMatch1, "suggestion-1"),
      // Click the match.
      InSameContext(ClickElement(kAimPopupWebView, kComposeboxMatch1)),
      // Ensure Google search occurs.
      WaitForGoogleSearch(kNewTab, {{"q", "suggestion-1"}}));
}

struct AimSearchParam {
  bool is_voice = false;
  bool submit_via_keyboard = false;
};

class OmniboxAimSearchFulfillmentTest
    : public OmniboxAimWebUiInteractiveTestBase,
      public testing::WithParamInterface<AimSearchParam> {
 public:
  OmniboxAimSearchFulfillmentTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(/*force_enable_aim=*/true),
        {omnibox::kAimServerEligibilityEnabled,
         omnibox::kAimFuseboxEligibilityCheckEnabled});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxAimSearchFulfillmentTest,
    testing::Values(AimSearchParam{},
                    AimSearchParam{.submit_via_keyboard = true},
                    AimSearchParam{.is_voice = true}),
    [](const testing::TestParamInfo<AimSearchParam>& info) {
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
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      // Open the AIM popup.
      OpenAimPopup(),
      // Write something into the input field.
      param.is_voice
          ? TriggerAimVoiceSearch(kAimPopupWebView, kVoiceSearch, query)
          : InputAimPopupText(query),
      // Submit query by pressing enter key or by clicking the submit button.
      // Skip this step if voice search auto-submits.
      If([&]() { return !param.is_voice; },
         Then(param.submit_via_keyboard
                  ? InAnyContext(
                        SendKeyPress(kOmniboxElementId, ui::VKEY_RETURN))
                  : InSameContext(ClickElement(kAimPopupWebView, kAimSubmit)))),
      // Ensure tab navigates to a Google search results page.
      WaitForGoogleSearch(kNewTab, {{"q", query}}));
}

struct OmniboxAimUploadInteractiveTestParams {
  ui::ElementIdentifier upload_context_menu_item_id;
  std::string file_name;
};

class OmniboxAimUploadInteractiveTest
    : public OmniboxAimWebUiInteractiveTestBase,
      public testing::WithParamInterface<
          OmniboxAimUploadInteractiveTestParams> {
 public:
  OmniboxAimUploadInteractiveTest() {
    auto enabled_features = GetEnabledFeatures(/*force_enable_aim=*/true);
    enabled_features.emplace_back(omnibox::kAimUsePecApi,
                                  base::FieldTrialParams());
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features, {omnibox::kAimServerEligibilityEnabled,
                           omnibox::kAimFuseboxEligibilityCheckEnabled});
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    OmniboxAimWebUiInteractiveTestBase::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxAimUploadInteractiveTest,
    testing::ValuesIn(std::vector<OmniboxAimUploadInteractiveTestParams>{
        {
            .upload_context_menu_item_id =
                OmniboxContextMenuController::kImageUploadMenuItemIdForTesting,
            .file_name = "Image1.png",
        },
        {
            .upload_context_menu_item_id =
                OmniboxContextMenuController::kFileUploadMenuItemIdForTesting,
            .file_name = "File1.pdf",
        },
    }),
    [](const testing::TestParamInfo<OmniboxAimUploadInteractiveTestParams>&
           info) {
      std::string name = info.param.file_name;
      base::ReplaceChars(name, ".", "", &name);
      std::string prefix =
          info.param.upload_context_menu_item_id ==
                  OmniboxContextMenuController::kImageUploadMenuItemIdForTesting
              ? "ImageUpload"
              : "FileUpload";
      return prefix + name;
    });

IN_PROC_BROWSER_TEST_P(OmniboxAimUploadInteractiveTest,
                       ClassicContextMenuUploadTriggersAimPopup) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath file_path = test_data_dir.AppendASCII(GetParam().file_name);

  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));

  RunTestSequence(
      SetAimEligibleResponse(),
      // Open the classic popup.
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      // Seed a result to ensure the classic popup is visible.
      SeedSearchboxResult("result"),
      // Focus on the Omnibox and enter text to trigger the classic popup and
      // wait for it to be ready.
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"result"),
      WaitForClassicPopupReady(),
      // Wait for the context menu button to render.
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kClassicContextMenu)),
      InAnyContext(ScrollIntoView(kClassicPopupWebView, kClassicContextMenu)),
      MayInvolveNativeContextMenu(
          // Open the context menu and click upload.
          InSameContext(
              ClickElement(kClassicPopupWebView, kClassicContextMenu)),
          InAnyContext(WaitForShow(GetParam().upload_context_menu_item_id)),
          InAnyContext(SelectMenuItem(GetParam().upload_context_menu_item_id)),
          // Wait for classic popup to hide.
          InAnyContext(WaitForHide(kClassicPopupWebView))),
      // Wait for AIM popup to open.
      WaitForAimPopupReady(),
      // Wait for thumbnail to render in AIM popup.
      InAnyContext(
          WaitForElementToRender(kAimPopupWebView, kComposeboxFileThumbnail)),
      // Type a query and submit.
      InputAimPopupText("test"),
      InAnyContext(SendKeyPress(kOmniboxElementId, ui::VKEY_RETURN)),
      // Ensure Google search occurs.
      WaitForGoogleSearch(kNewTab, {{"q", "test"}}));
}

class WebUIOmniboxSimplificationInteractiveTest
    : public OmniboxAimWebUiInteractiveTestBase {
 public:
  WebUIOmniboxSimplificationInteractiveTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetEnabledFeatures(/*force_enable_aim=*/true);
    enabled_features.emplace_back(
        omnibox::internal::kWebUIOmniboxSimplification,
        base::FieldTrialParams{
            {"Omnibox_ContextButtonHasBackground", "true"},
            {"Omnibox_ContextButtonShapeIsOblong", "true"},
            {"Omnibox_ContextButtonShowSuggestionLabel", "true"}});
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIOmniboxSimplificationInteractiveTest,
                       HasBackgroundApplied) {
  const DeepQuery kContextButton = {"omnibox-popup-app", "#context",
                                    "#entrypoint"};
  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      SetAimEligibleResponse(),
      SeedSearchboxResult("a"),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
      WaitForClassicPopupReady(),
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kContextButton)),
      InSameContext(CheckJsResultAt(
          kClassicPopupWebView, kContextButton,
          "el => window.getComputedStyle(el).backgroundColor !== 'transparent'",
          true)));
}

IN_PROC_BROWSER_TEST_F(WebUIOmniboxSimplificationInteractiveTest,
                       OblongShapeApplied) {
  const DeepQuery kContextButton = {"omnibox-popup-app", "#context",
                                    "#entrypoint"};
  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      SetAimEligibleResponse(),
      SeedSearchboxResult("a"),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
      WaitForClassicPopupReady(),
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kContextButton)),
      InSameContext(CheckJsResultAt(
          kClassicPopupWebView, kContextButton,
          "el => window.getComputedStyle(el).borderRadius", "100px")));
}

IN_PROC_BROWSER_TEST_F(WebUIOmniboxSimplificationInteractiveTest,
                       HasSuggestionLabel) {
  const DeepQuery kSuggestionLabel = {"omnibox-popup-app", "#context",
                                      "#description"};
  std::u16string expected_text =
      l10n_util::GetStringUTF16(IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL);
  RunTestSequence(
      AddInstrumentedTab(kNewTab, chrome::ChromeUINewTabURLAsGURL()),
      SetAimEligibleResponse(), SeedSearchboxResult("a"),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"a"),
      WaitForClassicPopupReady(),
      InAnyContext(
          WaitForElementToRender(kClassicPopupWebView, kSuggestionLabel)),
      InSameContext(CheckJsResultAt(kClassicPopupWebView, kSuggestionLabel,
                                    "el => el.textContent.trim()",
                                    base::UTF16ToUTF8(expected_text))));
}
