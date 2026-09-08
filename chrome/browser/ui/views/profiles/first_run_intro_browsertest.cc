// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://intro WebUI page. They live here and not in the webui
// directory because they manipulate views.
namespace {
struct FirstRunTestParam {
  PixelTestParam pixel_test_param;
  bool use_fixed_size = false;
  bool use_longer_strings = false;
  bool decline_signin_cta_experiment_enabled = false;

  bool use_refresh = false;
  bool use_revamp = false;
  bool enable_sound = true;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<FirstRunTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const FirstRunTestParam kTestParams[] = {
    {.pixel_test_param = {.test_suffix = "DarkThemeFixedSize",
                          .use_dark_theme = true},
     .use_fixed_size = true},
    {.pixel_test_param = {.test_suffix = "DarkThemeDeclineSigninCTAExperiment",
                          .use_dark_theme = true},
     .decline_signin_cta_experiment_enabled = true},

#if !BUILDFLAG(IS_WIN)
    // TODO(https://crbug.com/40261456): The following test has been frequently
    // flaking on "Win10 Tests x64" since 2024-05-09:
    // FirstRunIntroPixelTest.InvokeUi_default/LightTheme
    {.pixel_test_param = {.test_suffix = "LightTheme"}},
#endif
    {.pixel_test_param = {.test_suffix = "LongerStringsFixedSize"},
     .use_fixed_size = true,
     .use_longer_strings = true,
     .use_refresh = false},
    {.pixel_test_param = {.test_suffix = "LongerStringsFixedSizeRefreshedUI"},
     .use_fixed_size = true,
     .use_longer_strings = true,
     .use_refresh = true},
    {.pixel_test_param = {.test_suffix = "RightToLeftLanguage",
                          .use_right_to_left_language = true}},
    // Refresh parameters
    {.pixel_test_param = {.test_suffix = "RefreshDefault"},
     .use_refresh = true},
    {.pixel_test_param = {.test_suffix = "RefreshDarkTheme",
                          .use_dark_theme = true},
     .use_refresh = true},
    {.pixel_test_param = {.test_suffix = "RefreshRightToLeftLanguage",
                          .use_right_to_left_language = true},
     .use_refresh = true},

    // Revamp parameters.
    {.pixel_test_param = {.test_suffix = "RevampDefault"},
     .use_refresh = true,
     .use_revamp = true},
    {.pixel_test_param = {.test_suffix = "RevampRightToLeftLanguage",
                          .use_right_to_left_language = true},
     .use_refresh = true,
     .use_revamp = true},
    {.pixel_test_param = {.test_suffix = "RevampSoundDisabled"},
     .use_refresh = true,
     .use_revamp = true,
     .enable_sound = false},
};

std::string_view GetMakeCardDescriptionLongerJsString() {
  if (base::FeatureList::IsEnabled(switches::kFirstRunDesktopRefresh)) {
    return "(() => {"
           "  const signInPromo = "
           "  document.querySelector('sign-in-promo-refresh');"
           "  const cardDescriptions = signInPromo.shadowRoot.querySelectorAll("
           "      '.benefit-card-description');"
           "  cardDescriptions[0].textContent = "
           "      cardDescriptions[0].textContent.repeat(20);"
           "  return true;"
           "})();";
  }

  return "(() => {"
         "  const introApp = document.querySelector('intro-app');"
         "  const signInPromo = "
         "introApp.shadowRoot.querySelector('sign-in-promo');"
         "  const cardDescriptions = signInPromo.shadowRoot.querySelectorAll("
         "      '.benefit-card-description');"
         "  cardDescriptions[0].textContent = "
         "      cardDescriptions[0].textContent.repeat(20);"
         "  return true;"
         "})();";
}

}  // namespace

// TODO(crbug.com/542896534): Add tests for larger profile picker window.
class FirstRunIntroPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<FirstRunTestParam> {
 public:
  FirstRunIntroPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    scoped_feature_list_.InitWithFeatureStates(
        {{switches::kProfileCreationDeclineSigninCTAExperiment,
          GetParam().decline_signin_cta_experiment_enabled},

         {switches::kFirstRunDesktopRefresh, GetParam().use_refresh},
         {switches::kFirstRunDesktopRevamp, GetParam().use_revamp},
         {switches::kFirstRunDesktopRevampSound, GetParam().enable_sound},
         {switches::kDisableFirstRunAnimationsForTesting,
          GetParam().use_refresh}});
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);

    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->GetProfile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kIntro,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return CreateIntroStep(
              host, /*choice_callback=*/base::DoNothing(),
              /*enable_animations=*/false,
              /*query_effects_callback=*/base::BindRepeating([] {
                return false;
              }),
              /*effects_button_shown_by_default=*/GetParam().use_revamp &&
                  GetParam().enable_sound);
        }));
    profile_picker_view_->ShowAndWait(
        GetParam().use_fixed_size
            ? std::optional<gfx::Size>(gfx::Size(840, 630))
            : std::nullopt);

    if (GetParam().use_longer_strings) {
      EXPECT_EQ(true, content::EvalJs(profile_picker_view_->GetPickerContents(),
                                      GetMakeCardDescriptionLongerJsString()));
    }
    if (GetParam().use_refresh) {
      // Explicitly wait for the animations to load to avoid flakiness.
      CHECK_EQ(
          content::EvalJs(profile_picker_view_->GetPickerContents(),
                          GetWaitForAnimationsScript("sign-in-promo-refresh")),
          true);
    }
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "FirstRunIntroPixelTest", screenshot_name) !=
           ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FirstRunIntroPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunIntroPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
