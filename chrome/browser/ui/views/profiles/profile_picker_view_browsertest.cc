// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include <optional>
#include <set>

#include "base/barrier_closure.h"
#include "base/cfi_buildflags.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_picker_dice_reauth_provider.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/fake_account_manager_ui_dialog_waiter.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#endif

namespace {
const SkColor kProfileColor = SK_ColorRED;

// State of the the ForceEphemeralProfiles policy.
enum class ForceEphemeralProfilesPolicy { kUnset, kEnabled, kDisabled };

const char16_t kOriginalProfileName[] = u"OriginalProfile";
const char kGaiaId[] = "some_gaia_id";

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
const char16_t kWork[] = u"Work";
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
const char kReauthResultHistogramName[] = "ProfilePicker.ReauthResult";
#endif

AccountInfo FillAccountInfo(
    const CoreAccountInfo& core_info,
    const std::string& given_name,
    const std::string& hosted_domain = kNoHostedDomainFound) {
  AccountInfo account_info;
  account_info.email = core_info.email;
  account_info.gaia = core_info.gaia;
  account_info.account_id = core_info.account_id;
  account_info.is_under_advanced_protection =
      core_info.is_under_advanced_protection;
  account_info.full_name = "Test Full Name";
  account_info.given_name = given_name;
  account_info.hosted_domain = hosted_domain;
  account_info.locale = "en";
  account_info.picture_url = "https://get-avatar.com/foo";
  return account_info;
}

GURL GetSyncConfirmationURL() {
  return AppendSyncConfirmationQueryParams(GURL("chrome://sync-confirmation/"),
                                           SyncConfirmationStyle::kWindow,
                                           /*is_sync_promo=*/true);
}

class BrowserAddedWaiter : public BrowserListObserver {
 public:
  explicit BrowserAddedWaiter(size_t total_count) : total_count_(total_count) {
    BrowserList::AddObserver(this);
  }

  BrowserAddedWaiter(const BrowserAddedWaiter&) = delete;
  BrowserAddedWaiter& operator=(const BrowserAddedWaiter&) = delete;

  ~BrowserAddedWaiter() override { BrowserList::RemoveObserver(this); }

  Browser* Wait() {
    if (BrowserList::GetInstance()->size() == total_count_)
      return BrowserList::GetInstance()->GetLastActive();
    run_loop_.Run();
    EXPECT_TRUE(browser_);
    return browser_;
  }

 private:
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override {
    if (BrowserList::GetInstance()->size() != total_count_)
      return;
    browser_ = browser;
    run_loop_.Quit();
  }

  const size_t total_count_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_ = nullptr;
  base::RunLoop run_loop_;
};

class TestTabDialogs : public TabDialogs {
 public:
  TestTabDialogs(content::WebContents* contents, base::RunLoop* run_loop)
      : contents_(contents), run_loop_(run_loop) {}
  ~TestTabDialogs() override = default;

  // Creates a platform specific instance, and attaches it to |contents|.
  // If an instance is already attached, it overwrites it.
  static void OverwriteForWebContents(content::WebContents* contents,
                                      base::RunLoop* run_loop) {
    DCHECK(contents);
    contents->SetUserData(UserDataKey(),
                          std::make_unique<TestTabDialogs>(contents, run_loop));
  }

  gfx::NativeView GetDialogParentView() const override {
    return contents_->GetNativeView();
  }
  void ShowCollectedCookies() override {}
  void ShowHungRendererDialog(
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {}
  void HideHungRendererDialog(
      content::RenderWidgetHost* render_widget_host) override {}
  bool IsShowingHungRendererDialog() override { return false; }

  void ShowManagePasswordsBubble(bool user_action) override {}
  void HideManagePasswordsBubble() override {}
  void ShowDeprecatedAppsDialog(
      const extensions::ExtensionId& optional_launched_extension_id,
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents) override {}
  void ShowForceInstalledDeprecatedAppsDialog(
      const extensions::ExtensionId& app_id,
      content::WebContents* web_contents) override {}
  void ShowForceInstalledPreinstalledDeprecatedAppDialog(
      const extensions::ExtensionId& app_id,
      content::WebContents* web_contents) override {}

 private:
  raw_ptr<content::WebContents> contents_;
  raw_ptr<base::RunLoop> run_loop_;
};

class PageNonEmptyPaintObserver : public content::WebContentsObserver {
 public:
  explicit PageNonEmptyPaintObserver(const GURL& url,
                                     content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        barrier_closure_(base::BarrierClosure(2, run_loop_.QuitClosure())),
        url_(url) {}

  void Wait() {
    // Check if the right page has already been painted or loaded.
    if (web_contents()->GetLastCommittedURL() == url_) {
      if (web_contents()->CompletedFirstVisuallyNonEmptyPaint())
        DidFirstVisuallyNonEmptyPaint();
      if (!web_contents()->IsLoading())
        DidStopLoading();
    }

    run_loop_.Run();
  }

 private:
  // WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    // Making sure that the same event does not trigger the barrier twice.
    if (did_paint_)
      return;

    did_paint_ = true;
    barrier_closure_.Run();
  }

  void DidStopLoading() override {
    ASSERT_EQ(web_contents()->GetLastCommittedURL(), url_);

    // Making sure that the same event does not trigger the barrier twice.
    if (did_load_)
      return;

    // It shouldn't technically be necessary to wait for load stop here, we do
    // this to be consistent with the other tests relying on `WaitForLoadStop()`
    did_load_ = true;
    barrier_closure_.Run();
  }

  base::RunLoop run_loop_;
  base::RepeatingClosure barrier_closure_;
  GURL url_;

  bool did_paint_ = false;
  bool did_load_ = false;
};

// Waits for a first non empty paint for `target` and expects that it will load
// the given `url`.
void WaitForFirstNonEmptyPaint(const GURL& url, content::WebContents* target) {
  ASSERT_NE(target, nullptr);

  PageNonEmptyPaintObserver observer(url, target);
  observer.Wait();
}

}  // namespace

class ProfilePickerViewBrowserTest : public ProfilePickerTestBase {};

// Regression test for crbug.com/1442159.
IN_PROC_BROWSER_TEST_F(ProfilePickerViewBrowserTest,
                       ShowScreen_DoesNotFinishForErrorOnInternalNavigation) {
  GURL bad_target_url{"chrome://unregistered-host"};
  base::test::TestFuture<void> navigation_finished_future;

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL{"chrome://profile-picker"});
  view()->ShowScreenInPickerContents(bad_target_url,
                                     navigation_finished_future.GetCallback());

  WaitForLoadStop(bad_target_url, web_contents());
  EXPECT_FALSE(navigation_finished_future.IsReady());
}

// Regression test for crbug.com/1442159.
IN_PROC_BROWSER_TEST_F(ProfilePickerViewBrowserTest,
                       ShowScreen_FinishesForErrorOnStandardNavigation) {
  // URL intended to simulate an https navigation that fails because the host
  // can't be found. With an internet connection it would redirect to the
  // DNS_PROBE_FINISHED_NXDOMAIN error page. Simulate that in the picker flow
  // using the `--gaia-url` command line parameter.
  // During tests all external navigations fail anyway, but that's good enough
  // for what we're trying to verify.
  GURL bad_target_url{"https://url.madeup"};
  base::test::TestFuture<void> navigation_finished_future;

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL{"chrome://profile-picker"});
  view()->ShowScreenInPickerContents(bad_target_url,
                                     navigation_finished_future.GetCallback());

  WaitForLoadStop(bad_target_url, web_contents());
  EXPECT_TRUE(navigation_finished_future.IsReady());
}

class ProfilePickerCreationFlowBrowserTest
    : public InteractiveFeaturePromoTestT<ProfilePickerTestBase> {
 public:
  ProfilePickerCreationFlowBrowserTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHProfileSwitchFeature})) {
#if BUILDFLAG(IS_MAC)
    // Ensure the platform is unmanaged
    platform_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
#endif
  }

  void SetUpInProcessBrowserTestFixture() override {
    InteractiveFeaturePromoTestT::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ProfilePickerCreationFlowBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTestT::SetUpOnMainThread();

    // Avoid showing the What's New page. These tests assume this isn't the
    // first update and the NTP opens after sign in.
    g_browser_process->local_state()->SetInteger(prefs::kLastWhatsNewVersion,
                                                 CHROME_VERSION_MAJOR);
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&policy::FakeUserPolicySigninService::Build));

    // Clear the previous cookie responses (if any) before using it for a new
    // profile (as test_url_loader_factory() is shared across profiles).
    test_url_loader_factory()->ClearResponses();
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     test_url_loader_factory()));
  }

  Profile* SignInForNewProfile(
      const GURL& target_url,
      const std::string& email,
      const std::string& given_name,
      const std::string& hosted_domain = kNoHostedDomainFound,
      bool start_on_management_page = false) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return LacrosSignIn(target_url, email, given_name, hosted_domain,
                        start_on_management_page);
#else
    Profile* profile_being_created = StartDiceSignIn(start_on_management_page);
    FinishDiceSignIn(profile_being_created, email, given_name, hosted_domain);
    WaitForLoadStop(target_url);
    return profile_being_created;
#endif
  }

  // Returns the initial page.
  GURL ShowPickerAndWait(bool start_on_management_page = false) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        start_on_management_page
            ? ProfilePicker::EntryPoint::kProfileMenuManageProfiles
            : ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
    // Wait until webUI is fully initialized.
    const GURL kInitialPageUrl(start_on_management_page
                                   ? "chrome://profile-picker"
                                   : "chrome://profile-picker/new-profile");
    WaitForLoadStop(kInitialPageUrl);
    return kInitialPageUrl;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* LacrosSignIn(const GURL& target_url,
                        const std::string& email,
                        const std::string& given_name,
                        const std::string& hosted_domain = kNoHostedDomainFound,
                        bool start_on_management_page = false) {
    const GURL kProfilePickerUrl = ShowPickerAndWait(start_on_management_page);

    account_manager::AccountKey kAccountKey{
        kGaiaId, account_manager::AccountType::kGaia};
    auto* account_manager = MaybeGetAshAccountManagerForTests();
    DCHECK(account_manager);
    if (account_manager->IsTokenAvailable(kAccountKey)) {
      // Account already exists on the device. Fake clicking the account button.
      base::Value::List args;
      args.Append(/*color=*/static_cast<int>(kProfileColor));
      args.Append(/*gaiaid=*/kGaiaId);
      web_contents()->GetWebUI()->ProcessWebUIMessage(
          kProfilePickerUrl, "selectExistingAccountLacros", std::move(args));
    } else {
      // The account needs to be added to the device.
      // Fake clicking the "Use another account" button.
      base::Value::List args;
      args.Append(/*color=*/static_cast<int>(kProfileColor));
      web_contents()->GetWebUI()->ProcessWebUIMessage(
          kProfilePickerUrl, "selectNewAccount", std::move(args));
      // Wait for the Ash UI to show up.
      FakeAccountManagerUI* fake_ui = GetFakeAccountManagerUI();
      FakeAccountManagerUIDialogWaiter(
          fake_ui, FakeAccountManagerUIDialogWaiter::Event::kAddAccount)
          .Wait();

      // Fake the OS account addition.
      account_manager->UpsertAccount(kAccountKey, email, "access_token");

      // Fake that this account was successfully added via the UI.
      crosapi::AccountManagerMojoService* mojo_service =
          MaybeGetAshAccountManagerMojoServiceForTests();
      DCHECK(mojo_service);
      mojo_service->OnAccountUpsertionFinishedForTesting(
          account_manager::AccountUpsertionResult::FromAccount(
              {kAccountKey, email}));
      fake_ui->CloseDialog();
    }

    WaitForLoadStop(target_url);
    // `contents_profile` is either a new profile, or the system profile if the
    // "profile switch" interstitial is shown.
    Profile* contents_profile =
        static_cast<Profile*>(web_contents()->GetBrowserContext());

    // Add full account info.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(contents_profile);
    if (identity_manager) {
      CoreAccountInfo core_account_info =
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin);
      AccountInfo account_info =
          FillAccountInfo(core_account_info, given_name, hosted_domain);
      signin::UpdateAccountInfoForAccount(identity_manager, account_info);
    }

    return contents_profile;
  }
