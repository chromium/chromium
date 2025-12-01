// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

// Tests for the chrome://profile-picker/ WebUI page. They live here
// and not in the webui directory because they manipulate views.
namespace {
struct ProfilePickerTestParam {
  PixelTestParam pixel_test_param;
  bool use_multiple_profiles = false;
  // Requires `use_multiple_profiles` to be enabled.
  bool has_supervised_user = false;
  bool disallow_profile_creation = false;
  bool use_glic_version = false;
  bool no_glic_eligible_profiles = false;
  bool is_enterprise_badging_enabled = false;
  bool is_profile_picker_first_run = true;
  std::string text_variation_feature_param;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like
// ProfilePickerUIPixelTest.InvokeUi_default/<TestSuffix>`
// instead of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfilePickerTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const ProfilePickerTestParam kTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "RegularSecondPickerRun"},
     .is_profile_picker_first_run = false},
    {
        .pixel_test_param = {.test_suffix = "MultipleProfiles"},
        .use_multiple_profiles = true,
    },
    {.pixel_test_param = {.test_suffix = "PortraitModeWindow",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize}},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_multiple_profiles = true},
    {.pixel_test_param = {.test_suffix = "MultipleProfilesNoProfileCreation"},
     .use_multiple_profiles = true,
     .disallow_profile_creation = true},
    {
        .pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles",
                             .use_dark_theme = true,
                             .use_right_to_left_language = true,
                             .window_size = PixelTestParam::kSmallWindowSize},
        .use_multiple_profiles = true,
    },
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    {.pixel_test_param = {.test_suffix = "MultipleProfiles_Kite"},
     .use_multiple_profiles = true,
     .has_supervised_user = true},
    {.pixel_test_param = {.test_suffix = "DarkRtlSmallMultipleProfiles_Kite",
                          .use_dark_theme = true,
                          .use_right_to_left_language = true,
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true,
     .has_supervised_user = true},
    {.pixel_test_param = {.test_suffix = "ManagedProfileHasCustomWorkLabel"},
     .use_multiple_profiles = true,
     .is_enterprise_badging_enabled = true},
#endif
    {.pixel_test_param = {.test_suffix = "GlicRegular"},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicRegularDarkMode",
                          .use_dark_theme = true},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicRegularSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicRegularPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicNoProfiles"},
     .use_glic_version = true,
     .no_glic_eligible_profiles = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfiles"},
     .use_multiple_profiles = true,
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfilesSmall",
                          .window_size = PixelTestParam::kSmallWindowSize},
     .use_multiple_profiles = true,
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "GlicMultipleProfilesPortraitMode",
                          .window_size =
                              PixelTestParam::kPortraitModeWindowSize},
     .use_multiple_profiles = true,
     .use_glic_version = true},
    {.pixel_test_param = {.test_suffix = "VariationKeepWorkAndLifeSeparate"},
     .text_variation_feature_param = "keep-work-and-life-separate"},
    {.pixel_test_param = {.test_suffix = "VariationGotAnotherGoogleAccount"},
     .text_variation_feature_param = "got-another-google-account"},
    {.pixel_test_param = {.test_suffix = "VariationKeepTasksSeparate"},
     .text_variation_feature_param = "keep-tasks-separate"},
    {.pixel_test_param = {.test_suffix = "VariationSharingAComputer"},
     .text_variation_feature_param = "sharing-a-computer"},
    {.pixel_test_param = {.test_suffix = "VariationKeepEverythingInChrome"},
     .text_variation_feature_param = "keep-everything-in-chrome"},
};

enum class ProfileStatus {
  kSignedOut,
  kSignedIn,
  kSignedInManaged,
  kSignedInSupervised,
};

void SetSigninProfileProperties(signin::IdentityManager* identity_manager,
                                ProfileStatus profile_status,
                                bool is_glic_version,
                                const base::FilePath& profile_path) {
  CHECK(identity_manager);

  AccountInfo account_info;
  switch (profile_status) {
    case ProfileStatus::kSignedOut:
      break;
    case ProfileStatus::kSignedIn:
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joe@gmail.com", signin::ConsentLevel::kSignin);
      break;
    case ProfileStatus::kSignedInManaged: {
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joework@example.com",
          signin::ConsentLevel::kSignin);
      account_info =
          FillAccountInfo(account_info, AccountManagementStatus::kManaged,
                          signin::Tribool::kUnknown);
      signin::UpdateAccountInfoForAccount(identity_manager, account_info);
      break;
    }
    case ProfileStatus::kSignedInSupervised: {
      account_info = signin::MakePrimaryAccountAvailable(
          identity_manager, "joejunior@gmail.com",
          signin::ConsentLevel::kSignin);
      supervised_user::UpdateSupervisionStatusForAccount(
          account_info, identity_manager, true);
      break;
    }
  }

  if (!account_info.IsEmpty() && is_glic_version) {
    // Override the value in the entry bypassing the real logic; this way the
    // test does not depend on the real implementation of the Glic-Eligibility.
    // The entry value is not expected to be updated after this call, the
    // eligibilty may be reset.
    g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile_path)
        ->SetIsGlicEligible(true);
  }
}

