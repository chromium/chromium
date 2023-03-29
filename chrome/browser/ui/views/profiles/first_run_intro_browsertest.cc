// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/functional/callback_helpers.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// Tests for the chrome://intro WebUI page. They live here and not in the webui
// directory because they manipulate views.
namespace {
struct FirstRunTestParam {
  PixelTestParam pixel_test_param;
  bool use_fixed_size = false;
  bool use_longer_strings = false;
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
                          .use_dark_theme = true,
                          .use_fre_style = true},
     .use_fixed_size = true},
    {.pixel_test_param = {.test_suffix = "LightTheme", .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix = "LongerStringsFixedSize",
                          .use_fre_style = true},
     .use_fixed_size = true,
     .use_longer_strings = true},
    {.pixel_test_param = {.test_suffix = "RightToLeftLanguage",
                          .use_right_to_left_language = true,
                          .use_fre_style = true}},
    {.pixel_test_param = {.test_suffix = "CR2023",
                          .use_fre_style = true,
                          .use_chrome_refresh_2023_style = true}},
};

const char kMakeCardDescriptionLongerJsString[] =
    "(() => {"
    "  const introApp = document.querySelector('intro-app');"
    "  const signInPromo = introApp.shadowRoot.querySelector('sign-in-promo');"
    "  const cardDescriptions = signInPromo.shadowRoot.querySelectorAll("
    "      '.benefit-card-description');"
    "  cardDescriptions[0].textContent = "
    "      cardDescriptions[0].textContent.repeat(20);"
    "  return true;"
    "})();";
}  // namespace

class FirstRunIntroPixelTest
    : public UiBrowserTest,
      public testing::WithParamInterface<FirstRunTestParam> {
 public:
  FirstRunIntroPixelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    InitPixelTestFeatures(GetParam().pixel_test_param, scoped_feature_list_,
                          enabled_features, disabled_features);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SetUpPixelTestCommandLine(GetParam().pixel_test_param, scoped_env_override_,
                              command_line);
  }

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    profile_picker_view_ = new ProfileManagementStepTestView(
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kIntro,
        /*step_controller_factory=*/
        base::BindRepeating([](ProfilePickerWebContentsHost* host) {
          return CreateIntroStep(host, base::DoNothing(),
                                 /*enable_animations=*/false);
        }));
    profile_picker_view_->ShowAndWait(
        GetParam().use_fixed_size
            ? absl::optional<gfx::Size>(gfx::Size(840, 630))
            : absl::nullopt);

    if (GetParam().use_longer_strings) {
      EXPECT_EQ(true, content::EvalJs(profile_picker_view_->GetPickerContents(),
                                      kMakeCardDescriptionLongerJsString));
    }
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "FirstRunIntroPixelTest", screenshot_name);
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ViewDeletedWaiter(profile_picker_view_).Wait();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return profile_picker_view_->GetWidget();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;

  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
};

IN_PROC_BROWSER_TEST_P(FirstRunIntroPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunIntroPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