#else
  // Opens the Gaia signin page in the profile creation flow. Returns the new
  // profile that was created.
  Profile* StartDiceSignIn(bool start_on_management_page = false) {
    ShowPickerAndWait(start_on_management_page);

    // Simulate a click on the signin button.
    base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
    EXPECT_CALL(switch_finished_callback, Run(true));
    ProfilePicker::SwitchToDiceSignIn(kProfileColor,
                                      switch_finished_callback.Get());

    // The DICE navigation happens in a new web contents (for the profile being
    // created), wait for it.
    WaitForLoadStop(GetSigninChromeSyncDiceUrl());

    // Check that the `DiceTabHelper` was created.
    DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents());
    CHECK(tab_helper);
    EXPECT_EQ(tab_helper->signin_access_point(),
              signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER);

    return static_cast<Profile*>(web_contents()->GetBrowserContext());
  }

  void SimulateEnableSyncDiceHeader(content::WebContents* contents,
                                    const CoreAccountInfo& account_info) {
    // Simulate the Dice "ENABLE_SYNC" header parameter.
    auto process_dice_header_delegate_impl =
        ProcessDiceHeaderDelegateImpl::Create(contents);
    process_dice_header_delegate_impl->EnableSync(account_info);
  }

  AccountInfo FinishDiceSignIn(
      Profile* profile_being_created,
      const std::string& email,
      const std::string& given_name,
      const std::string& hosted_domain = kNoHostedDomainFound) {
    // Add an account - simulate a successful Gaia sign-in.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_being_created);
    CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
        identity_manager,
        signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithAccessPoint(
                signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER)
            .Build(email));
    EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
        core_account_info.account_id));

    AccountInfo account_info =
        FillAccountInfo(core_account_info, given_name, hosted_domain);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    if (web_contents()) {
      SimulateEnableSyncDiceHeader(web_contents(), core_account_info);
    }

    // The flow should work even if the primary account is not set at this
    // point,for example because the /ListAccounts call did not complete yet.
    // Regression test for https://crbug.com/1469586
    EXPECT_FALSE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    return account_info;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Returns true if the profile switch IPH has been shown.
  bool ProfileSwitchPromoHasBeenShown(Browser* browser) {
    return feature_engagement::TrackerFactory::GetForBrowserContext(
               browser->profile())
        ->HasEverTriggered(feature_engagement::kIPHProfileSwitchFeature,
                           /*from_window=*/false);
  }

  // Simulates a click on a profile card. The profile picker must be already
  // opened.
  void OpenProfileFromPicker(const base::FilePath& profile_path,
                             bool open_settings) {
    base::Value::List args;
    args.Append(base::FilePathToValue(profile_path));
    profile_picker_handler()->HandleLaunchSelectedProfile(open_settings, args);
  }

  // Simulates a click on "Browse as Guest".
  void OpenGuestFromPicker() {
    base::Value::List args;
    profile_picker_handler()->HandleLaunchGuestProfile(args);
  }

  // Creates a new profile without opening a browser.
  base::FilePath CreateNewProfileWithoutBrowser() {
    // Create a second profile.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path, base::BindLambdaForTesting([&run_loop](Profile* profile) {
          ASSERT_TRUE(profile);
          // Avoid showing the welcome page.
          profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);
          run_loop.Quit();
        }));
    run_loop.Run();
    return path;
  }

  // Simulates a click on "Continue without an account" to create a local
  // profile and open the profile customization dialog.
  void CreateLocalProfile() {
    base::Value::List args;
    args.Append(base::Value());
    profile_picker_handler()->HandleContinueWithoutAccount(args);
  }

  // Simulates a click on "Done" on the Profile Customization to confirm the
  // creation of the local profile.
  void ConfirmLocalProfileCreation(content::WebContents* dialog_web_contents) {
    base::Value::List args;
    args.Append(base::Value(kLocalProfileName));
    dialog_web_contents->GetWebUI()
        ->GetController()
        ->GetAs<ProfileCustomizationUI>()
        ->GetProfileCustomizationHandlerForTesting()
        ->HandleDone(args);
  }

  // Simulates a click on "Delete profile" on the Profile Customization to
  // cancel the creation of the local profile.
  void DeleteLocalProfile(content::WebContents* dialog_web_contents) {
    dialog_web_contents->GetWebUI()
        ->GetController()
        ->GetAs<ProfileCustomizationUI>()
        ->GetProfileCustomizationHandlerForTesting()
        ->HandleDeleteProfile(base::Value::List());
  }

  // Returns profile picker webUI handler. Profile picker must be opened before
  // calling this function.
  ProfilePickerHandler* profile_picker_handler() {
    DCHECK(ProfilePicker::IsOpen());
    return web_contents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<ProfilePickerUI>()
        ->GetProfilePickerHandlerForTesting();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void SimulateNavigateBack() {
    // Use "Command [" for Mac and "Alt Left" for the other operating systems.
#if BUILDFLAG(IS_MAC)
    view()->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN));
#else
    view()->AcceleratorPressed(ui::Accelerator(ui::VKEY_LEFT, ui::EF_ALT_DOWN));
#endif
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool IsNativeToolbarVisible() {
    return view()->IsNativeToolbarVisibleForTesting();
  }
#endif

 protected:
  const GURL kLocalProfileCreationUrl = AppendProfileCustomizationQueryParams(
      GURL("chrome://profile-customization"),
      ProfileCustomizationStyle::kLocalProfileCreation);
  const std::string kLocalProfileName = "LocalProfile";

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList scoped_feature_list_{
      kForceSigninFlowInProfilePicker};
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_;
#endif
};

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowPicker) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForPickerWidgetCreated();
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowChoice) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForPickerWidgetCreated();
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfile) {
  base::HistogramTester histogram_tester;

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  Profile* profile_being_created = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");

  signin_metrics::AccessPoint expected_access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40202341): Record signin access point on Lacros.
  expected_access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_DESKTOP_SIGNIN_MANAGER;
#endif
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed",
                                      expected_access_point, 1);

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

// Regression test for https://crbug.com/1431342
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileClosePicker) {
  // Closes the picker at the same time the new browser is created.
  class ClosePickerOnBrowserAddedObserver : public BrowserListObserver {
   public:
    ClosePickerOnBrowserAddedObserver() { BrowserList::AddObserver(this); }

    // This observer is registered early, before the call to
    // `OpenBrowserWindowForProfile()` in `ProfileManagementFlowController`. It
    // causes the `ProfileManagementFlowController` to be deleted before its
    // `clear_host_callback_` is called
    void OnBrowserAdded(Browser* browser) override {
      BrowserList::RemoveObserver(this);
      ProfilePicker::Hide();
    }
  };

  ClosePickerOnBrowserAddedObserver close_picker_on_browser_added;

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  Profile* profile_being_created = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  Browser* new_browser = BrowserAddedWaiter(/*total_count=*/2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40868761): Test is flaky on Linux and Windows.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_CreateForceSignedInProfile DISABLED_CreateForceSignedInProfile
#else
#define MAYBE_CreateForceSignedInProfile CreateForceSignedInProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_CreateForceSignedInProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter{true};
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, g_browser_process->profile_manager()->GetNumberOfProfiles());

  // Note: Observed some rare flakiness on some bots. Inclusing some logs to
  // understand it.
  LOG(WARNING) << "DEBUG - Before showing the picker.";

  // Wait for the picker to open on the profile creation flow.
  ShowPickerAndWait();
  auto* profile_picker_view =
      static_cast<ProfilePickerView*>(ProfilePicker::GetViewForTesting());

  if (base::FeatureList::IsEnabled(kForceSigninFlowInProfilePicker)) {
    // The DICE navigation happens in a new web contents (for the profile being
    // created), wait for it.
    profiles::testing::WaitForPickerUrl(GetSigninChromeSyncDiceUrl());
  } else {
    // Wait for the force signin dialog to load.
    LOG(WARNING)
        << "DEBUG - Picker shown. Ensuring that the dialog is shown. "
        << (profile_picker_view->dialog_host_.GetDialogDelegateViewForTesting()
                ? "We already"
                : "We don't yet")
        << " have a dialog delegate view.";
    GURL force_signin_webui_url = signin::GetEmbeddedPromoURL(
        signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
        signin_metrics::Reason::kForcedSigninPrimaryAccount, true);
    force_signin_webui_url =
        AddFromProfilePickerURLParameter(force_signin_webui_url);

    // Memorize the WebContents that shows the sign-in URL.
    auto* signin_web_contents =
        profile_picker_view->get_dialog_web_contents_for_testing();
    WaitForLoadStop(force_signin_webui_url, signin_web_contents);
    LOG(WARNING) << "DEBUG - Finished waiting for the dialog.";

    // The dialog view should be created.
    EXPECT_TRUE(
        profile_picker_view->dialog_host_.GetDialogDelegateViewForTesting());

    // A new profile should have been created for the forced sign-in flow.
    EXPECT_EQ(2u, g_browser_process->profile_manager()->GetNumberOfProfiles());
    // Get the profile that was used to load the `force_signin_webui_url`.
    Profile* force_signin_profile =
        Profile::FromBrowserContext(signin_web_contents->GetBrowserContext());
    EXPECT_TRUE(force_signin_profile);
    // Make sure that the force_signin profile is different from the main one.
    EXPECT_FALSE(force_signin_profile->IsSameOrParent(browser()->profile()));

    // The tail end of the flow is handled by inline_login_*
  }
}

