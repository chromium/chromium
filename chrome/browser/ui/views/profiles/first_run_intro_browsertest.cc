// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/first_run_flow_controller_dice.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
                          .use_dark_theme = true},
     .use_fixed_size = true},
#if !BUILDFLAG(IS_WIN)
    // TODO(https://crbug.com/40261456): The following test has been frequently
    // flaking on "Win10 Tests x64" since 2024-05-09:
    // FirstRunIntroPixelTest.InvokeUi_default/LightTheme
    {.pixel_test_param = {.test_suffix = "LightTheme"}},
#endif
    {.pixel_test_param = {.test_suffix = "LongerStringsFixedSize"},
     .use_fixed_size = true,
     .use_longer_strings = true},
    {.pixel_test_param = {.test_suffix = "RightToLeftLanguage",
                          .use_right_to_left_language = true}},
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
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<FirstRunTestParam> {
 public:
  FirstRunIntroPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {}

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);

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
            ? std::optional<gfx::Size>(gfx::Size(840, 630))
            : std::nullopt);

    if (GetParam().use_longer_strings) {
      EXPECT_EQ(true, content::EvalJs(profile_picker_view_->GetPickerContents(),
                                      kMakeCardDescriptionLongerJsString));
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
};

IN_PROC_BROWSER_TEST_P(FirstRunIntroPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         FirstRunIntroPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
