// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
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
  bool hide_guest_mode_for_supervised_users = false;
  bool show_kite_for_supervised_users = false;
  // param to be removed when `kOutlineSilhouetteIcon` is enabled by default.
  bool outline_silhouette_icon = false;
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
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_OutlineSilhouette"},
     .use_multiple_profiles = true,
     .outline_silhouette_icon = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .use_small_window = true},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix =
                              "DarkRtlSmallMultipleProfiles_OutlineSilhouette",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .use_small_window = true},
     .use_multiple_profiles = true,
     .outline_silhouette_icon = true},
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_HideGuest"},
     .use_multiple_profiles = true,
     .hide_guest_mode_for_supervised_users = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_Kite"},
     .use_multiple_profiles = true,
     .show_kite_for_supervised_users = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles_Kite",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .use_small_window = true},
     .use_multiple_profiles = true,
     .show_kite_for_supervised_users = true},
#endif
};

// Create 4 profiles with different icons and types.
void AddMultipleProfiles(Profile* profile) {
  DCHECK(profile);

  for (size_t i = 0; i < 4; i++) {
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"Joe", /*icon_index=*/i, /*is_hidden=*/false,
        /*initialized_callback=*/
        base::BindLambdaForTesting([&run_loop, &i](Profile* profile) {
          // Set properties for the profile.
          signin::IdentityManager* identity_manager =
              IdentityManagerFactory::GetForProfile(profile);
          CHECK(identity_manager);
          AccountInfo account_info;

          switch (i) {
            case 0:
              // A signed out profile.
              break;
            case 1:
              // A signed in regular profile.
              account_info = signin::MakePrimaryAccountAvailable(
                  identity_manager, "joe@gmail.com",
                  signin::ConsentLevel::kSignin);
              break;
            case 2:
              // A signed in Enterprise managed profile.
              account_info = signin::MakePrimaryAccountAvailable(
                  identity_manager, "joework@example.com",
                  signin::ConsentLevel::kSignin);
              account_info = FillAccountInfo(account_info,
                                             AccountManagementStatus::kManaged,
                                             signin::Tribool::kUnknown);
              signin::UpdateAccountInfoForAccount(identity_manager,
                                                  account_info);
              break;
            case 3:
              // A signed in supervised profile.
              account_info = signin::MakePrimaryAccountAvailable(
                  identity_manager, "joejunior@gmail.com",
                  signin::ConsentLevel::kSignin);
              supervised_user::UpdateSupervisionStatusForAccount(
                  account_info, identity_manager, true);
              break;
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}
}  // namespace

class ProfilePickerUIPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<ProfilePickerTestParam> {
 public:
  ProfilePickerUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureStates(
        {{supervised_user::kHideGuestModeForSupervisedUsers,
          GetParam().hide_guest_mode_for_supervised_users},
         {supervised_user::kShowKiteForSupervisedUsers,
          GetParam().show_kite_for_supervised_users},
         {kOutlineSilhouetteIcon, GetParam().outline_silhouette_icon}});
#endif
  }

  void ShowUi(const std::string& name) override {
    DCHECK(browser());
    if (GetParam().use_multiple_profiles) {
      AddMultipleProfiles(browser()->profile());
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
            ? std::optional<gfx::Size>(gfx::Size(750, 590))
            : std::nullopt);
    observer.Wait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

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

  base::test::ScopedFeatureList scoped_feature_list_;
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
