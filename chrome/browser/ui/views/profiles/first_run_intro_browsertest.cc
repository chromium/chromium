// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/functional/callback_helpers.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error Platform not supported
#endif

// Tests for the chrome://intro WebUI page. They live here and not in the webui
// directory because they manipulate views.
namespace {

struct TestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
  bool use_fixed_size = false;
  bool use_longer_strings = false;
  bool use_right_to_left_language = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `kTestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const TestParam kTestParams[] = {
    {.test_suffix = "DarkThemeFixedSize",
     .use_dark_theme = true,
     .use_fixed_size = true},
    {.test_suffix = "LightTheme"},
    {.test_suffix = "LongerStringsFixedSize",
     .use_fixed_size = true,
     .use_longer_strings = true},
    {.test_suffix = "RightToLeftLanguage", .use_right_to_left_language = true},
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

class FirstRunIntroPixelTest : public UiBrowserTest,
                               public testing::WithParamInterface<TestParam> {
 public:
  FirstRunIntroPixelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {kForYouFre};
    if (GetParam().use_dark_theme) {
      enabled_features.push_back(features::kWebUIDarkMode);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
    if (GetParam().use_right_to_left_language) {
      command_line->AppendSwitchASCII(switches::kLang, "ar");

      // On Linux & Lacros the command line switch has no effect, we need to use
      // environment variables to change the language.
      scoped_env_override_ =
          std::make_unique<base::ScopedEnvironmentVariableOverride>("LANGUAGE",
                                                                    "ar");
    }
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
