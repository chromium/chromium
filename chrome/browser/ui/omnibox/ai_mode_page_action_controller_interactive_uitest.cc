// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/interactive_browser_window_test.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test.h"

namespace omnibox {

namespace {

constexpr char kTestPageUrl[] = "https://foo.bar";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

ui::ElementIdentifier GetTargetElementId() {
  return features::IsWebUILocationBarEnabled() ? kBrowserViewElementId
                                               : kOmniboxElementId;
}

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
          AimEligibilityService::Configuration{});

  return std::move(mock_aim_eligibility_service);
}

}  // namespace

class AiModePageActionControllerInteractiveUiTest
    : public PageActionInteractiveTestMixin<InteractiveBrowserTest> {
 protected:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InitializeFeatures();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InteractiveBrowserTest::SetUpBrowserContextKeyedServices(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(BuildMockAimServiceEligibilityServiceInstance));
  }

  virtual void InitializeFeatures() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features*/ {{omnibox::internal::kWebUIOmniboxPopup, {}},
                              {omnibox::internal::kWebUIOmniboxAimPopup, {}}},
        /*disabled_features*/ {kHideAimEntrypointOnUserInput,
                               kHideAimEntrypointForUrlSuggestions});
  }

  InteractiveTestApi::MultiStep OpenTabWithPageUrlAndFocusOmnibox(
      bool is_ntp = false) {
    const GURL url =
        is_ntp ? chrome::ChromeUINewTabPageURLAsGURL() : GURL(kTestPageUrl);

    return Steps(
        InteractiveBrowserWindowTestApi::InstrumentTab(kTabId),
        InteractiveBrowserWindowTestApi::NavigateWebContents(kTabId, url),
        InteractiveBrowserWindowTestApi::WaitForWebContentsReady(kTabId),
        ui::test::InteractiveTestApi::FocusElement(kOmniboxElementId));
  }

  ui::InteractionSequence::StepBuilder OpenOmniboxPopupByTypingASingleZero() {
    return ui::test::InteractiveTestApi::SendKeyPress(GetTargetElementId(),
                                                      ui::VKEY_0);
  }

  ui::InteractionSequence::StepBuilder ClosePopupOrBlurOmnibox() {
    return ui::test::InteractiveTestApi::SendKeyPress(GetTargetElementId(),
                                                      ui::VKEY_ESCAPE);
  }

  InteractiveTestApi::MultiStep CheckChipVisible(bool visible) {
    BrowserWindowInterface* bwi = browser();
    if (visible) {
      return ui::test::InteractiveTestApi::Steps(
          PageActionInteractiveTestMixin::WaitForPageActionChipVisible(
              kActionAiMode),
          Do([bwi]() {
            EXPECT_TRUE(AiModePageActionController::From(bwi)->IsVisible());
          }));
    }
    return ui::test::InteractiveTestApi::Steps(
        PageActionInteractiveTestMixin::WaitForPageActionChipNotVisible(
            kActionAiMode),
        Do([bwi]() {
          EXPECT_FALSE(AiModePageActionController::From(bwi)->IsVisible());
        }));
  }

  ui::InteractionSequence::StepBuilder WaitForAimPopup() {
    return ui::test::InteractiveTestApi::WaitForShow(
               OmniboxPopupPresenterBase::kRoundedResultsFrame, true)
        .SetContext(ui::InteractionSequence::ContextMode::kAny);
  }

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       NtpOmniboxFocusedButPopupClosed) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(/*visible=*/true));
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       NonNtpOmniboxFocusedButPopupClosed) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/false),
                  CheckChipVisible(/*visible=*/false));
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       HiddenWhenOmniboxBlurred) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(),
                  ClosePopupOrBlurOmnibox(),
                  CheckChipVisible(/*visible=*/false));
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       VisibleWithOmniboxPopupOpen) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(),
                  OpenOmniboxPopupByTypingASingleZero(),
                  CheckChipVisible(/*visible=*/true));
}

// TODO(crbug.com/547718513): Re-enable
#if BUILDFLAG(IS_MAC)
#define MAYBE_PressingChipWithMouseOpensAiMode \
  DISABLED_PressingChipWithMouseOpensAiMode
#else
#define MAYBE_PressingChipWithMouseOpensAiMode \
  PressingChipWithMouseOpensAiMode
#endif
IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       MAYBE_PressingChipWithMouseOpensAiMode) {
  base::HistogramTester histogram_tester;
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(/*visible=*/true),
                  InvokePageAction(kActionAiMode, InputType::kMouse),
                  WaitForAimPopup());

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEntrypoint.Activated.ViaKeyboard", false, 1);
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       PressingChipWithKeyboardOpensAiMode) {
  base::HistogramTester histogram_tester;
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(/*visible=*/true),
                  InvokePageAction(kActionAiMode, InputType::kKeyboard),
                  WaitForAimPopup());

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEntrypoint.Activated.ViaKeyboard", true, 1);
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       TogglesVisibilityWithPreferenceChange) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(/*visible=*/true),

                  Do(base::BindLambdaForTesting([&]() {
                    chrome::ToggleShowAiModeOmniboxButton(browser());
                  })),
                  CheckChipVisible(/*visible=*/false));
}