// Force signin is disabled on Linux and ChromeOS.
// TODO(crbug.com/40235093): enable this test when enabling force sign in
// on Linux.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
class ForceSigninProfilePickerCreationFlowBrowserTest
    : public ProfilePickerCreationFlowBrowserTest {
 public:
  explicit ForceSigninProfilePickerCreationFlowBrowserTest(
      bool force_signin_enabled = true)
      : force_signin_setter_(force_signin_enabled) {}

  void SimulateSuccesfulSignin(signin::IdentityManager* identity_manager,
                               const std::string& email) {
    // Simulate a successful reauth by making the account available.
    signin::MakeAccountAvailable(identity_manager, email);
  }

  bool IsForceSigninErrorDialogShown() {
    CheckMainProfilePickerUrlOpened();
    return content::EvalJs(web_contents(),
                           // Check the `open` field
                           base::StrCat({kForceSigninErrorDialogPath, ".open"}))
        .ExtractBool();
  }

  std::u16string GetForceSigninErrorDialogTitleText() {
    CheckMainProfilePickerUrlOpened();
    return std::u16string(base::TrimWhitespace(
        base::UTF8ToUTF16(
            content::EvalJs(
                web_contents(),
                // Get the title text content of the dialog.
                base::StrCat({kForceSigninErrorDialogPath,
                              ".querySelector(\'#dialog-title\').textContent"}))
                .ExtractString()),
        base::TRIM_ALL));
  }

  std::u16string GetForceSigninErrorDialogBodyText() {
    CheckMainProfilePickerUrlOpened();
    return std::u16string(base::TrimWhitespace(
        base::UTF8ToUTF16(
            content::EvalJs(
                web_contents(),
                // Get the body text content of the dialog.
                base::StrCat({kForceSigninErrorDialogPath,
                              ".querySelector(\'#dialog-body\').textContent"}))
                .ExtractString()),
        base::TRIM_ALL));
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void CheckMainProfilePickerUrlOpened() {
    // Make sure the profile picker is opened, with the main profile picker view
    // (where the dialog can be shown), and the page is fully loaded.
    EXPECT_TRUE(ProfilePicker::IsOpen());
    const GURL main_profile_picker_url("chrome://profile-picker");
    EXPECT_EQ(web_contents()->GetURL().GetWithEmptyPath(),
              main_profile_picker_url);
    WaitForLoadStop(main_profile_picker_url);
  }

  // 'forceSigninErrorDialog' cr-dialog node.
  static constexpr char kForceSigninErrorDialogPath[] =
      "document.body.getElementsByTagName('profile-picker-app')[0]."
      "shadowRoot.getElementById('mainView').shadowRoot."
      "getElementById(\'forceSigninErrorDialog\')";

  signin_util::ScopedForceSigninSetterForTesting force_signin_setter_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninSuccessful) {
  size_t initial_browser_count = BrowserList::GetInstance()->size();
  // Create a new signin flow, sign-in, and wait for the Sync Comfirmation
  // promo.
  Profile* force_sign_in_profile =
      SignInForNewProfile(GetSyncConfirmationURL(), "joe.consumer@gmail.com",
                          "Joe", kNoHostedDomainFound, true);
  // No browser for the created profile exist yet.
  ASSERT_EQ(chrome::GetBrowserCount(force_sign_in_profile), 0u);
  ASSERT_TRUE(ProfilePicker::IsOpen());

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(force_sign_in_profile->GetPath());
  // Profile is still locked and ephemeral at this point.
  EXPECT_EQ(entry->IsSigninRequired(), true);
  EXPECT_EQ(entry->IsEphemeral(), true);

  // Simulate the "Yes, I'm in" button clicked.
  LoginUIServiceFactory::GetForProfile(force_sign_in_profile)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // A browser should open up and the picker should be closed.
  Browser* new_browser = BrowserAddedWaiter(initial_browser_count + 1u).Wait();
  WaitForPickerClosed();

  // The browser is for the newly created profile.
  EXPECT_EQ(new_browser->profile(), force_sign_in_profile);
  // Profile is unlocked and ready to be used.
  EXPECT_EQ(entry->IsSigninRequired(), false);
  EXPECT_EQ(entry->IsEphemeral(), false);
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninAbortedBySyncDeclined_ThenSigninAgain) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Only the default profile exists at this point.
  size_t initial_profile_count = 1u;
  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), initial_profile_count);

  // Create a new signin flow, sign-in, and wait for the Sync Comfirmation
  // promo.
  Profile* force_sign_in_profile =
      SignInForNewProfile(GetSyncConfirmationURL(), "joe.consumer@gmail.com",
                          "Joe", kNoHostedDomainFound, true);
  base::FilePath force_sign_in_profile_path = force_sign_in_profile->GetPath();
  // No browser for the created profile exist yet.
  ASSERT_EQ(chrome::GetBrowserCount(force_sign_in_profile), 0u);
  ASSERT_TRUE(ProfilePicker::IsOpen());

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(force_sign_in_profile_path);
  ASSERT_NE(entry, nullptr);

  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), initial_profile_count + 1);
  // Profile is still locked and ephemeral at this point.
  EXPECT_EQ(entry->IsSigninRequired(), true);
  EXPECT_EQ(entry->IsEphemeral(), true);

  ProfileDeletionObserver deletion_observer;
  // Simulate the "No thanks" button clicked.
  LoginUIServiceFactory::GetForProfile(force_sign_in_profile)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  // Expect the profile to be deleted.
  deletion_observer.Wait();

  // Expect a redirect to the initial page of the profile picker.
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // The created profile path entry is now deletec.
  EXPECT_EQ(storage.GetProfileAttributesWithPath(force_sign_in_profile_path),
            nullptr);
  // Makes sure that the only profile that exist is the default one and not the
  // one we attempted to create.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), initial_profile_count);

  // ---------------------------------------------------------------------------
  // This part of the test is to make sure we can safely instantiate a new sign
  // in flow after declining the first one.
  // ---------------------------------------------------------------------------

  size_t initial_browser_count = BrowserList::GetInstance()->size();

  // Create a second signin flow as part of the same session.
  Profile* force_sign_in_profile_2 =
      SignInForNewProfile(GetSyncConfirmationURL(), "joe.consumer1@gmail.com",
                          "Joe", kNoHostedDomainFound, true);

  LoginUIServiceFactory::GetForProfile(force_sign_in_profile_2)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  Browser* new_browser = BrowserAddedWaiter(initial_browser_count + 1u).Wait();
  WaitForPickerClosed();

  // The browser is for the newly created profile.
  EXPECT_EQ(new_browser->profile(), force_sign_in_profile_2);
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninReauthSuccessful) {
  size_t initial_browser_count = BrowserList::GetInstance()->size();
  ASSERT_EQ(initial_browser_count, 0u);

  const std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  ASSERT_GE(profiles.size(), 1u);
  Profile* profile = profiles[0];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  ASSERT_TRUE(entry->IsSigninRequired());
  ASSERT_TRUE(ProfilePicker::IsOpen());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  const std::string email("test@managedchrome.com");
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);
  // Only managed accounts are allowed to reauth.
  entry->SetUserAcceptedAccountManagement(true);

  CoreAccountId primary_account =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.empty());

  // Simulate an invalid account.
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // Opening the locked profile from the profile picker should trigger the
  // reauth.
  OpenProfileFromPicker(entry->GetPath(), false);
  WaitForLoadStop(GetChromeReauthURL(email));

  // Simulate a successful reauth with the existing email.
  SimulateSuccesfulSignin(identity_manager, email);

  // A browser should open and the profile should now be unlocked.
  Browser* new_browser = BrowserAddedWaiter(initial_browser_count + 1u).Wait();
  EXPECT_TRUE(new_browser);
  EXPECT_EQ(new_browser->profile(), profile);
  EXPECT_FALSE(entry->IsSigninRequired());
  histogram_tester()->ExpectUniqueSample(
      kReauthResultHistogramName, ProfilePickerReauthResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninReauthWithAnotherAccount) {
  size_t initial_browser_count = BrowserList::GetInstance()->size();
  ASSERT_EQ(initial_browser_count, 0u);

  const std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  ASSERT_GE(profiles.size(), 1u);
  Profile* profile = profiles[0];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  ASSERT_TRUE(entry->IsSigninRequired());
  ASSERT_TRUE(ProfilePicker::IsOpen());
  ASSERT_FALSE(IsForceSigninErrorDialogShown());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  const std::string email("test@managedchrome.com");
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);
  // Only managed accounts are allowed to reauth.
  entry->SetUserAcceptedAccountManagement(true);

  CoreAccountId primary_account =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.empty());

  // Simulate an invalid account.
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // Opening the locked profile from the profile picker should trigger the
  // reauth.
  OpenProfileFromPicker(entry->GetPath(), false);
  WaitForLoadStop(GetChromeReauthURL(email));

  // Simulate a successful sign in with another email address.
  const std::string different_email("test2@managedchrome.com");
  ASSERT_NE(email, different_email);
  SimulateSuccesfulSignin(identity_manager, different_email);

  // Expect the profile picker to be opened instead of a browser, and the
  // profile to be still locked.
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_TRUE(IsForceSigninErrorDialogShown());
  // Check error dialog content.
  ForceSigninUIError::UiTexts errors =
      ForceSigninUIError::ReauthWrongAccount(email).GetErrorTexts();
  EXPECT_EQ(GetForceSigninErrorDialogTitleText(), errors.first);
  EXPECT_EQ(GetForceSigninErrorDialogBodyText(), errors.second);
  EXPECT_EQ(BrowserList::GetInstance()->size(), initial_browser_count);
  EXPECT_TRUE(entry->IsSigninRequired());
  histogram_tester()->ExpectUniqueSample(
      kReauthResultHistogramName, ProfilePickerReauthResult::kErrorUsedNewEmail,
      1);
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninReauthNavigateBackShouldAbort) {
  size_t initial_browser_count = BrowserList::GetInstance()->size();
  ASSERT_EQ(initial_browser_count, 0u);

  const std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  ASSERT_GE(profiles.size(), 1u);
  Profile* profile = profiles[0];
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  ASSERT_TRUE(entry->IsSigninRequired());
  ASSERT_TRUE(ProfilePicker::IsOpen());

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  const std::string email("test@managedchrome.com");
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);
  // Only managed accounts are allowed to reauth.
  entry->SetUserAcceptedAccountManagement(true);

  CoreAccountId primary_account =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(primary_account.empty());

  // Simulate an invalid account.
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  // Opening the locked profile from the profile picker should trigger the
  // reauth, and the back button toolbar should be visible.
  OpenProfileFromPicker(entry->GetPath(), false);
  WaitForLoadStop(GetChromeReauthURL(email));
  EXPECT_TRUE(IsNativeToolbarVisible());

  // Simulate a redirect within the reauth page (requesting a password for
  // example), the actual URL is not important for the testing purposes.
  GURL redirect_url("https://www.google.com/");
  web_contents()->GetController().LoadURL(redirect_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          std::string());
  WaitForLoadStop(redirect_url);

  // Simulate a back navigation within the reauth redirect.
  SimulateNavigateBack();

  // Expect it to take us back to the initial reauth page.
  WaitForLoadStop(GetChromeReauthURL(email));

  // Simulate a back navigation within the reauth page.
  SimulateNavigateBack();

  // Expect the profile picker to be opened since it was the last step before
  // reauth, toolbar should be hidden, and the profile to be still locked.
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_FALSE(IsNativeToolbarVisible());
  EXPECT_EQ(BrowserList::GetInstance()->size(), initial_browser_count);
  EXPECT_TRUE(entry->IsSigninRequired());
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTest,
                       ForceSigninLaunchInactiveDefaultProfile) {
  size_t initial_browser_count = BrowserList::GetInstance()->size();
  ASSERT_EQ(initial_browser_count, 0u);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GE(profiles.size(), 1u);
  Profile* default_profile = profiles[0];
  ProfileAttributesEntry* default_profile_entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(default_profile->GetPath());

  ASSERT_TRUE(ProfilePicker::IsOpen());
  // Make sure that this is the default profile. Also this profile is not yet
  // active/used which makes a valid candidate to sign in.
  ASSERT_EQ(default_profile_entry->GetPath(),
            profiles::GetDefaultProfileDir(profile_manager->user_data_dir()));
  ASSERT_EQ(default_profile_entry->GetActiveTime(), base::Time());
  ASSERT_TRUE(default_profile_entry->IsSigninRequired());

  // Opening the default profile for the first time is allowed, it is expected
  // to open the sign in screen.
  OpenProfileFromPicker(default_profile_entry->GetPath(), false);
  WaitForLoadStop(GetSigninChromeSyncDiceUrl());

  // Finish the signin that was started from opening the default profile.
  FinishDiceSignIn(default_profile, "joe.consumer@gmail.com", "Joe");
  WaitForLoadStop(GetSyncConfirmationURL());

  // Accept Sync.
  LoginUIServiceFactory::GetForProfile(default_profile)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // A browser should open and the profile should now be unlocked.
  Browser* new_browser = BrowserAddedWaiter(initial_browser_count + 1u).Wait();
  EXPECT_TRUE(new_browser);
  EXPECT_EQ(new_browser->profile(), default_profile);
  EXPECT_FALSE(default_profile_entry->IsSigninRequired());

  // Default profile is now active.
  EXPECT_NE(default_profile_entry->GetActiveTime(), base::Time());
}

