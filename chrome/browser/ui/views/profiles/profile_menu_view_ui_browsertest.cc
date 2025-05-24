// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

enum class ProfileTypePixelTestParam {
  kRegular,
  kIncognito,
  kGuest,
};

enum class SigninStatusPixelTestParam {
  kSigninDisallowed,
  kSignedOut,
  kWebSignedIn,
  kSignedInNoSync,
  kSignInPendingNoSync,
  kSignedInWithSync,
  kSignedInSyncPaused,
  kSignedInSyncNotWorking
};

enum class ManagementStatus {
  kNonManaged,
  kAccountManaged,
  kBrowserManaged,
  kSupervisedUser
};

struct ProfileMenuViewPixelTestParam {
  PixelTestParam pixel_test_param;
  ProfileTypePixelTestParam profile_type_param =
      ProfileTypePixelTestParam::kRegular;
  SigninStatusPixelTestParam signin_status =
      SigninStatusPixelTestParam::kSignedOut;
  ManagementStatus management_status = ManagementStatus::kNonManaged;
  bool use_multiple_profiles = false;
  bool account_image_available = true;

  // Features and parameters that are enabled in addition to the features
  // enabled by default.
  std::vector<base::test::FeatureRefAndParams> extra_features_and_params;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfileMenuViewPixelTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const ProfileMenuViewPixelTestParam kPixelTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {
        .pixel_test_param = {.test_suffix = "SigninDisallowed"},
        .signin_status = SigninStatusPixelTestParam::kSigninDisallowed,
    },
    {
        .pixel_test_param = {.test_suffix = "DarkTheme",
                             .use_dark_theme = true},
    },
    {
        .pixel_test_param = {.test_suffix = "RTL",
                             .use_right_to_left_language = true},
    },
    {
        .pixel_test_param = {.test_suffix = "SignedOut_MultipleProfiles"},
        .use_multiple_profiles = true,
    },
    {
        .pixel_test_param = {.test_suffix =
                                 "SignedOut_MultipleProfiles_DarkTheme",
                             .use_dark_theme = true},
        .use_multiple_profiles = true,
    },
    {
        .pixel_test_param = {.test_suffix = "WebSignedIn"},
        .signin_status = SigninStatusPixelTestParam::kWebSignedIn,
    },
    {
        .pixel_test_param = {.test_suffix = "WebSignedIn_PlaceholderIcon"},
        .signin_status = SigninStatusPixelTestParam::kWebSignedIn,
        .account_image_available = false,
    },
    {
        .pixel_test_param = {.test_suffix =
                                 "WebSignedIn_PlaceholderIcon_DarkTheme",
                             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kWebSignedIn,
        .account_image_available = false,
    },
    {
        .pixel_test_param = {.test_suffix = "SignedIn_MultipleProfiles"},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .use_multiple_profiles = true,
    },
    {
        .pixel_test_param = {.test_suffix =
                                 "SignedIn_MultipleProfiles_DarkTheme",
                             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .use_multiple_profiles = true,
    },
    {
        .pixel_test_param = {.test_suffix = "SignedIn_Sync"},
        .signin_status = SigninStatusPixelTestParam::kSignedInWithSync,
    },
    {
        .pixel_test_param = {.test_suffix = "SignedIn_SyncPaused",
                             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignedInSyncPaused,
    },
    {
        .pixel_test_param = {.test_suffix = "SignInPending"},
        .signin_status = SigninStatusPixelTestParam::kSignInPendingNoSync,
    },
    {
        .pixel_test_param = {.test_suffix = "SignInPending_RTL",
                             .use_right_to_left_language = true},
        .signin_status = SigninStatusPixelTestParam::kSignInPendingNoSync,
    },
    {
        .pixel_test_param = {.test_suffix = "SignedIn_AccountManaged"},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .management_status = ManagementStatus::kAccountManaged,
    },
    {
        .pixel_test_param = {.test_suffix = "SignedIn_BrowserManaged",
                             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignedOut,
        .management_status = ManagementStatus::kBrowserManaged,
    },
    {
        .pixel_test_param = {.test_suffix =
                                 "SignedIn_BrowserSupervised_DarkTheme",
                             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignedInWithSync,
        .management_status = ManagementStatus::kSupervisedUser,
    },
    {
        .pixel_test_param =
            {.test_suffix = "SignInPending_Nosync_BrowserSupervised_DarkTheme",
             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignInPendingNoSync,
        .management_status = ManagementStatus::kSupervisedUser,
    },
    {
        .pixel_test_param =
            {
                .test_suffix = "SignedIn_BrowserSupervised",
                .use_dark_theme = false,
            },
        .signin_status = SigninStatusPixelTestParam::kSignedInWithSync,
        .management_status = ManagementStatus::kSupervisedUser,
    },
    {
        .pixel_test_param =
            {
                .test_suffix = "SignInPending_Nosync_BrowserSupervised",
                .use_dark_theme = false,
            },
        .signin_status = SigninStatusPixelTestParam::kSignInPendingNoSync,
        .management_status = ManagementStatus::kSupervisedUser,
    },
    {
        .pixel_test_param = {.test_suffix = "Guest"},
        .profile_type_param = ProfileTypePixelTestParam::kGuest,
        .extra_features_and_params =
            {{switches::kEnableImprovedGuestProfileMenu, {}}},
    },
    {
        .pixel_test_param = {.test_suffix = "Guest_Dark",
                             .use_dark_theme = true},
        .profile_type_param = ProfileTypePixelTestParam::kGuest,
        .extra_features_and_params =
            {{switches::kEnableImprovedGuestProfileMenu, {}}},
    },
    {
        .pixel_test_param = {.test_suffix = "Incognito"},
        .profile_type_param = ProfileTypePixelTestParam::kIncognito,
        .extra_features_and_params =
            {{switches::kEnableImprovedGuestProfileMenu, {}}},
    },
    {
        .pixel_test_param = {.test_suffix = "HistorySyncOptinExperiment"},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .extra_features_and_params =
            {{switches::kEnableHistorySyncOptinExpansionPill, {}}},
    },
    {
        .pixel_test_param = {.test_suffix =
                                 "HistorySyncOptinExperimentNewPromoVariant"},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .extra_features_and_params =
            {{switches::kEnableHistorySyncOptinExpansionPill,
              {{"history-sync-optin-expansion-pill-option",
                "browse-across-devices-new-profile-menu-promo-variant"}}}},
    },
    {
        .pixel_test_param =
            {.test_suffix =
                 "HistorySyncOptinExperimentNewPromoVariant_DarkTheme",
             .use_dark_theme = true},
        .signin_status = SigninStatusPixelTestParam::kSignedInNoSync,
        .extra_features_and_params =
            {{switches::kEnableHistorySyncOptinExpansionPill,
              {{"history-sync-optin-expansion-pill-option",
                "browse-across-devices-new-profile-menu-promo-variant"}}}},
    },
};

}  // namespace

class ProfileMenuViewPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<ProfileMenuViewPixelTestParam> {
 public:
  ProfileMenuViewPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam().pixel_test_param) {
    // Disabled by default but may be overridden by `extra_features_and_params`.
    base::flat_set<base::test::FeatureRef> disabled_features = {
        switches::kEnableImprovedGuestProfileMenu,
        switches::kEnableHistorySyncOptinExpansionPill};
    for (const auto& [feature, _] : GetParam().extra_features_and_params) {
      disabled_features.erase(feature.get());
    }

    std::vector<base::test::FeatureRefAndParams> enabled_features_and_params = {
        {features::kEnterpriseProfileBadgingForMenu, {}},
        {features::kEnterpriseProfileBadgingPolicies, {}}};
    std::move(GetParam().extra_features_and_params.begin(),
              GetParam().extra_features_and_params.end(),
              std::back_inserter(enabled_features_and_params));

    feature_list_.InitWithFeaturesAndParameters(
        std::move(enabled_features_and_params),
        std::vector<base::test::FeatureRef>(disabled_features.begin(),
                                            disabled_features.end()));

    // The Profile menu view seems not to be resizied properly on changes which
    // causes the view to go out of bounds. This should not happen and needs to
    // be investigated further. As a work around, to have a proper screenshot
    // tests, disable the check.
    // TODO(crbug.com/336224718): Make the view resize properly and remove this
    // line as it is not recommended to have per
    // `TestBrowserDialog::should_verify_dialog_bounds_` definition and default
    // value.
    set_should_verify_dialog_bounds(false);
  }

  ~ProfileMenuViewPixelTest() override = default;

  void TearDownOnMainThread() override {
    scoped_browser_management_.reset();
    ProfilesPixelTestBaseT<DialogBrowserTest>::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfilesPixelTestBaseT<DialogBrowserTest>::SetUpCommandLine(command_line);
    if (GetSigninStatus() == SigninStatusPixelTestParam::kSigninDisallowed) {
      command_line->AppendSwitchASCII("allow-browser-signin", "false");
    }
  }

  ProfileTypePixelTestParam GetProfileType() const {
    return GetParam().profile_type_param;
  }

  SigninStatusPixelTestParam GetSigninStatus() const {
    return GetParam().signin_status;
  }

  ManagementStatus GetManagementStatus() const {
    return GetParam().management_status;
  }

  AccountManagementStatus GetAccountManagementStatus() const {
    const bool account_managed =
        GetManagementStatus() == ManagementStatus::kAccountManaged;
    return account_managed ? AccountManagementStatus::kManaged
                           : AccountManagementStatus::kNonManaged;
  }

  bool ShouldUseMultipleProfiles() const {
    return GetParam().use_multiple_profiles;
  }

  void SetColorTheme(Profile& profile,
                     SkColor color = SK_ColorTRANSPARENT,
                     bool dark_mode = false) {
    ThemeService* service = ThemeServiceFactory::GetForProfile(&profile);
    service->UseDeviceTheme(false);

    if (color != SK_ColorTRANSPARENT) {
      service->SetUserColorAndBrowserColorVariant(
          color, ui::mojom::BrowserColorVariant::kTonalSpot);
    }
    if (dark_mode) {
      service->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);
    } else {
      service->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
    }

    // Open browser to make changes effective.
    Browser* tmp_browser = CreateBrowser(&profile);
    CloseBrowserAsynchronously(tmp_browser);
  }

  void SetUpOnMainThread() override {
    ProfilesPixelTestBaseT<DialogBrowserTest>::SetUpOnMainThread();

    // Configures the browser according to the profile type.
    ui_test_utils::BrowserChangeObserver browser_added_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    Browser* new_browser = nullptr;

    switch (GetProfileType()) {
      case ProfileTypePixelTestParam::kRegular:
        // Nothing to do.
        break;
      case ProfileTypePixelTestParam::kIncognito:
        CreateIncognitoBrowser();
        new_browser = browser_added_observer.Wait();
        ASSERT_TRUE(new_browser);
        ASSERT_TRUE(new_browser->profile()->IsIncognitoProfile());
        break;
      case ProfileTypePixelTestParam::kGuest:
        CreateGuestBrowser();
        new_browser = browser_added_observer.Wait();
        ASSERT_TRUE(new_browser);
        ASSERT_TRUE(new_browser->profile()->IsGuestSession());
        break;
    }

    // Close the initial browser and set the new one as default.
    if (new_browser) {
      ASSERT_NE(new_browser, browser());
      CloseBrowserSynchronously(browser());
      SelectFirstBrowser();
      ASSERT_EQ(new_browser, browser());
    }

    AccountInfo account_info;
    // Configures browser according to desired signin status.
    switch (GetSigninStatus()) {
      case SigninStatusPixelTestParam::kSignedOut:
      case SigninStatusPixelTestParam::kSigninDisallowed:
        // Nothing to do.
        break;
      case SigninStatusPixelTestParam::kWebSignedIn:
        // Account management is not applied with `kWebSignedIn`.
        account_info = SignInWithAccount(AccountManagementStatus::kNonManaged,
                                         std::nullopt);
        break;
      case SigninStatusPixelTestParam::kSignedInNoSync:
        account_info = SignInWithAccount();
        break;
      case SigninStatusPixelTestParam::kSignInPendingNoSync:
        account_info = SignInWithAccount();
        identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
        break;
      case SigninStatusPixelTestParam::kSignedInWithSync: {
        account_info = SignInWithAccount(GetAccountManagementStatus(),
                                         signin::ConsentLevel::kSync);
        // Enable sync.
        syncer::SyncService* sync_service =
            SyncServiceFactory::GetForProfile(GetProfile());
        sync_service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

        break;
      }

      case SigninStatusPixelTestParam::kSignedInSyncPaused: {
        account_info = SignInWithAccount(GetAccountManagementStatus(),
                                         signin::ConsentLevel::kSync);

        // Enable sync.
        syncer::SyncService* sync_paused_service =
            SyncServiceFactory::GetForProfile(GetProfile());
        sync_paused_service->GetUserSettings()
            ->SetInitialSyncFeatureSetupComplete(
                syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

        identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
        break;
      }
      case SigninStatusPixelTestParam::kSignedInSyncNotWorking:
        account_info = SignInWithAccount(GetAccountManagementStatus(),
                                         signin::ConsentLevel::kSync);
        break;
    }

    switch (GetManagementStatus()) {
      case ManagementStatus::kNonManaged:
        break;
      case ManagementStatus::kAccountManaged:
        enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
        scoped_browser_management_ =
            std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
                policy::ManagementServiceFactory::GetForProfile(GetProfile()),
                policy::EnterpriseManagementAuthority::CLOUD);
        break;
      case ManagementStatus::kBrowserManaged:
        scoped_browser_management_ =
            std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
                policy::ManagementServiceFactory::GetForProfile(GetProfile()),
                policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
        break;
      case ManagementStatus::kSupervisedUser:
        if (!account_info.IsEmpty()) {
          supervised_user::UpdateSupervisionStatusForAccount(
              account_info, identity_test_env()->identity_manager(), true);
          break;
        }
    }

    if (ShouldUseMultipleProfiles()) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();

      // Default theme, light mode.
      profiles::testing::CreateProfileSync(
          profile_manager, profile_manager->GenerateNextProfileDirectoryPath());

      // Default theme, dark mode.
      Profile& dark_profile = profiles::testing::CreateProfileSync(
          profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
      SetColorTheme(dark_profile, SK_ColorTRANSPARENT, /*dark_mode=*/true);

      // Set theme, light mode.
      Profile& theme_profile = profiles::testing::CreateProfileSync(
          profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
      SetColorTheme(theme_profile, SK_ColorMAGENTA);

      // Set theme, dark mode.
      Profile& theme_dark_profile = profiles::testing::CreateProfileSync(
          profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
      SetColorTheme(theme_dark_profile, SK_ColorGREEN, /*dark_mode=*/true);
    }

    if (!GetParam().account_image_available) {
      // Remove account images. `SignInWithAccount()` adds an image by default.
      signin::IdentityManager* identity_manager =
          identity_test_env()->identity_manager();
      for (const CoreAccountInfo& info :
           identity_manager->GetAccountsWithRefreshTokens()) {
        SimulateAccountImageFetch(identity_manager, info.account_id,
                                  /*image_url_with_size=*/"NO_IMAGE",
                                  gfx::Image());
      }
    }
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CHECK(browser());

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "ProfileMenuViewBase");

    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

    widget_waiter.WaitIfNeededAndGet();
  }

 private:
  void OpenProfileMenu() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    OpenProfileMenuFromToolbar(browser_view->toolbar_button_provider());
  }

  void OpenProfileMenuFromToolbar(ToolbarButtonProvider* toolbar) {
    // Click the avatar button to open the menu.
    views::View* avatar_button = toolbar->GetAvatarToolbarButton();
    ASSERT_TRUE(avatar_button);
    Click(avatar_button);

    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_close_on_deactivate(false);

#if BUILDFLAG(IS_MAC)
    base::RunLoop().RunUntilIdle();
#else
    // If possible wait until the menu is active.
    views::Widget* menu_widget = profile_menu_view()->GetWidget();
    ASSERT_TRUE(menu_widget);
    if (menu_widget->CanActivate()) {
      views::test::WaitForWidgetActive(menu_widget, /*active=*/true);
    } else {
      LOG(ERROR) << "menu_widget can not be activated";
    }
#endif

    LOG(INFO) << "Opening profile menu was successful";
  }

  void Click(views::View* clickable_view) {
    // Simulate a mouse click. Note: Buttons are either fired when pressed or
    // when released, so the corresponding methods need to be called.
    clickable_view->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  ProfileMenuViewBase* profile_menu_view() {
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
    return coordinator ? coordinator->GetProfileMenuViewBaseForTesting()
                       : nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
};

IN_PROC_BROWSER_TEST_P(ProfileMenuViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMenuViewPixelTest,
                         testing::ValuesIn(kPixelTestParams),
                         &ParamToTestSuffix);