class AiModePageActionControllerHideEntryPointOnEditInteractiveUiTest
    : public AiModePageActionControllerInteractiveUiTest {
 protected:
  void InitializeFeatures() override {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features*/ {{kHideAimEntrypointOnUserInput, {}},
                              {omnibox::internal::kWebUIOmniboxPopup, {}},
                              {omnibox::internal::kWebUIOmniboxAimPopup, {}}},
        /*disabled_features*/ {});
  }
};

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerHideEntryPointOnEditInteractiveUiTest,
    HiddenWhileEditingOmnibox) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(),
                  OpenOmniboxPopupByTypingASingleZero(),
                  CheckChipVisible(/*visible=*/false));
}

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerHideEntryPointOnEditInteractiveUiTest,
    VisibleWhileNotEditingOmnibox) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(),
                  OpenOmniboxPopupByTypingASingleZero(),
                  SendKeyPress(GetTargetElementId(), ui::VKEY_BACK),
                  CheckChipVisible(/*visible=*/true));
}

class AiModePageActionControllerHideEntryPointForUrlInteractiveUiTest
    : public AiModePageActionControllerInteractiveUiTest {
 protected:
  void InitializeFeatures() override {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features*/ {{kHideAimEntrypointForUrlSuggestions, {}},
                              {omnibox::internal::kWebUIOmniboxPopup, {}},
                              {omnibox::internal::kWebUIOmniboxAimPopup, {}}},
        /*disabled_features*/ {});
  }
};

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerHideEntryPointForUrlInteractiveUiTest,
    HidesOnUrlSuggestion) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(true),
                  // Type a URL.
                  EnterText(GetTargetElementId(), u"https://google.com"),
                  CheckChipVisible(false));
}

class AiModePageActionControllerDynamicAiModeButtonInteractiveUiTest
    : public AiModePageActionControllerInteractiveUiTest {
 protected:
  void InitializeFeatures() override {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features*/ {{kWebUIOmniboxDynamicAiModeButton,
                               {{"Omnibox_DynamicAnimation", "true"},
                                {"Omnibox_DynamicColorScheme", "true"}}},
                              {omnibox::internal::kWebUIOmniboxPopup, {}},
                              {omnibox::internal::kWebUIOmniboxAimPopup, {}}},
        /*disabled_features*/ {});
  }
};

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerDynamicAiModeButtonInteractiveUiTest,
    HidesOnUrlSuggestion) {
  RunTestSequence(OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
                  CheckChipVisible(true),
                  // Type a URL.
                  EnterText(GetTargetElementId(), u"https://google.com"),
                  CheckChipVisible(false));
}

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerDynamicAiModeButtonInteractiveUiTest,
    VerifyBackgroundColorOverride) {
  RunTestSequence(
      OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
      CheckChipVisible(true), Do([this]() {
        if (features::IsWebUILocationBarEnabled()) {
          // TODO(crbug.com/545160323): Support background color test in WebUI
          // location bar.
          return;
        }
        auto* provider = BrowserView::GetBrowserViewForBrowser(browser())
                             ->toolbar_button_provider();
        auto* view = static_cast<page_actions::PageActionView*>(
            page_actions::GetIconLabelBubbleViewForTesting(
                provider->GetPageActionViewInterface(kActionAiMode),
                kActionAiMode));
        ASSERT_NE(view, nullptr);
        SkColor actual_bg_color = view->GetBackgroundColorForTesting();
        SkColor expected_bg_color = view->GetColorProvider()->GetColor(
            kColorOmniboxResultsBackgroundHovered);
        EXPECT_EQ(actual_bg_color, expected_bg_color);
      }));
}

IN_PROC_BROWSER_TEST_F(
    AiModePageActionControllerDynamicAiModeButtonInteractiveUiTest,
    ShowsLeadingIconWhenNoUserInputInProgress) {
  RunTestSequence(
      OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
      CheckChipVisible(true),
      Do([this]() {
        if (features::IsWebUILocationBarEnabled()) {
          return;
        }
        auto* provider = BrowserView::GetBrowserViewForBrowser(browser())
                             ->toolbar_button_provider();
        auto* view = static_cast<page_actions::PageActionView*>(
            page_actions::GetIconLabelBubbleViewForTesting(
                provider->GetPageActionViewInterface(kActionAiMode),
                kActionAiMode));
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(view->slide_animation_for_testing().GetCurrentValue(), 0.0);
      }));
}

}  // namespace omnibox