// Regression tetst for b/360733721.
IN_PROC_BROWSER_TEST_F(
    ForceSigninProfilePickerCreationFlowBrowserTest,
    ForceSigninWithPatternMatchingShouldFailSigninWithWrongPatternEmail) {
  // Set the username pattern restriction.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kGoogleServicesUsernamePattern, "*.google.com");

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t initial_number_of_profiles = profile_manager->GetNumberOfProfiles();

  ASSERT_TRUE(ProfilePicker::IsOpen());

  GURL initial_picker_url =
      ProfilePicker::GetWebViewForTesting()->GetWebContents()->GetURL();

  // Start the signin process.
  Profile* profile_being_created = StartDiceSignIn(true);
  // Profile will be destroyed at the end of the flow.
  ProfileDestructionWaiter destruction_waiter(profile_being_created);
  // During signin process a new profile is created.
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(),
            initial_number_of_profiles + 1u);

  // Make sure that the ProfilePicker navigated.
  EXPECT_NE(initial_picker_url,
            ProfilePicker::GetWebViewForTesting()->GetWebContents()->GetURL());

  const std::string email = "joe.consumer@gmail.com";
  // Verify that patternt does not match.
  ASSERT_FALSE(signin::IsUsernameAllowedByPatternFromPrefs(local_state, email));
  // Signing in with a profile that does not match the pattern should stop the
  // profile creation flow.
  FinishDiceSignIn(profile_being_created, email, "Joe", kNoHostedDomainFound);

  // Returning to the profile picker main page.
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Created profile is destroyed.
  destruction_waiter.Wait();
  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), initial_number_of_profiles);
  EXPECT_TRUE(IsForceSigninErrorDialogShown());
  // Check error dialog content.
  ForceSigninUIError::UiTexts errors =
      ForceSigninUIError::SigninPatternNotMatching(email).GetErrorTexts();
  EXPECT_EQ(GetForceSigninErrorDialogTitleText(), errors.first);
  EXPECT_EQ(GetForceSigninErrorDialogBodyText(), errors.second);
}

class ForceSigninProfilePickerCreationFlowBrowserTestWithPRE
    : public ForceSigninProfilePickerCreationFlowBrowserTest {
 public:
  ForceSigninProfilePickerCreationFlowBrowserTestWithPRE()
      : ForceSigninProfilePickerCreationFlowBrowserTest(
            /*force_signin_enabled=*/!content::IsPreTest()) {}
};

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTestWithPRE,
                       PRE_ProfileThatCannotBeManagedCannotBeOpened) {
  ASSERT_FALSE(signin_util::IsForceSigninEnabled());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Only default profile exists.
  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());

  // Activate this profile then close the session.
  CreateBrowser(profile_manager->GetLoadedProfiles()[0]);
}

IN_PROC_BROWSER_TEST_F(ForceSigninProfilePickerCreationFlowBrowserTestWithPRE,
                       ProfileThatCannotBeManagedCannotBeOpened) {
  ASSERT_TRUE(signin_util::IsForceSigninEnabled());

  size_t initial_browser_count = BrowserList::GetInstance()->size();
  ASSERT_EQ(0u, initial_browser_count);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<ProfileAttributesEntry*> entries =
      profile_manager->GetProfileAttributesStorage().GetAllProfilesAttributes();
  ASSERT_EQ(1u, entries.size());
  // Use the same (only) profile as in PRE.
  ProfileAttributesEntry* existing_entry = entries[0];

  // Profile has been used and is now locked.
  ASSERT_NE(existing_entry->GetActiveTime(), base::Time());
  ASSERT_TRUE(existing_entry->IsSigninRequired());
  ASSERT_FALSE(existing_entry->CanBeManaged());

  ASSERT_TRUE(ProfilePicker::IsOpen());
  ASSERT_FALSE(IsForceSigninErrorDialogShown());

  // Attempting to open this profile, profile was previously active and not
  // syncing/managed.
  OpenProfileFromPicker(existing_entry->GetPath(), false);

  // Should not succeed.
  EXPECT_EQ(initial_browser_count, BrowserList::GetInstance()->size());
  // Error dialog is shown on top of the ProfilePicker.
  EXPECT_TRUE(IsForceSigninErrorDialogShown());
  // Check error dialog content.
  ForceSigninUIError::UiTexts errors =
      ForceSigninUIError::ReauthNotAllowed().GetErrorTexts();
  EXPECT_EQ(GetForceSigninErrorDialogTitleText(), errors.first);
  EXPECT_EQ(GetForceSigninErrorDialogBodyText(), errors.second);
  // Profile is still locked.
  EXPECT_TRUE(existing_entry->IsSigninRequired());
}

#endif

// Regression test for crbug.com/1266415.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileWithSyncEncryptionKeys) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  StartDiceSignIn();

  // It would be nicer to verify that HasEncryptionKeysApiForTesting()
  // returns true but this isn't possible because the sigin page returns an
  // error, without setting up a fake HTTP server.
  EXPECT_NE(
      TrustedVaultEncryptionKeysTabHelper::FromWebContents(web_contents()),
      nullptr);
}

// Regression test for crbug.com/1196290. Makes no sense for lacros because you
// cannot sign-in twice in the same way on lacros.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileAfterCancellingFirstAttempt) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  Profile* profile_to_cancel = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");

  // Close the flow with the [X] button.
  ProfileDeletionObserver observer;
  base::FilePath canceled_path = profile_to_cancel->GetPath();
  widget()->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  WaitForPickerClosed();
  observer.Wait();

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  // The canceled profile got marked for deletion.
  ASSERT_EQ(storage.GetProfileAttributesWithPath(canceled_path), nullptr);

  // Restart the flow again. As the flow for `profile_to_cancel` got aborted,
  // it's disregarded. Instead of the profile switch screen, the normal sync
  // confirmation should appear.
  Profile* profile_being_created = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");
  EXPECT_NE(canceled_path, profile_being_created->GetPath());

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CancelWhileSigningIn) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_to_cancel = StartDiceSignIn();
  base::FilePath profile_to_cancel_path = profile_to_cancel->GetPath();

  // Close the flow with the [X] button.
  ProfileDeletionObserver observer;
  widget()->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  WaitForPickerClosed();
  observer.Wait();

  // The profile entry is deleted.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_to_cancel_path);
  EXPECT_EQ(entry, nullptr);
}

// Regression test for crbug.com/1278726.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CancelWhileSigningInBeforeProfileCreated) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  // Wait until webUI is fully initialized.
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run).Times(0);
  ProfilePicker::SwitchToDiceSignIn(kProfileColor,
                                    switch_finished_callback.Get());

  // Close the flow immediately with the [X] button before
  // `switch_finished_callback` gets called (and before the respective profile
  // gets created).
  widget()->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  // The flow should not crash.
  WaitForPickerClosed();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       PRE_CancelWhileSigningInWithNoOtherWindow) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_to_cancel = StartDiceSignIn();
  base::FilePath profile_to_cancel_path = profile_to_cancel->GetPath();

  // First close all browser windows to make sure Chrome quits when closing the
  // flow.
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Close the flow with the [X] button.
  widget()->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  WaitForPickerClosed();

  // The profile entry is not yet deleted when Chrome is shutting down, but it
  // will be deleted at next startup since it is an ephemeral profile.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_to_cancel_path);
  EXPECT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsEphemeral());
  ASSERT_EQ(2u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());

  // Still no browser window is open.
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CancelWhileSigningInWithNoOtherWindow) {
  // There is only one profile left.
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
}

// Tests dice-specific logic for keeping track of the new profile color.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileDiceReenter) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartDiceSignIn();
  EXPECT_TRUE(IsNativeToolbarVisible());

  // Navigate back from the sign in step.
  SimulateNavigateBack();
  EXPECT_FALSE(IsNativeToolbarVisible());

  // Simulate the sign-in screen get re-entered with a different color
  // (configured on the local profile screen).
  const SkColor kDifferentProfileColor = SK_ColorBLUE;
  base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
  EXPECT_CALL(switch_finished_callback, Run(true));
  ProfilePicker::SwitchToDiceSignIn(kDifferentProfileColor,
                                    switch_finished_callback.Get());

  // Simulate a successful Gaia sign-in.
  FinishDiceSignIn(profile_being_created, "joe.consumer@gmail.com", "Joe");

  // Wait for the sign-in to propagate to the flow, resulting in sync
  // confirmation screen getting displayed.
  WaitForLoadStop(GetSyncConfirmationURL());

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kDifferentProfileColor);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/40817459) Test is flaky on Linux CFI, Linux dbg, Mac ASan
#if ((BUILDFLAG(CFI_ICALL_CHECK) || !defined(NDEBUG)) && \
     BUILDFLAG(IS_LINUX)) ||                             \
    (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define MAYBE_CreateSignedInProfileSettings \
  DISABLED_CreateSignedInProfileSettings
#else
#define MAYBE_CreateSignedInProfileSettings CreateSignedInProfileSettings
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_CreateSignedInProfileSettings) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  Profile* profile_being_created = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");

  // Simulate closing the UI with "Yes, I'm in".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://settings/syncSetup"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");

  // Sync is getting configured.
  EXPECT_TRUE(entry->IsAuthenticated());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_TRUE(sync_service->HasSyncConsent());
  EXPECT_FALSE(
      sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  // The color is not applied if the user enters settings.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->GetUserColor()
                   .has_value());
}

// The following tests rely on dice specific logic. Some of them could be
// extended to cover lacros as well.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileOpenLink) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  StartDiceSignIn();

  // Simulate clicking on a link that opens in a new window.
  const GURL kURL("https://foo.google.com");
  EXPECT_TRUE(ExecJs(web_contents(),
                     "var link = document.createElement('a');"
                     "link.href = '" +
                         kURL.spec() +
                         "';"
                         "link.target = '_blank';"
                         "document.body.appendChild(link);"
                         "link.click();"));
  // A new pppup browser is displayed (with the specified URL).
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  EXPECT_EQ(new_browser->type(), Browser::TYPE_POPUP);
  WaitForLoadStop(kURL, new_browser->tab_strip_model()->GetActiveWebContents());
}

// Regression test for crbug.com/1219980.
// TODO(crbug.com/40772284): Re-implement the test bases on the final fix.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileSecurityInterstitials) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  StartDiceSignIn();

  // Simulate clicking on the settings link in a security interstitial (that
  // appears in the sign-in flow e.g. due to broken internet connection).
  security_interstitials::ChromeSettingsPageHelper::
      CreateChromeSettingsPageHelper()
          ->OpenEnhancedProtectionSettings(web_contents());
  // Nothing happens, the browser should not crash.
  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/40197099): Extend this test to support lacros.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileExtendedInfoTimeout) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartDiceSignIn();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);

  // Make it work without waiting for a long delay.
  auto timeout_override =
      ProfileNameResolver::CreateScopedInfoFetchTimeoutOverrideForTesting(
          base::Milliseconds(10));

  // Add an account - simulate a successful Gaia sign-in.
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .Build("joe.consumer@gmail.com"));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Simulate the Dice "ENABLE_SYNC" header parameter, resulting in sync
  // confirmation screen getting displayed.
  SimulateEnableSyncDiceHeader(web_contents(), core_account_info);
  WaitForLoadStop(GetSyncConfirmationURL());

  // Simulate closing the UI with "No, Thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  // Since the given name is not provided, the email address is used instead as
  // a profile name.
  EXPECT_EQ(entry->GetLocalProfileName(), u"joe.consumer@gmail.com");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

