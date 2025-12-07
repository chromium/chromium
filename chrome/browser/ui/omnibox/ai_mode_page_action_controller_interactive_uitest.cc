// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox {

namespace {

constexpr char kTestPageUrl[] = "https://foo.bar";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

  EXPECT_CALL(*mock_aim_eligibility_service, IsServerEligibilityEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_aim_eligibility_service, IsAimEligible())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_aim_eligibility_service, IsAimLocallyEligible())
      .WillRepeatedly(testing::Return(true));

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
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kAiModeOmniboxEntryPoint, {}},
        {
            features::kPageActionsMigration,
            {
                {
                    features::kPageActionsMigrationAiMode.name,
                    "true",
                },
            },
        }};

    std::vector<base::test::FeatureRef> disabled_features = {
        kHideAimEntrypointOnUserInput};

    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

  using PageActionInteractiveTestMixin::WaitForPageActionChipVisible;

  auto WaitForPageActionChipVisible() {
    MultiStep steps;
    steps += WaitForPageActionChipVisible(kActionAiMode);
    return steps;
  }

  MultiStep OpenTabWithPageUrlAndFocusOmnibox(bool is_ntp = false) {
    const std::string url =
        is_ntp ? chrome::kChromeUINewTabPageURL : kTestPageUrl;

    return Steps(InstrumentTab(kTabId), NavigateWebContents(kTabId, GURL(url)),
                 WaitForWebContentsReady(kTabId),
                 FocusElement(kOmniboxElementId));
  }

  MultiStep OpenOmniboxPopupByTypingASingleZero() {
    return Steps(SendKeyPress(kOmniboxElementId, ui::VKEY_0));
  }

  MultiStep ClosePopupOrBlurOmnibox() {
    return Steps(SendKeyPress(kOmniboxElementId, ui::VKEY_ESCAPE));
  }

  MultiStep CheckChipVisible(bool visible) {
    return visible ? Steps(WaitForPageActionChipVisible())
                   : Steps(WaitForHide(kAiModePageActionIconElementId));
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

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       PressingChipWithMouseOpensAiMode) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
      CheckChipVisible(/*visible=*/true),
      PressButton(kAiModePageActionIconElementId, InputType::kMouse),
      WaitForWebContentsNavigation(kTabId));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEntrypoint.Activated.ViaKeyboard", false, 1);
}

IN_PROC_BROWSER_TEST_F(AiModePageActionControllerInteractiveUiTest,
                       PressingChipWithKeyboardOpensAiMode) {
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenTabWithPageUrlAndFocusOmnibox(/*is_ntp=*/true),
      CheckChipVisible(/*visible=*/true),
      PressButton(kAiModePageActionIconElementId, InputType::kKeyboard),
      WaitForWebContentsNavigation(kTabId));

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
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {kAiModeOmniboxEntryPoint, {}},
        {kHideAimEntrypointOnUserInput, {}},
        {
            features::kPageActionsMigration,
            {
                {
                    features::kPageActionsMigrationAiMode.name,
                    "true",
                },
            },
        }};

    features_.InitWithFeaturesAndParameters(enabled_features, {});
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
                  SendKeyPress(kOmniboxElementId, ui::VKEY_BACK),
                  CheckChipVisible(/*visible=*/true));
}

}  // namespace omnibox
