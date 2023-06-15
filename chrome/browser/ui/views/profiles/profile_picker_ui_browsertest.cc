// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://profile-picker/ WebUI page. They live here
// and not in the webui directory because they manipulate views.
namespace {
struct ProfilePickerTestParam {
  PixelTestParam pixel_test_param;
  bool use_multiple_profiles = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfilePickerTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const ProfilePickerTestParam kTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "MultipleProfiles"},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .use_small_window = true},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "CR2023",
                          .use_chrome_refresh_2023_style = true}},
};

void AddMultipleProfiles(Profile* profile, size_t number_of_profiles) {
  DCHECK(profile);

  for (size_t i = 0; i < number_of_profiles; i++) {
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"Joe", /*icon_index=*/i, /*is_hidden=*/false,
        base::IgnoreArgs<Profile*>(run_loop.QuitClosure()));
    run_loop.Run();
  }
}
}  // namespace

class ProfilePickerUIPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<ProfilePickerTestParam> {
 public:
  ProfilePickerUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {}

  void ShowUi(const std::string& name) override {
    DCHECK(browser());
    if (GetParam().use_multiple_profiles) {
      AddMultipleProfiles(browser()->profile(), /*number_of_profiles=*/4);
    }
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    const GURL profile_picker_main_view_url =
        GURL(chrome::kChromeUIProfilePickerUrl);
    content::TestNavigationObserver observer(profile_picker_main_view_url);
    observer.StartWatchingNewWebContents();

    profile_picker_view_ = new ProfileManagementStepTestView(
        // We use `ProfilePicker::Params::ForFirstRun` here because it is the
        // only constructor that lets us force a profile to use.
        ProfilePicker::Params::ForFirstRun(browser()->profile()->GetPath(),
                                           base::DoNothing()),
        ProfileManagementFlowController::Step::kProfilePicker,
        /*step_controller_factory=*/
        base::BindLambdaForTesting(
            [profile_picker_main_view_url](ProfilePickerWebContentsHost* host) {
              return ProfileManagementStepController::CreateForProfilePickerApp(
                  host, profile_picker_main_view_url);
            }));
    profile_picker_view_->ShowAndWait(
        GetParam().pixel_test_param.use_small_window
            ? absl::optional<gfx::Size>(gfx::Size(750, 590))
            : absl::nullopt);
    observer.Wait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "ProfilePickerUIPixelTest", screenshot_name) !=
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

IN_PROC_BROWSER_TEST_P(ProfilePickerUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfilePickerUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