// TODO(crbug.com/40197099): Extend this test to support lacros.
// TODO(crbug.com/41496960): Flaky on Linux MSan.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_CreateSignedInProfileExtendedInfoDelayed \
  DISABLED_CreateSignedInProfileExtendedInfoDelayed
#else
#define MAYBE_CreateSignedInProfileExtendedInfoDelayed \
  CreateSignedInProfileExtendedInfoDelayed
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_CreateSignedInProfileExtendedInfoDelayed) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartDiceSignIn();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);

  // Add an account - simulate a successful Gaia sign-in.
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .Build("joe.consumer@gmail.com"));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Simulate the Dice "ENABLE_SYNC" header parameter, resulting in sync
  // confirmation screen getting displayed.
  SimulateEnableSyncDiceHeader(web_contents(), core_account_info);
  WaitForLoadStop(GetSyncConfirmationURL());

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  base::RunLoop().RunUntilIdle();

  // Add full account info.
  AccountInfo account_info =
      FillAccountInfo(core_account_info, "Joe", kNoHostedDomainFound);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Check expectations when the profile creation flow is closes.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  // Even if the given name is provided after the user clicked to complete the
  // flow, we still wait to use it as  profile name.
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileWithSAML) {
  const GURL kNonGaiaURL("https://signin.saml-provider.com/");

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartDiceSignIn();

  // Redirect the web contents to a non gaia url (simulating a SAML page).
  content::WebContents* wc = web_contents();
  wc->GetController().LoadURL(kNonGaiaURL, content::Referrer(),
                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(kNonGaiaURL, wc);
  WaitForPickerClosed();

  // Check that the web contents got actually moved to the browser.
  EXPECT_EQ(wc, new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(nullptr, DiceTabHelper::FromWebContents(wc));

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_EQ(entry->GetLocalProfileName(), kWork);
  // The color is not applied if the user enters the SAML flow.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->GetUserColor()
                   .has_value());
}

// TODO(crbug.com/40197099): Extend this test to support lacros.
// Regression test for crash https://crbug.com/1195784.
// Crash requires specific conditions to be reproduced. Browser should have 2
// profiles with the same GAIA account name and the first profile should use
// default local name. This is set up specifically in order to trigger
// ProfileAttributesStorage::NotifyIfProfileNamesHaveChanged() when a new third
// profile is added.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       PRE_ProfileNameChangesOnProfileAdded) {
  Profile* default_profile = browser()->profile();
  AccountInfo default_account_info =
      FinishDiceSignIn(default_profile, "joe@gmail.com", "Joe");
  IdentityManagerFactory::GetForProfile(default_profile)
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(default_account_info.account_id,
                          signin::ConsentLevel::kSync);

  // Create a second profile.
  base::RunLoop run_loop;
  Profile* second_profile = nullptr;
  ProfileManager::CreateMultiProfileAsync(
      u"Joe", /*icon_index=*/0, /*is_hidden=*/false,
      base::BindLambdaForTesting([&](Profile* profile) {
        ASSERT_TRUE(profile);
        second_profile = profile;
        run_loop.Quit();
      }));
  run_loop.Run();
  AccountInfo second_profile_info =
      FinishDiceSignIn(second_profile, "joe.secondary@gmail.com", "Joe");
  IdentityManagerFactory::GetForProfile(second_profile)
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(second_profile_info.account_id,
                          signin::ConsentLevel::kSync);

  // The first profile should use default name.
  ProfileAttributesEntry* default_profile_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(default_profile->GetPath());
  ASSERT_NE(default_profile_entry, nullptr);
  EXPECT_TRUE(default_profile_entry->IsUsingDefaultName());

  ProfileAttributesEntry* second_profile_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(second_profile->GetPath());
  ASSERT_NE(second_profile_entry, nullptr);

  // Both profiles should have matching GAIA name.
  EXPECT_EQ(default_profile_entry->GetGAIANameToDisplay(),
            second_profile_entry->GetGAIANameToDisplay());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       ProfileNameChangesOnProfileAdded) {
  EXPECT_EQ(g_browser_process->profile_manager()->GetNumberOfProfiles(), 2u);

  // This should not crash.
  StartDiceSignIn();
}

// Regression test for https://crbug.com/1467483
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       DiceSigninFailure) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  StartDiceSignIn();

  // Simulate Dice token exchange failure. This should not crash.
  auto process_dice_header_delegate_impl =
      ProcessDiceHeaderDelegateImpl::Create(web_contents());
  process_dice_header_delegate_impl->HandleTokenExchangeFailure(
      "example@gmail.com",
      GoogleServiceAuthError::FromServiceError("SomeError"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenPickerAndClose) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  ProfilePicker::Hide();
  WaitForPickerClosed();
}

// Regression test for https://crbug.com/1205147.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenPickerWhileClosing) {
  // Open the first picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Request to open the second picker window while the first one is still
  // closing.
  ProfilePicker::Hide();
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));

  // The first picker should be closed and the second picker should be
  // displayed.
  WaitForPickerClosedAndReopenedImmediately();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ReShow) {
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Show the picker with a different entry point, the picker is reused.
  base::WeakPtr<views::Widget> widget_weak = widget()->GetWeakPtr();
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
  EXPECT_FALSE(widget_weak->IsClosed());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Show the account selection.
  base::test::TestFuture<const std::string&> account_future_1;
  ProfilePicker::Show(ProfilePicker::Params::ForLacrosSelectAvailableAccount(
      base::FilePath(), account_future_1.GetCallback()));
  // The picker is not reused
  EXPECT_TRUE(widget_weak->IsClosed());
  WaitForPickerClosedAndReopenedImmediately();

  // Show the account selection again.
  base::test::TestFuture<const std::string&> account_future_2;
  EXPECT_FALSE(account_future_1.IsReady());
  widget_weak = widget()->GetWeakPtr();
  ProfilePicker::Show(ProfilePicker::Params::ForLacrosSelectAvailableAccount(
      base::FilePath(), account_future_2.GetCallback()));
  // The picker is reused, and the previous callback is called.
  EXPECT_FALSE(widget_weak->IsClosed());
  EXPECT_TRUE(account_future_1.Get().empty());
  EXPECT_FALSE(account_future_2.IsReady());

  // Hide the picker. The callback is called.
  ProfilePicker::Hide();
  EXPECT_TRUE(widget_weak->IsClosed());
  EXPECT_TRUE(account_future_2.Get().empty());
#endif
}

// TODO(crbug.com/325310963): Re-enable this flaky test on macOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenProfile DISABLED_OpenProfile
#else
#define MAYBE_OpenProfile OpenProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_OpenProfile) {
  base::HistogramTester histogram_tester;

  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(base::Seconds(0));
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the other profile.
  OpenProfileFromPicker(other_path, /*open_settings=*/false);
  // Browser for the profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstNonEmptyPaint(
      GURL("chrome://newtab/"),
      new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_browser->profile()->GetPath(), other_path);
  WaitForPickerClosed();
  // IPH is shown.
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown(new_browser));

  // FirstProfileTime.* histograms aren't recorded because the picker
  // is opened from the menu.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "ProfilePicker.FirstProfileTime."),
              testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenProfileFromStartup) {
  base::HistogramTester histogram_tester;
  ASSERT_FALSE(ProfilePicker::IsOpen());

  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForLoadStop(GURL("chrome://profile-picker"));

  // Open the new profile.
  OpenProfileFromPicker(other_path, /*open_settings=*/false);

  // Measurement of startup performance started.

  // Browser for the profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstNonEmptyPaint(
      GURL("chrome://newtab/"),
      new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_browser->profile()->GetPath(), other_path);
  WaitForPickerClosed();

  histogram_tester.ExpectTotalCount(
      "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint", 1);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FirstProfileTime.FirstWebContentsFinishReason",
      metrics::StartupProfilingFinishReason::kDone, 1);
}

// TODO(crbug.com/40817459) Test is flaky on Linux CFI, Linux dbg, Mac ASan
#if ((BUILDFLAG(CFI_ICALL_CHECK) || !defined(NDEBUG)) && \
     BUILDFLAG(IS_LINUX)) ||                             \
    (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define MAYBE_OpenProfile_Settings DISABLED_OpenProfile_Settings
#else
#define MAYBE_OpenProfile_Settings OpenProfile_Settings
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_OpenProfile_Settings) {
  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(base::Seconds(0));
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the other profile.
  OpenProfileFromPicker(other_path, /*open_settings=*/true);
  // Browser for the profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://settings/manageProfile"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_browser->profile()->GetPath(), other_path);
  WaitForPickerClosed();
  // IPH is not shown.
  EXPECT_FALSE(ProfileSwitchPromoHasBeenShown(new_browser));
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenURL_PickerClosed) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  const GURL kTargetURL("chrome://settings/help");
  // Create a profile.
  base::FilePath profile_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::ForBackgroundManager(kTargetURL));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open the profile.
  OpenProfileFromPicker(profile_path, /*open_settings=*/false);
  // Browser for the profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(kTargetURL,
                  new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_browser->profile()->GetPath(), profile_path);
  WaitForPickerClosed();
}

// Regression test for https://crbug.com/1199035
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       OpenProfile_Guest) {
  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(base::Seconds(0));
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  // Open a Guest profile.
  OpenGuestFromPicker();
  // Browser for the guest profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(new_browser->profile()->IsGuestSession());
  WaitForPickerClosed();
  // IPH is not shown.
  EXPECT_FALSE(ProfileSwitchPromoHasBeenShown(new_browser));
}

// Closes the default browser window before creating a new profile in the
// profile picker.
// Regression test for https://crbug.com/1144092.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CloseBrowserBeforeCreatingNewProfile) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));

  // Close the browser window.
  BrowserList::GetInstance()->CloseAllBrowsersWithProfile(browser()->profile());
  ui_test_utils::WaitForBrowserToClose(browser());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Imitate creating a new profile through the profile picker.
  CreateLocalProfile();

  BrowserAddedWaiter(1u).Wait();
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
  WaitForPickerClosed();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateLocalProfile) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());

  content::TestNavigationObserver profile_customization_observer(
      kLocalProfileCreationUrl);
  profile_customization_observer.StartWatchingNewWebContents();
  BrowserAddedWaiter waiter = BrowserAddedWaiter(2u);

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  // Wait until webUI is fully initialized.
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));

  // Simulate clicking the "Continue without an account" button.
  CreateLocalProfile();

  Browser* new_browser = waiter.Wait();
  profile_customization_observer.Wait();
  content::WebContents* dialog_web_contents =
      new_browser->signin_view_controller()
          ->GetModalDialogWebContentsForTesting();
  EXPECT_EQ(dialog_web_contents->GetLastCommittedURL(),
            kLocalProfileCreationUrl);

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(new_browser->profile()->GetPath());
  ASSERT_TRUE(entry->IsEphemeral());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(new_browser->signin_view_controller()->ShowsModalDialog());

  // Simulate clicking the "Done" button on the profile customization dialog.
  ConfirmLocalProfileCreation(dialog_web_contents);

  ASSERT_FALSE(entry->IsEphemeral());
  ASSERT_EQ(kLocalProfileName, base::UTF16ToUTF8(entry->GetLocalProfileName()));
  ASSERT_EQ(2u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  EXPECT_FALSE(new_browser->signin_view_controller()->ShowsModalDialog());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_CancelLocalProfileCreation DISABLED_CancelLocalProfileCreation
#else
#define MAYBE_CancelLocalProfileCreation CancelLocalProfileCreation
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       MAYBE_CancelLocalProfileCreation) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());

  content::TestNavigationObserver profile_customization_observer(
      kLocalProfileCreationUrl);
  profile_customization_observer.StartWatchingNewWebContents();
  BrowserAddedWaiter browser_added_waiter = BrowserAddedWaiter(2u);

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  // Wait until webUI is fully initialized.
  WaitForLoadStop(GURL("chrome://profile-picker/new-profile"));

  // Simulate clicking the "Continue without an account" button.
  CreateLocalProfile();

  Browser* new_browser = browser_added_waiter.Wait();
  profile_customization_observer.Wait();
  content::WebContents* dialog_web_contents =
      new_browser->signin_view_controller()
          ->GetModalDialogWebContentsForTesting();
  EXPECT_EQ(dialog_web_contents->GetLastCommittedURL(),
            kLocalProfileCreationUrl);

  base::FilePath profile_path = new_browser->profile()->GetPath();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  ASSERT_EQ(2u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  ASSERT_TRUE(entry->IsEphemeral());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_TRUE(new_browser->signin_view_controller()->ShowsModalDialog());

  // Simulate clicking the "Delete profile" button on the profile customization
  // dialog.
  ProfileDeletionObserver observer;
  DeleteLocalProfile(dialog_web_contents);
  observer.Wait();

  ASSERT_EQ(1u, g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetNumberOfProfiles());
  ASSERT_EQ(nullptr, g_browser_process->profile_manager()
                         ->GetProfileAttributesStorage()
                         .GetProfileAttributesWithPath(profile_path));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, DeleteProfile) {
  // Create a second profile.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(other_path);
  ASSERT_TRUE(profile);
  // Open the picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  WaitForLoadStop(GURL("chrome://profile-picker"));
  ProfilePickerHandler* handler = profile_picker_handler();

  // Simulate profile deletion from the picker.
  ProfileDestructionWaiter waiter(profile);
  base::Value::List args;
  args.Append(base::FilePathToValue(other_path));
  handler->HandleGetProfileStatistics(args);
  handler->HandleRemoveProfile(args);
  waiter.Wait();
}