// Create 4 profiles with different icons and types.
void AddMultipleProfiles(bool is_glic_version, bool has_supervised_user) {
  std::vector<ProfileStatus> profiles_status;
  if (is_glic_version) {
    // For the glic version, we need all Profiles to be signed in.
    profiles_status.insert(
        profiles_status.end(),
        {ProfileStatus::kSignedIn, ProfileStatus::kSignedInManaged,
         ProfileStatus::kSignedIn, ProfileStatus::kSignedInManaged});
  } else {
    profiles_status.insert(profiles_status.end(),
                           {ProfileStatus::kSignedOut, ProfileStatus::kSignedIn,
                            ProfileStatus::kSignedInManaged});
    if (has_supervised_user) {
      profiles_status.push_back(ProfileStatus::kSignedInSupervised);
    }
  }

  size_t icon_index = 0;
  for (ProfileStatus profile_status : profiles_status) {
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"Joe", icon_index++, /*is_hidden=*/false,
        /*initialized_callback=*/
        base::BindLambdaForTesting([&run_loop, &profile_status,
                                    &is_glic_version](Profile* profile) {
          SetSigninProfileProperties(
              IdentityManagerFactory::GetForProfile(profile), profile_status,
              is_glic_version, profile->GetPath());
          if (profile_status == ProfileStatus::kSignedInManaged) {
            enterprise_util::SetUserAcceptedAccountManagement(profile, true);
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}
}  // namespace

class ProfilePickerUIPixelTest
    : public WithProfilePickerTestHelpers,
      public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<ProfilePickerTestParam> {
 public:
  ProfilePickerUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {
    if (!GetParam().text_variation_feature_param.empty()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{switches::kProfilePickerTextVariations,
            {{"profile-picker-variation",
              GetParam().text_variation_feature_param}}}},
          {});
    }
  }

  void ShowUi(const std::string& name) override {
    DCHECK(browser());

    bool is_glic_version = GetParam().use_glic_version;
    bool no_glic_eligible_profiles = GetParam().no_glic_eligible_profiles;

    // In Glic mode, sign in the default account as well if we need eligible
    // profiles.
    if (is_glic_version && !no_glic_eligible_profiles) {
      SetSigninProfileProperties(
          IdentityManagerFactory::GetForProfile(browser()->profile()),
          ProfileStatus::kSignedIn,
          /*is_glic_version=*/true, browser()->profile()->GetPath());
    }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    if (GetParam().is_enterprise_badging_enabled) {
      policy::ScopedManagementServiceOverrideForTesting platform_management(
          policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
          policy::EnterpriseManagementAuthority::CLOUD);
      browser()->profile()->GetPrefs()->SetString(
          prefs::kEnterpriseCustomLabelForProfile, "Work");
    }
#endif

    if (GetParam().use_multiple_profiles) {
      // In Glic mode, if `use_multiple_profiles` is set,
      // `no_glic_eligible_profiles` must be set to false.
      CHECK(!is_glic_version || !no_glic_eligible_profiles);
      AddMultipleProfiles(is_glic_version, GetParam().has_supervised_user);
    }

    if (GetParam().disallow_profile_creation) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kBrowserAddPersonEnabled, false);
    }

    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    GURL profile_picker_main_view_url = GURL(chrome::kChromeUIProfilePickerUrl);
    // Since we override the FlowController, we need to give in the full Url
    // with the glic query param from the start.
    if (is_glic_version) {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerGlicQuery);
      profile_picker_main_view_url =
          profile_picker_main_view_url.ReplaceComponents(replacements);
    }

    content::TestNavigationObserver observer(profile_picker_main_view_url);
    observer.StartWatchingNewWebContents();

    ProfilePicker::Params params =
        is_glic_version
            ? ProfilePicker::Params::ForGlicManager(base::DoNothing())
            // We use `ProfilePicker::Params::ForFirstRun` here because it is
            // the only constructor that lets us force a profile to use.
            : ProfilePicker::Params::ForFirstRun(
                  browser()->profile()->GetPath(), base::DoNothing());

    if (!GetParam().is_profile_picker_first_run) {
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
      WaitForLoadStop(GURL("chrome://profile-picker"));
      CHECK(ProfilePicker::IsOpen());
      ProfilePicker::Hide();
      WaitForPickerClosed();
    }

    profile_picker_view_ = new ProfileManagementStepTestView(
        std::move(params),
        ProfileManagementFlowController::Step::kProfilePicker,
        /*step_controller_factory=*/
        base::BindLambdaForTesting(
            [profile_picker_main_view_url](ProfilePickerWebContentsHost* host) {
              return ProfileManagementStepController::CreateForProfilePickerApp(
                  host, profile_picker_main_view_url);
            }));
    profile_picker_view_->ShowAndWait(GetParam().pixel_test_param.window_size);
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

  raw_ptr<ProfileManagementStepTestView, DanglingUntriaged>
      profile_picker_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ProfilePickerUIPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfilePickerUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
