// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://profile-picker/new-profile WebUI page. They live here
// and not in the webui directory because they manipulate views.
namespace {

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<PixelTestParam>& info) {
  return info.param.test_suffix;
}

// Permutations of supported parameters.
const PixelTestParam kTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkRtlSmall",
     .use_dark_theme = true,
     .use_right_to_left_language = true,
     .use_small_window = true},
};

const char kRemoveAvatarIconJS[] =
    "(() => {"
    "  const profilePickerApp = document.querySelector('profile-picker-app');"
    "  const profileTypeChoice = "
    "profilePickerApp.shadowRoot.querySelector('profile-type-choice');"
    "  const image = profileTypeChoice.shadowRoot.querySelector('.avatar');"
    "  image.src = '';"
    "  return true;"
    "})();";
}  // namespace

class ProfileTypeChoiceUIPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  ProfileTypeChoiceUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {}

  void ShowUi(const std::string& name) override {
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);

    DCHECK(browser());
    const GURL profile_type_choice_url =
        GURL("chrome://profile-picker/new-profile");
    content::TestNavigationObserver observer(profile_type_choice_url);
    observer.StartWatchingNewWebContents();

    profile_picker_view_ = new ProfileManagementStepTestView(
        // We use `ProfilePicker::Params::ForFirstRun` here because it is the
        // only constructor that lets us force a profile to use.
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kProfilePicker,
        /*step_controller_factory=*/
        base::BindLambdaForTesting(
            [profile_type_choice_url](ProfilePickerWebContentsHost* host) {
              return ProfileManagementStepController::CreateForProfilePickerApp(
                  host, profile_type_choice_url);
            }));
    profile_picker_view_->ShowAndWait(
        GetParam().use_small_window
            ? std::optional<gfx::Size>(gfx::Size(750, 590))
            : std::nullopt);
    observer.Wait();

    // We need to remove the avatar icon because it will be generated
    // randomly with a different color every time we run the test. Not removing
    // it will cause the test to be flaky.
    EXPECT_EQ(true, content::EvalJs(profile_picker_view_->GetPickerContents(),
                                    kRemoveAvatarIconJS));
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "ProfileTypeChoiceUIPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
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

IN_PROC_BROWSER_TEST_P(ProfileTypeChoiceUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileTypeChoiceUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