// Regression test for https://crbug.com/1488267
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       DeleteProfileFromOwnTab) {
  // Create a new profile and browser. This is required on Lacros because the
  // main profile cannot be deleted.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath other_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, other_path);
  Browser* other_browser = CreateBrowser(&other_profile);

  // Open the picker in a tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      other_browser, GURL(chrome::kChromeUIProfilePickerUrl)));
  content::WebContents* contents =
      other_browser->tab_strip_model()->GetActiveWebContents();
  ProfilePickerHandler* handler = contents->GetWebUI()
                                      ->GetController()
                                      ->GetAs<ProfilePickerUI>()
                                      ->GetProfilePickerHandlerForTesting();

  // Simulate profile deletion from the picker.
  ProfileDestructionWaiter waiter(&other_profile);
  base::Value::List args;
  args.Append(base::FilePathToValue(other_profile.GetPath()));
  handler->HandleGetProfileStatistics(args);
  handler->HandleRemoveProfile(args);
  waiter.Wait();
}

class SupervisedProfilePickerHideGuestModeTest
    : public ProfilePickerCreationFlowBrowserTest,
      public testing::WithParamInterface<
          /*HideGuestModeForSupervisedUsers=*/bool> {
 public:
  SupervisedProfilePickerHideGuestModeTest() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitWithFeatureState(
        supervised_user::kHideGuestModeForSupervisedUsers,
        HideGuestModeForSupervisedUsersEnabled());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  }

  static bool HideGuestModeForSupervisedUsersEnabled() { return GetParam(); }

  void OpenProfilePicker() {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
    WaitForLoadStop(GURL("chrome://profile-picker"));
  }

  void RemoveProfile(Profile* profile) {
    ProfileDestructionWaiter waiter(profile);
    webui::DeleteProfileAtPath(profile->GetPath(),
                               ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
    waiter.Wait();
  }

  bool IsGuestModeButtonHidden() {
    return content::EvalJs(
               web_contents(),
               base::StrCat({"getComputedStyle(", kBrowseAsGuestButtonPath,
                             ").display == \"none\" "}))
        .ExtractBool();
  }

  ::testing::AssertionResult ClickGuestModeButton() {
    return content::ExecJs(web_contents(),
                           base::StrCat({kBrowseAsGuestButtonPath, ".click"}));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  static constexpr char kBrowseAsGuestButtonPath[] =
      "document.body.getElementsByTagName('"
      "profile-picker-app')[0].shadowRoot."
      "getElementById('mainView').shadowRoot.getElementById("
      "\'browseAsGuestButton\')";
};

IN_PROC_BROWSER_TEST_P(SupervisedProfilePickerHideGuestModeTest,
                       DeleteSupervisedProfile) {
  Profile* default_profile = browser()->profile();
  AccountInfo default_account_info =
      FinishDiceSignIn(default_profile, "adult@gmail.com", "Adult");

  // Create a second supervised profile and signin.
  base::FilePath child_path = CreateNewProfileWithoutBrowser();
  Profile* child_profile =
      g_browser_process->profile_manager()->GetProfileByPath(child_path);
  supervised_user::EnableParentalControls(*child_profile->GetPrefs());

  AccountInfo child_account_info =
      FinishDiceSignIn(child_profile, "child@gmail.com", "child");
  ASSERT_TRUE(child_profile);

  OpenProfilePicker();

  // Guest Mode button will be unavailable when a supervised user is added, and
  // kHideGuestModeForSupervisedUsers is enabled.
  EXPECT_EQ(IsGuestModeButtonHidden(),
            HideGuestModeForSupervisedUsersEnabled());

  RemoveProfile(child_profile);

  // Guest Mode button will be available when a supervised user is removed.
  EXPECT_FALSE(IsGuestModeButtonHidden());
  EXPECT_TRUE(ClickGuestModeButton());
}

IN_PROC_BROWSER_TEST_P(SupervisedProfilePickerHideGuestModeTest,
                       DeleteLastSupervisedProfile) {
  base::FilePath child_path = CreateNewProfileWithoutBrowser();
  Profile* child_profile =
      g_browser_process->profile_manager()->GetProfileByPath(child_path);
  supervised_user::EnableParentalControls(*child_profile->GetPrefs());

  ASSERT_TRUE(child_profile);

  OpenProfilePicker();

  EXPECT_EQ(IsGuestModeButtonHidden(),
            HideGuestModeForSupervisedUsersEnabled());

  RemoveProfile(child_profile);

  EXPECT_FALSE(IsGuestModeButtonHidden());
  EXPECT_TRUE(ClickGuestModeButton());
}

IN_PROC_BROWSER_TEST_P(SupervisedProfilePickerHideGuestModeTest,
                       RegularProfile_GuestModeAvailable) {
  Profile* default_profile = browser()->profile();
  AccountInfo default_account_info =
      FinishDiceSignIn(default_profile, "adult@gmail.com", "Adult");

  // Open the picker.
  OpenProfilePicker();

  // Guest Mode button is available when kHideGuestModeForSupervisedUsers is
  // enabled and disabled.
  EXPECT_FALSE(IsGuestModeButtonHidden());
  EXPECT_TRUE(ClickGuestModeButton());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupervisedProfilePickerHideGuestModeTest,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
                         testing::Bool(),
#else
      testing::Values(false),
#endif
                         [](const auto& info) {
                           return info.param ? "WithHideGuestModeEnabled"
                                             : "WithHideGuestModeDisabled";
                         });

class ProfilePickerEnterpriseCreationFlowBrowserTest
    : public ProfilePickerCreationFlowBrowserTest {
 public:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    ProfilePickerCreationFlowBrowserTest::OnWillCreateBrowserContextServices(
        context);
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &policy::FakeUserPolicySigninService::BuildForEnterprise));
  }
};

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfile) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in managed user notice screen getting displayed.
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  // Enterprise domain needed for this profile being detected as Work.
  Profile* profile_being_created =
      SignInForNewProfile(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                          "joe.enterprise@gmail.com", "Joe", "enterprise.com");

  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      /*choice=*/signin::SIGNIN_CHOICE_NEW_PROFILE);

  WaitForLoadStop(GetSyncConfirmationURL());
  // Simulate finishing the flow with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  WaitForPickerClosed();

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_NE(entry->GetGAIAId(), std::string());
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"enterprise.com");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileWithSuggestedTwoFactorAuthSetup) {
  const GURL kTwoFactorIntersitialUrl(
      "https://myaccount.google.com/interstitials/twosvrequired?query=value");

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  Profile* profile_being_created = StartDiceSignIn();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithAccessPoint(
              signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER)
          .Build("joe.acme@gmail.com"));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  signin::UpdateAccountInfoForAccount(
      identity_manager,
      /*account_info=*/FillAccountInfo(core_account_info, "Joe", "acme.com"));
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      core_account_info.account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);

  // Redirect the web contents to a the two factor intersitial authentication
  // page.
  web_contents()->GetController().LoadURL(
      kTwoFactorIntersitialUrl, content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  WaitForLoadStop(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));
  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      /*choice=*/signin::SIGNIN_CHOICE_NEW_PROFILE);

  WaitForLoadStop(GetSyncConfirmationURL());
  // Simulate finishing the flow with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  WaitForPickerClosed();
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(kTwoFactorIntersitialUrl,
                  new_browser->tab_strip_model()->GetActiveWebContents());

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_NE(entry->GetGAIAId(), std::string());
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"acme.com");

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_FALSE(sync_service->HasSyncConsent());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}

// TODO(crbug.com/40197102): Extend this test to support mirror.
IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileWithSyncDisabled) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartDiceSignIn();

  // Set the device as managed in prefs.
  profile_being_created->GetPrefs()->SetBoolean(
      syncer::prefs::internal::kSyncManaged, true);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);

  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  // Enterprise domain needed for this profile being detected as Work.
  FinishDiceSignIn(profile_being_created, "joe.enterprise@gmail.com", "Joe",
                   "enterprise.com");

  // Wait for the sign-in to propagate to the flow, resulting in managed user
  // notice screen getting displayed.
  WaitForLoadStop(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));

  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncDisabled,
      /*choice=*/signin::SIGNIN_CHOICE_NEW_PROFILE);

  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  WaitForPickerClosed();

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"enterprise.com");

  // Sync is disabled.
  EXPECT_NE(entry->GetGAIAId(), std::string());
  EXPECT_FALSE(sync_service->IsSyncFeatureEnabled());
  EXPECT_EQ(
      ThemeServiceFactory::GetForProfile(profile_being_created)->GetUserColor(),
      kProfileColor);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// TODO(crbug.com/40817459) Test is flaky on Linux CFI
// TODO(crbug.com/40885685) Test is also flaky on Linux (dbg)
#if BUILDFLAG(IS_LINUX)
#define MAYBE_CreateSignedInEnterpriseProfileSettings \
  DISABLED_CreateSignedInEnterpriseProfileSettings
#else
#define MAYBE_CreateSignedInEnterpriseProfileSettings \
  CreateSignedInEnterpriseProfileSettings
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       MAYBE_CreateSignedInEnterpriseProfileSettings) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in managed user notice screen getting displayed.
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  // Enterprise domain needed for this profile being detected as Work.
  Profile* profile_being_created =
      SignInForNewProfile(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                          "joe.enterprise@gmail.com", "Joe", "enterprise.com");

  // Wait for the sign-in to propagate to the flow, resulting in managed user
  // notice screen getting displayed.
  WaitForLoadStop(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));

  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      /*choice=*/signin::SIGNIN_CHOICE_NEW_PROFILE);

  WaitForLoadStop(GetSyncConfirmationURL());
  // Simulate finishing the flow with "Configure sync".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);

  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://settings/syncSetup"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  WaitForPickerClosed();

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_NE(entry->GetGAIAId(), std::string());
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), u"enterprise.com");

  // Sync is getting configured.
  EXPECT_TRUE(entry->IsAuthenticated());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_being_created);
  EXPECT_TRUE(sync_service->HasSyncConsent());
  EXPECT_FALSE(
      sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  // The color is not applied if the user enters settings.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->GetUserColor()
                   .has_value());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest, Cancel) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in managed user notice screen getting displayed.
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  // Enterprise domain needed for this profile being detected as Work.
  Profile* profile_being_created =
      SignInForNewProfile(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                          "joe.enterprise@gmail.com", "Joe", "enterprise.com");
  base::FilePath profile_being_created_path = profile_being_created->GetPath();

  // Wait for the sign-in to propagate to the flow, resulting in managed user
  // notice screen getting displayed.
  WaitForLoadStop(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));

  ProfileDeletionObserver observer;
  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      /*choice=*/signin::SIGNIN_CHOICE_CANCEL);

  // As the profile creation flow was opened directly, the window is closed now.
  WaitForPickerClosed();
  observer.Wait();

  // The profile entry is deleted
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created_path);
  EXPECT_EQ(entry, nullptr);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CancelFromPicker) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in managed user notice screen getting displayed.
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  // Enterprise domain needed for this profile being detected as Work.
  Profile* profile_being_created =
      SignInForNewProfile(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl),
                          "joe.enterprise@gmail.com", "Joe", "enterprise.com",
                          /*start_on_management_page=*/true);
  base::FilePath profile_being_created_path = profile_being_created->GetPath();

  // Wait for the sign-in to propagate to the flow, resulting in managed user
  // notice screen getting displayed.
  WaitForLoadStop(GURL(chrome::kChromeUIManagedUserProfileNoticeUrl));

  ProfileDeletionObserver observer;
  profiles::testing::ExpectPickerManagedUserNoticeScreenTypeAndProceed(
      /*expected_type=*/
      ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled,
      /*choice=*/signin::SIGNIN_CHOICE_CANCEL);

  // As the management page was opened, the picker returns to it.
  WaitForLoadStop(GURL("chrome://profile-picker"));
  observer.Wait();

  // The profile entry is deleted
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created_path);
  EXPECT_EQ(entry, nullptr);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileSigninAlreadyExists_ConfirmSwitch) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Create a pre-existing profile syncing with the same account as the profile
  // being created.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* other_entry =
      storage.GetProfileAttributesWithPath(other_path);
  ASSERT_NE(other_entry, nullptr);
  // Fake sync is enabled in this profile with Joe's account.
  other_entry->SetAuthInfo(kGaiaId, u"joe.consumer@gmail.com",
                           /*is_consented_primary_account=*/true);
  other_entry->SetGaiaIds({kGaiaId});
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Add the account to the OS account manager.
  account_manager::AccountKey kAccountKey{kGaiaId,
                                          account_manager::AccountType::kGaia};
  auto* account_manager = MaybeGetAshAccountManagerForTests();
  DCHECK(account_manager);
  account_manager->UpsertAccount(kAccountKey, "joe.consumer@gmail.com",
                                 "access_token");
#endif
  size_t initial_profile_count = g_browser_process->profile_manager()
                                     ->GetProfileAttributesStorage()
                                     .GetNumberOfProfiles();

  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in profile switch screen getting displayed (in between,
  // chrome://sync-confirmation/loading gets displayed but that page may not
  // finish loading and anyway is not so relevant).
  Profile* contents_profile =
      SignInForNewProfile(GURL("chrome://profile-picker/profile-switch"),
                          "joe.consumer@gmail.com", "Joe");
  base::FilePath contents_profile_path = contents_profile->GetPath();
  EXPECT_EQ(ProfilePicker::GetSwitchProfilePath(), other_path);

  // Simulate clicking on the confirm switch button.
  ProfilePickerHandler* handler = profile_picker_handler();
  base::Value::List args;
  args.Append(base::FilePathToValue(other_path));
  handler->HandleConfirmProfileSwitch(args);

  // Browser for a pre-existing profile is displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_browser->profile()->GetPath(), other_path);

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, the "profile switch" interstitial is rendered in the system
  // profile.
  EXPECT_TRUE(contents_profile->IsSystemProfile());
#else
  EXPECT_NE(contents_profile_path, ProfileManager::GetSystemProfilePath());
  // Profile should be already deleted.
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(contents_profile_path);
  EXPECT_EQ(entry, nullptr);
#endif
  EXPECT_EQ(initial_profile_count, g_browser_process->profile_manager()
                                       ->GetProfileAttributesStorage()
                                       .GetNumberOfProfiles());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileSigninAlreadyExists_CancelSwitch) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Create a pre-existing profile syncing with the same account as the profile
  // being created.
  base::FilePath other_path = CreateNewProfileWithoutBrowser();
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* other_entry =
      storage.GetProfileAttributesWithPath(other_path);
  ASSERT_NE(other_entry, nullptr);
  // Fake sync is enabled in this profile with Joe's account.
  other_entry->SetAuthInfo(kGaiaId, u"joe.consumer@gmail.com",
                           /*is_consented_primary_account=*/true);
  other_entry->SetGaiaIds({kGaiaId});
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Add the account to the OS account manager.
  account_manager::AccountKey kAccountKey{kGaiaId,
                                          account_manager::AccountType::kGaia};
  auto* account_manager = MaybeGetAshAccountManagerForTests();
  DCHECK(account_manager);
  account_manager->UpsertAccount(kAccountKey, "joe.consumer@gmail.com",
                                 "access_token");
#endif
  size_t initial_profile_count = g_browser_process->profile_manager()
                                     ->GetProfileAttributesStorage()
                                     .GetNumberOfProfiles();

  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in profile switch screen getting displayed (in between,
  // chrome://sync-confirmation/loading gets displayed but that page may not
  // finish loading and anyway is not so relevant).
  Profile* contents_profile =
      SignInForNewProfile(GURL("chrome://profile-picker/profile-switch"),
                          "joe.consumer@gmail.com", "Joe");
  base::FilePath contents_profile_path = contents_profile->GetPath();

  // The profile switch screen should be displayed
  EXPECT_EQ(ProfilePicker::GetSwitchProfilePath(), other_path);

  // Simulate clicking on the cancel button.
  ProfileDeletionObserver observer;
  ProfilePickerHandler* handler = profile_picker_handler();
  base::Value::List args;
  handler->HandleCancelProfileSwitch(args);

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();
  observer.Wait();

  // Only one browser should be displayed.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, the "profile switch" interstitial is rendered in the system
  // profile.
  EXPECT_TRUE(contents_profile->IsSystemProfile());
#else
  EXPECT_FALSE(contents_profile->IsSystemProfile());
  // The sign-in profile should be marked for deletion.
  IsProfileDirectoryMarkedForDeletion(contents_profile_path);
#endif
  EXPECT_EQ(initial_profile_count, g_browser_process->profile_manager()
                                       ->GetProfileAttributesStorage()
                                       .GetNumberOfProfiles());
}

class ProfilePickerCreationFlowEphemeralProfileBrowserTest
    : public ProfilePickerCreationFlowBrowserTest,
      public testing::WithParamInterface<ForceEphemeralProfilesPolicy> {
 public:
  ProfilePickerCreationFlowEphemeralProfileBrowserTest() = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (GetForceEphemeralProfilesPolicy() !=
        ForceEphemeralProfilesPolicy::kUnset) {
      GTEST_SKIP() << "Lacros does not support ephemeral profiles policy";
    }
#endif

    ProfilePickerCreationFlowBrowserTest::SetUp();
  }

  ForceEphemeralProfilesPolicy GetForceEphemeralProfilesPolicy() const {
    return GetParam();
  }

  bool AreEphemeralProfilesForced() const {
    return GetForceEphemeralProfilesPolicy() ==
           ForceEphemeralProfilesPolicy::kEnabled;
  }

  // Check that the policy was correctly applied to the preference.
  void CheckPolicyApplied(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    DCHECK_EQ(GetForceEphemeralProfilesPolicy(),
              ForceEphemeralProfilesPolicy::kUnset);
#else
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles),
              AreEphemeralProfilesForced());
#endif
  }

  static ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }

  // Checks if a profile matching `name` exists in the profile manager.
  bool ProfileWithNameExists(const std::u16string& name) {
    for (const auto* entry : profile_manager()
                                 ->GetProfileAttributesStorage()
                                 .GetAllProfilesAttributes()) {
      if (entry->GetLocalProfileName() == name)
        return true;
    }
    return false;
  }

  // Checks if the original profile (the initial profile existing at the start
  // of the test) exists in the profile manager.
  bool OriginalProfileExists() {
    return ProfileWithNameExists(kOriginalProfileName);
  }

  void SetUpInProcessBrowserTestFixture() override {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    ForceEphemeralProfilesPolicy policy = GetForceEphemeralProfilesPolicy();

    if (policy != ForceEphemeralProfilesPolicy::kUnset) {
      policy::PolicyMap policy_map;
      policy_map.Set(
          policy::key::kForceEphemeralProfiles, policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
          base::Value(policy == ForceEphemeralProfilesPolicy::kEnabled),
          nullptr);
      policy_provider_.UpdateChromePolicy(policy_map);

      policy_provider_.SetDefaultReturns(
          /*is_initialization_complete_return=*/true,
          /*is_first_policy_load_complete_return=*/true);
      policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
          &policy_provider_);
    }
#endif
    ProfilePickerCreationFlowBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ProfilePickerCreationFlowBrowserTest::SetUpOnMainThread();
    if (GetTestPreCount() == 1) {
      // Only called in "PRE_" tests, to set a name to the starting profile.
      ProfileAttributesEntry* entry =
          profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(browser()->profile()->GetPath());
      ASSERT_NE(entry, nullptr);
      entry->SetLocalProfileName(kOriginalProfileName,
                                 entry->IsUsingDefaultName());
    }
    CheckPolicyApplied(browser()->profile());
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Flaky on Windows: https://crbug.com/1247530.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_Signin DISABLED_PRE_Signin
#define MAYBE_Signin DISABLED_Signin
#else
#define MAYBE_PRE_Signin PRE_Signin
#define MAYBE_Signin Signin
#endif
// Checks that the new profile is no longer ephemeral at the end of the flow and
// still exists after restart.
IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       MAYBE_PRE_Signin) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  ASSERT_TRUE(OriginalProfileExists());

  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  Profile* profile_being_created = SignInForNewProfile(
      GetSyncConfirmationURL(), "joe.consumer@gmail.com", "Joe");

  // Check that the profile is ephemeral, regardless of the policy.
  ProfileAttributesEntry* entry =
      profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_TRUE(entry->IsOmitted());

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForLoadStop(GURL("chrome://newtab/"),
                  new_browser->tab_strip_model()->GetActiveWebContents());

  WaitForPickerClosed();
  EXPECT_EQ(2u, profile_manager()->GetNumberOfProfiles());
  EXPECT_EQ(entry->GetLocalProfileName(), u"Joe");
  // The profile is no longer ephemeral, unless the policy is enabled.
  EXPECT_EQ(entry->IsEphemeral(), AreEphemeralProfilesForced());
  EXPECT_FALSE(entry->IsOmitted());
  // The preference is consistent with the policy.
  CheckPolicyApplied(profile_being_created);
}

IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       MAYBE_Signin) {
  if (AreEphemeralProfilesForced()) {
    // If the policy is set, all profiles should have been deleted.
    EXPECT_EQ(1u, profile_manager()->GetNumberOfProfiles());
    // The current profile is not the one that was created in the previous run.
    EXPECT_FALSE(ProfileWithNameExists(u"Joe"));
    EXPECT_FALSE(OriginalProfileExists());
    return;
  }

  // If the policy is disabled or unset, the two profiles are still here.
  EXPECT_EQ(2u, profile_manager()->GetNumberOfProfiles());
  EXPECT_TRUE(ProfileWithNameExists(u"Joe"));
  EXPECT_TRUE(OriginalProfileExists());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Flaky on Windows: https://crbug.com/1247530.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PRE_ExitDuringSignin DISABLED_PRE_ExitDuringSignin
#define MAYBE_ExitDuringSignin DISABLED_ExitDuringSignin
#else
#define MAYBE_PRE_ExitDuringSignin PRE_ExitDuringSignin
#define MAYBE_ExitDuringSignin ExitDuringSignin
#endif
// Checks that the new profile is deleted on next startup if Chrome exits during
// the signin flow.
IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       MAYBE_PRE_ExitDuringSignin) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  ASSERT_TRUE(OriginalProfileExists());
  Profile* profile_being_created = StartDiceSignIn();

  // Check that the profile is ephemeral, regardless of the policy.
  ProfileAttributesEntry* entry =
      profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_TRUE(entry->IsOmitted());
  // Exit Chrome while still in the signin flow.
}

IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       MAYBE_ExitDuringSignin) {
  // The profile was deleted, regardless of the policy.
  EXPECT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  // The other profile still exists.
  EXPECT_NE(AreEphemeralProfilesForced(), OriginalProfileExists());
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfilePickerCreationFlowEphemeralProfileBrowserTest,
    testing::Values(ForceEphemeralProfilesPolicy::kUnset,
                    ForceEphemeralProfilesPolicy::kDisabled,
                    ForceEphemeralProfilesPolicy::kEnabled));

// Only MacOS has a keyboard shortcut to exit Chrome.
#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       SyncConfirmationExitChromeTest) {
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  SignInForNewProfile(GetSyncConfirmationURL(), "joe.consumer@gmail.com",
                      "Joe");
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Exit the sync confirmation view (Cmd-Q).
  view()->AcceleratorPressed(ui::Accelerator(ui::VKEY_Q, ui::EF_COMMAND_DOWN));
  WaitForPickerClosed();
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}
#endif

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       SyncConfirmationNavigateBackTest) {
  // Simulate a successful sign-in and wait for the sign-in to propagate to the
  // flow, resulting in sync confirmation screen getting displayed.
  SignInForNewProfile(GetSyncConfirmationURL(), "joe.consumer@gmail.com",
                      "Joe");
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Navigate back does nothing.
  SimulateNavigateBack();

  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)

class ProfilePickerLacrosFirstRunBrowserTestBase
    : public ProfilePickerTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ProfilePickerTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProfilePickerLacrosFirstRunBrowserTestBase::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto sync_service = std::make_unique<syncer::TestSyncService>();

          // The FRE will be paused, waiting for the state to change
          // before either showing or exiting it.
          // `GoThroughFirstRunFlow()` will do this, or the test
          // should call `sync_service()` to do this manually.
          sync_service->SetMaxTransportState(
              syncer::SyncService::TransportState::INITIALIZING);

          return sync_service;
        }));
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    ProfilePickerTestBase::SetUpDefaultCommandLine(command_line);

    if (GetTestPreCount() <= 1) {
      // Show the FRE in these tests. We only disable the FRE for PRE_PRE_ tests
      // (with GetTestPreCount() == 2) as we need the general set up to run
      // and finish registering a signed in account with the primary profile. It
      // will then be available to the subsequent steps of the test.
      // TODO(crbug.com/40833358): Find a simpler way to set this up.
      command_line->RemoveSwitch(switches::kNoFirstRun);
    }
  }

  // Helper to obtain the primary profile from the `ProfileManager` instead of
  // going through the `Browser`, which we don't open in many tests here.
  Profile* GetPrimaryProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    return profile_manager->GetProfile(
        profile_manager->GetPrimaryUserProfilePath());
  }

  // Helper to walk through the FRE. Performs a few assertions, and performs the
  // specified choices when prompted.
  void GoThroughFirstRunFlow(bool quit_on_welcome,
                             std::optional<bool> quit_on_sync) {
    Profile* profile = GetPrimaryProfile();
    EXPECT_TRUE(ShouldOpenFirstRun(profile));

    // The profile picker should be open on start to show the FRE.
    EXPECT_EQ(0u, BrowserList::GetInstance()->size());
    EXPECT_TRUE(ProfilePicker::IsOpen());

    // Unblock the sync service.
    sync_service()->SetMaxTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service()->FireStateChanged();

    // A welcome page should be displayed.
    WaitForPickerWidgetCreated();

    WaitForLoadStop(GURL(chrome::kChromeUIIntroURL));
    content::WebContents* contents = web_contents();
    EXPECT_TRUE(contents);
    base::OnceClosure complete_welcome =
        base::BindLambdaForTesting([contents]() {
          contents->GetWebUI()->ProcessWebUIMessage(
              contents->GetURL(), "continueWithAccount", base::Value::List());
        });

    if (quit_on_welcome) {
      // Do nothing for now, we will exit the flow below.
      ASSERT_FALSE(quit_on_sync.has_value());
    } else {
      // Proceed to the sync confirmation page.
      std::move(complete_welcome).Run();
      WaitForLoadStop(GetSyncConfirmationURL());
    }
    if (quit_on_welcome || quit_on_sync.value()) {
      // Exit the flow.
      ProfilePicker::Hide();
    } else {
      // Opt-in to sync.
      LoginUIServiceFactory::GetForProfile(profile)->SyncConfirmationUIClosed(
          LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    }

    // Wait for the FRE closure to settle before the caller can proceed with
    // assertions.
    WaitForPickerClosed();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetPrimaryProfile()));
  }

 private:
  // Start tracking the logged histograms from the beginning, since the FRE can
  // be triggered and completed before we enter the test body.
  base::HistogramTester histogram_tester_;

  base::CallbackListSubscription create_services_subscription_;

  // Lifts the timeout to make sure it is not hiding errors where we don't get
  // the signal that the sync service started.
  // TODO(crbug.com/40839518): Find a better way to safely work around
  // the sync service stalling issue.
  testing::ScopedSyncStartupTimeoutOverride sync_startup_timeout_{
      std::optional<base::TimeDelta>()};
};

class ProfilePickerLacrosFirstRunBrowserTest
    : public ProfilePickerLacrosFirstRunBrowserTestBase {
 private:
  profiles::testing::ScopedNonEnterpriseDomainSetterForTesting
      non_enterprise_domain_setter_;
};

// Overall sequence for QuitEarly:
// Start browser => Show FRE => Quit on welcome step.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest,
                       PRE_PRE_QuitEarly) {}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, PRE_QuitEarly) {
  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/true,
      /*quit_on_sync=*/std::nullopt);

  // No browser window should open because we closed the FRE UI early.
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(ShouldOpenFirstRun(GetPrimaryProfile()));
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, QuitEarly) {
  // On the second run, the FRE is still not marked finished and we should
  // reopen it.
  EXPECT_TRUE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

// Overall sequence for QuitAtEnd:
// Start browser => Show FRE => Advance to sync consent step => Quit.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest,
                       PRE_PRE_QuitAtEnd) {
  // Dummy case to set up the primary profile.
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, PRE_QuitAtEnd) {
  Profile* profile = GetPrimaryProfile();

  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/false,
      /*quit_on_sync=*/true);

  // Because we quit, we should also quit chrome, but mark the FRE finished.
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile));
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, QuitAtEnd) {
  Profile* profile = GetPrimaryProfile();

  // On the second run, the FRE is marked finished and we should skip it.
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

// Overall sequence for OptIn:
// Start browser => Show FRE => Advance to sync consent step => Opt-in.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, PRE_PRE_OptIn) {}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, PRE_OptIn) {
  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/false,
      /*quit_on_sync=*/false);

  // A browser should open.
  EXPECT_FALSE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosFirstRunBrowserTest, OptIn) {
  // On the second run, the FRE is marked finished and we should skip it.
  EXPECT_FALSE(ShouldOpenFirstRun(GetPrimaryProfile()));
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

class ManagedProfileSetUpHelper : public ChromeBrowserMainExtraParts {
 public:
  void PostProfileInit(Profile* profile, bool is_initial_profile) override {
    // Only one profile, the primary one, should be initialized in this test.
    EXPECT_EQ(
        profile->GetPath(),
        g_browser_process->profile_manager()->GetPrimaryUserProfilePath());
    profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  }
};

class ProfilePickerLacrosManagedFirstRunBrowserTest
    : public ProfilePickerLacrosFirstRunBrowserTestBase {
 public:
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    ProfilePickerLacrosFirstRunBrowserTestBase::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ManagedProfileSetUpHelper>());
  }

  const base::UserActionTester& user_action_tester() {
    return user_action_tester_;
  }

 private:
  base::UserActionTester user_action_tester_;
};

// Overall sequence for QuitEarly:
// Start browser => Show FRE => Quit on welcome step.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_PRE_QuitEarly) {}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_QuitEarly) {
  Profile* profile = GetPrimaryProfile();
  // TODO(crbug.com/40224163): This is a bug, the flag should not be set.
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Signin_EnterpriseAccountPrompt_ImportData"));

  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/true,
      /*quit_on_sync=*/std::nullopt);

  // No browser window should open because we closed the FRE UI early.
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  EXPECT_TRUE(ShouldOpenFirstRun(profile));
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       QuitEarly) {
  Profile* profile = GetPrimaryProfile();

  // TODO(crbug.com/40224163): This is a bug, the flag should not be set.
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_EQ(0, user_action_tester().GetActionCount(
                   "Signin_EnterpriseAccountPrompt_ImportData"));

  // On the second run, the FRE is still not marked finished and we should
  // reopen it.
  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/true,
      /*quit_on_sync=*/std::nullopt);
}

// Overall sequence for QuitAtEnd:
// Start browser => Show FRE => Advance to sync consent step => Quit.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_PRE_QuitAtEnd) {}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_QuitAtEnd) {
  Profile* profile = GetPrimaryProfile();
  // TODO(crbug.com/40224163): This is a bug, the flag is set too early
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Signin_EnterpriseAccountPrompt_ImportData"));

  GoThroughFirstRunFlow(
      /*quit_on_welcome=*/false,
      /*quit_on_sync=*/true);

  // The user went past the welcome step, management should be marked accepted.
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Signin_EnterpriseAccountPrompt_ImportData"));

  // No browser window should open because we closed the FRE UI early.
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       QuitAtEnd) {
  Profile* profile = GetPrimaryProfile();

  // On the second run, the FRE is marked finished and we should skip it.
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

// Overall sequence for SyncDisabled:
// Start browser => FRE Skipped => Browser opens.
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_PRE_SyncDisabled) {}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       PRE_SyncDisabled) {
  Profile* profile = GetPrimaryProfile();

  // The profile picker is created but is waiting for the
  // sync service to complete its initialization to
  // determine whether to show the FRE or not.
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_EQ(1, user_action_tester().GetActionCount(
                   "Signin_EnterpriseAccountPrompt_ImportData"));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  EXPECT_EQ(ProfilePickerView::State::kInitializing,
            view()->state_for_testing());

  // Unblock the sync service and simulate the server-side
  // being disabled.
  sync_service()->SetAllowedByEnterprisePolicy(false);
  sync_service()->FireStateChanged();

  // The pending state should resolve by skipping the FRE.
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  WaitForPickerClosed();
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}
IN_PROC_BROWSER_TEST_F(ProfilePickerLacrosManagedFirstRunBrowserTest,
                       SyncDisabled) {
  Profile* profile = GetPrimaryProfile();

  // On the second run, the FRE is marked finished and we should skip it.
  EXPECT_FALSE(ShouldOpenFirstRun(profile));
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(profile));
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(1u, BrowserList::GetInstance()->size());
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
