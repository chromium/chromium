// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace {

// State of the the ForceEphemeralProfiles policy.
enum class ForceEphemeralProfilesPolicy { kUnset, kEnabled, kDisabled };

const SkColor kProfileColor = SK_ColorRED;
const char kWork[] = "Work";
const char kOriginalProfileName[] = "OriginalProfile";

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
  account_info.is_child_account = false;
  return account_info;
}

class BrowserAddedWaiter : public BrowserListObserver {
 public:
  explicit BrowserAddedWaiter(size_t total_count) : total_count_(total_count) {
    BrowserList::AddObserver(this);
  }
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
  Browser* browser_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedWaiter);
};

// Fake user policy signin service immediately invoking the callbacks.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<FakeUserPolicySigninService>(
        profile, IdentityManagerFactory::GetForProfile(profile), std::string(),
        std::string());
  }

  static std::unique_ptr<KeyedService> BuildForEnterprise(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    // Non-empty dm token & client id means enterprise account.
    return std::make_unique<FakeUserPolicySigninService>(
        profile, IdentityManagerFactory::GetForProfile(profile), "foo", "bar");
  }

  FakeUserPolicySigninService(Profile* profile,
                              signin::IdentityManager* identity_manager,
                              const std::string& dm_token,
                              const std::string& client_id)
      : UserPolicySigninService(profile,
                                nullptr,
                                nullptr,
                                nullptr,
                                identity_manager,
                                nullptr),
        dm_token_(dm_token),
        client_id_(client_id) {}

  // policy::UserPolicySigninService:
  void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback) override {
    std::move(callback).Run(dm_token_, client_id_);
  }

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override {
    std::move(callback).Run(true);
  }

 private:
  std::string dm_token_;
  std::string client_id_;
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

  void ShowProfileSigninConfirmation(
      Browser* browser,
      const std::string& username,
      bool prompt_for_new_profile,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate)
      override {
    delegate->OnContinueSignin();
    run_loop_->Quit();
  }

  void ShowManagePasswordsBubble(bool user_action) override {}
  void HideManagePasswordsBubble() override {}

 private:
  content::WebContents* contents_;
  base::RunLoop* run_loop_;
};

class ProfilePickerCreationFlowBrowserTest : public ProfilePickerTestBase {
 public:
  ProfilePickerCreationFlowBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSignInProfileCreation);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ProfilePickerTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ProfilePickerCreationFlowBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&FakeUserPolicySigninService::Build));
  }

  // Opens the Gaia signin page in the profile creation flow. Returns the new
  // profile that was created.
  Profile* StartSigninFlow() {
    ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);
    WaitForLayoutWithoutToolbar();

    // Simulate a click on the signin button.
    base::MockCallback<base::OnceCallback<void(bool)>> switch_finished_callback;
    EXPECT_CALL(switch_finished_callback, Run(true));
    ProfilePicker::SwitchToSignIn(kProfileColor,
                                  switch_finished_callback.Get());

    // The DICE navigation happens in a new web contents (for the profile being
    // created), wait for it.
    WaitForLayoutWithToolbar();
    WaitForFirstPaint(web_contents(),
                      GaiaUrls::GetInstance()->signin_chrome_sync_dice());
    return static_cast<Profile*>(web_contents()->GetBrowserContext());
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList feature_list_;

  // The sync service and waits for policies to load before starting for
  // enterprise users, managed devices and browsers. This means that services
  // depending on it might have to wait too. By setting the management
  // authorities to none by default, we assume that the default test is on an
  // unmanaged device and browser thus we avoid unnecessarily waiting for
  // policies to load. Tests expecting either an enterprise user, a managed
  // device or browser should add the appropriate management authorities.
  policy::ScopedManagementServiceOverrideForTesting browser_management_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementTarget::BROWSER,
          base::flat_set<policy::EnterpriseManagementAuthority>());
  policy::ScopedManagementServiceOverrideForTesting platform_management_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementTarget::PLATFORM,
          base::flat_set<policy::EnterpriseManagementAuthority>());
};

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowPicker) {
  ProfilePicker::Show(ProfilePicker::EntryPoint::kOnStartup);
  WaitForLayoutWithoutToolbar();
  EXPECT_TRUE(ProfilePicker::IsOpen());
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForFirstPaint(web_contents(), GURL("chrome://profile-picker"));
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowChoice) {
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);
  WaitForLayoutWithoutToolbar();
  EXPECT_TRUE(ProfilePicker::IsOpen());
  // Check that non-default accessible title is provided both before the page
  // loads and after it loads.
  views::WidgetDelegate* delegate = widget()->widget_delegate();
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
  WaitForFirstPaint(web_contents(),
                    GURL("chrome://profile-picker/new-profile"));
  EXPECT_NE(delegate->GetWindowTitle(), delegate->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfile) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in sync
  // confirmation screen getting displayed.
  WaitForFirstPaint(web_contents(), GURL("chrome://sync-confirmation/"));

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));

  EXPECT_EQ(ThemeServiceFactory::GetForProfile(profile_being_created)
                ->GetAutogeneratedThemeColor(),
            kProfileColor);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileWithSyncDisabled) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Disable sync by setting the device as managed in prefs.
  syncer::SyncPrefs prefs(profile_being_created->GetPrefs());
  prefs.SetManagedForTest(true);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_being_created);

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in new browser
  // getting opened.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));

  WaitForPickerClosed();

  // Now the sync consent screen is shown, simulate closing the UI with "Stay
  // signed in"
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));
  EXPECT_EQ(ThemeServiceFactory::GetForProfile(profile_being_created)
                ->GetAutogeneratedThemeColor(),
            kProfileColor);
  EXPECT_FALSE(sync_service->GetUserSettings()->IsSyncRequested());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileSettings) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in sync
  // confirmation screen getting displayed.
  WaitForFirstPaint(web_contents(), GURL("chrome://sync-confirmation/"));

  // Simulate closing the UI with "Yes, I'm in".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://settings/syncSetup"));

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  // Sync is technically enabled for the profile. Without SyncService, the
  // difference between SYNC_WITH_DEFAULT_SETTINGS and CONFIGURE_SYNC_FIRST
  // cannot be told.
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));
  // The color is not applied if the user enters settings.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->UsingAutogeneratedTheme());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileOpenLink) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  StartSigninFlow();

  // Simulate clicking on a link that opens in a new window.
  const GURL kURL("https://foo.google.com");
  EXPECT_TRUE(ExecuteScript(web_contents(),
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
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    kURL);
}

// TODO(crbug.com/1144065): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       DISABLED_CreateSignedInProfileSigninAlreadyExists) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Create a pre-existing profile syncing with the same account as the profile
  // being created.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path,
      base::BindLambdaForTesting(
          [&run_loop](Profile* profile, Profile::CreateStatus status) {
            if (status == Profile::CREATE_STATUS_INITIALIZED) {
              run_loop.Quit();
            }
          }),
      base::string16(), std::string());
  run_loop.Run();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* other_entry =
      storage.GetProfileAttributesWithPath(new_path);
  ASSERT_NE(other_entry, nullptr);
  // Fake sync is enabled in this profile with Joe's account.
  other_entry->SetAuthInfo(std::string(),
                           base::UTF8ToUTF16("joe.consumer@gmail.com"),
                           /*is_consented_primary_account=*/true);

  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Instead of sync confirmation, a browser is displayed (with a login error).
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_FALSE(entry->IsAuthenticated());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));
  EXPECT_EQ(ThemeServiceFactory::GetForProfile(profile_being_created)
                ->GetAutogeneratedThemeColor(),
            kProfileColor);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileExtendedInfoTimeout) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);

  // Make it work without waiting for a long delay.
  ProfilePicker::SetExtendedAccountInfoTimeoutForTesting(
      base::TimeDelta::FromMilliseconds(10));

  // Add an account - simulate a successful Gaia sign-in.
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Wait for the sign-in to propagate to the flow, resulting in sync
  // confirmation screen getting displayed.
  WaitForFirstPaint(web_contents(), GURL("chrome://sync-confirmation/"));

  // Simulate closing the UI with "Yes, I'm in".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_FALSE(entry->IsAuthenticated());
  // Since the given name is not provided, the email address is used instead as
  // a profile name.
  EXPECT_EQ(entry->GetLocalProfileName(),
            base::UTF8ToUTF16("joe.consumer@gmail.com"));
  EXPECT_EQ(ThemeServiceFactory::GetForProfile(profile_being_created)
                ->GetAutogeneratedThemeColor(),
            kProfileColor);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfileWithSAML) {
  const GURL kNonGaiaURL("https://signin.saml-provider.com/");

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Redirect the web contents to a non gaia url (simulating a SAML page).
  content::WebContents* wc = web_contents();
  wc->GetController().LoadURL(kNonGaiaURL, content::Referrer(),
                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(wc, kNonGaiaURL);
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
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16(kWork));
  // The color is not applied if the user enters the SAML flow.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->UsingAutogeneratedTheme());
}

class ProfilePickerEnterpriseCreationFlowBrowserTest
    : public ProfilePickerCreationFlowBrowserTest {
 public:
  ProfilePickerEnterpriseCreationFlowBrowserTest() = default;

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&FakeUserPolicySigninService::BuildForEnterprise));
  }
};

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfile) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager, "joe.enterprise@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Enterprise domain needed for this profile being detected as Work.
  AccountInfo account_info =
      FillAccountInfo(core_account_info, "Joe", "enterprise.com");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in new browser
  // getting opened.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  // Inject a fake tab helper that confirms the enterprise dialog right away.
  base::RunLoop loop_until_enterprise_confirmed;
  TestTabDialogs::OverwriteForWebContents(
      new_browser->tab_strip_model()->GetActiveWebContents(),
      &loop_until_enterprise_confirmed);
  // Wait until the enterprise dialog in shown and confirmed.
  loop_until_enterprise_confirmed.Run();

  // The picker should be closed even before the enterprise confirmation but it
  // is closed asynchronously after opening the browser so after the NTP
  // renders, it is safe to check.
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));
  WaitForPickerClosed();

  // Now the sync consent screen is shown, simulate closing the UI with "No,
  // thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("enterprise.com"));

  // The color is not applied for enterprise users.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->UsingAutogeneratedTheme());
}

// Regression test for https://crbug.com/1184746.
IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileWithSyncDisabled) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager, "joe.enterprise@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Enterprise domain needed for this profile being detected as Work.
  AccountInfo account_info =
      FillAccountInfo(core_account_info, "Joe", "enterprise.com");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in new browser
  // getting opened and the enterprise dialog displayed.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();

  // Now disable sync by setting the device as managed in prefs. This simulates
  // that it is disabled after showing the enterprise dialog through policies
  // that are fetched.
  syncer::SyncPrefs prefs(profile_being_created->GetPrefs());
  prefs.SetManagedForTest(true);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_being_created);

  // Inject a fake tab helper that confirms the enterprise dialog right away.
  base::RunLoop loop_until_enterprise_confirmed;
  TestTabDialogs::OverwriteForWebContents(
      new_browser->tab_strip_model()->GetActiveWebContents(),
      &loop_until_enterprise_confirmed);
  // Wait until the enterprise dialog in shown and confirmed.
  loop_until_enterprise_confirmed.Run();

  // The picker should be closed even before the enterprise confirmation but it
  // is closed asynchronously after opening the browser so after the NTP
  // renders, it is safe to check.
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));
  WaitForPickerClosed();

  // Now the sync consent screen is shown, simulate closing the UI with "Stay
  // signed in"
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);

  // Check expectations when the profile creation flow is done.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("enterprise.com"));

  // The color is not applied for enterprise users.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->UsingAutogeneratedTheme());
  // Sync is disabled.
  EXPECT_FALSE(sync_service->GetUserSettings()->IsSyncRequested());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerEnterpriseCreationFlowBrowserTest,
                       CreateSignedInProfileSettings) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Profile* profile_being_created = StartSigninFlow();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  // Consumer-looking gmail address avoids code that forces the sync service to
  // actually start which would add overhead in mocking further stuff.
  CoreAccountInfo core_account_info = signin::MakeAccountAvailable(
      identity_manager, "joe.enterprise@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  // Enterprise domain needed for this profile being detected as Work.
  AccountInfo account_info =
      FillAccountInfo(core_account_info, "Joe", "enterprise.com");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in new browser
  // getting opened.
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  // Inject a fake tab helper that confirms the enterprise dialog right away.
  base::RunLoop loop_until_enterprise_confirmed;
  TestTabDialogs::OverwriteForWebContents(
      new_browser->tab_strip_model()->GetActiveWebContents(),
      &loop_until_enterprise_confirmed);
  // Wait until the enterprise dialog in shown and confirmed.
  loop_until_enterprise_confirmed.Run();

  // Now the sync consent screen is shown, simulate closing the UI with
  // "Configure sync".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::CONFIGURE_SYNC_FIRST);
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://settings/syncSetup"));

  // Check expectations when the profile creation flow is done.
  WaitForPickerClosed();

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("enterprise.com"));
  // The color is not applied if the user enters settings.
  EXPECT_FALSE(ThemeServiceFactory::GetForProfile(profile_being_created)
                   ->UsingAutogeneratedTheme());
}

class ProfilePickerCreationFlowEphemeralProfileBrowserTest
    : public ProfilePickerCreationFlowBrowserTest,
      public testing::WithParamInterface<ForceEphemeralProfilesPolicy> {
 public:
  ProfilePickerCreationFlowEphemeralProfileBrowserTest() = default;

  ForceEphemeralProfilesPolicy GetForceEphemeralProfilesPolicy() const {
    return GetParam();
  }

  bool AreEphemeralProfilesForced() const {
    return GetForceEphemeralProfilesPolicy() ==
           ForceEphemeralProfilesPolicy::kEnabled;
  }

  // Check that the policy was correctly applied to the preference.
  void CheckPolicyApplied(Profile* profile) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles),
              AreEphemeralProfilesForced());
  }

  static ProfileManager* profile_manager() {
    return g_browser_process->profile_manager();
  }

  // Checks if a profile matching `name` exists in the profile manager.
  bool ProfileWithNameExists(const std::string& name) {
    for (const auto* entry : profile_manager()
                                 ->GetProfileAttributesStorage()
                                 .GetAllProfilesAttributes()) {
      if (entry->GetLocalProfileName() == base::UTF8ToUTF16(name))
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
    ForceEphemeralProfilesPolicy policy = GetForceEphemeralProfilesPolicy();

    if (policy != ForceEphemeralProfilesPolicy::kUnset) {
      policy::PolicyMap policy_map;
      policy_map.Set(
          policy::key::kForceEphemeralProfiles, policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
          base::Value(policy == ForceEphemeralProfilesPolicy::kEnabled),
          nullptr);
      policy_provider_.UpdateChromePolicy(policy_map);

      ON_CALL(policy_provider_, IsInitializationComplete(testing::_))
          .WillByDefault(testing::Return(true));
      ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(testing::_))
          .WillByDefault(testing::Return(true));
      policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
          &policy_provider_);
    }
    ProfilePickerCreationFlowBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ProfilePickerCreationFlowBrowserTest::SetUpInProcessBrowserTestFixture();
    if (GetTestPreCount() == 1) {
      // Only called in "PRE_" tests, to set a name to the starting profile.
      ProfileAttributesEntry* entry =
          profile_manager()
              ->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(browser()->profile()->GetPath());
      ASSERT_NE(entry, nullptr);
      entry->SetLocalProfileName(base::UTF8ToUTF16(kOriginalProfileName),
                                 entry->IsUsingDefaultName());
    }
    CheckPolicyApplied(browser()->profile());
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

// Checks that the new profile is no longer ephemeral at the end of the flow and
// still exists after restart.
IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       PRE_Signin) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  ASSERT_TRUE(OriginalProfileExists());
  Profile* profile_being_created = StartSigninFlow();

  // Check that the profile is ephemeral, regardless of the policy.
  ProfileAttributesEntry* entry =
      profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_being_created->GetPath());
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->IsEphemeral());
  EXPECT_TRUE(entry->IsOmitted());
  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  // Wait for the sign-in to propagate to the flow, resulting in sync
  // confirmation screen getting displayed.
  WaitForFirstPaint(web_contents(), GURL("chrome://sync-confirmation/"));

  // Simulate closing the UI with "No, thanks".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::ABORT_SYNC);
  Browser* new_browser = BrowserAddedWaiter(2u).Wait();
  WaitForFirstPaint(new_browser->tab_strip_model()->GetActiveWebContents(),
                    GURL("chrome://newtab/"));

  WaitForPickerClosed();
  EXPECT_EQ(2u, profile_manager()->GetNumberOfProfiles());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));
  // The profile is no longer ephemeral, unless the policy is enabled.
  EXPECT_EQ(entry->IsEphemeral(), AreEphemeralProfilesForced());
  EXPECT_FALSE(entry->IsOmitted());
  // The preference is consistent with the policy.
  CheckPolicyApplied(profile_being_created);
}

IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       Signin) {
  if (AreEphemeralProfilesForced()) {
    // If the policy is set, all profiles should have been deleted.
    EXPECT_EQ(1u, profile_manager()->GetNumberOfProfiles());
    // The current profile is not the one that was created in the previous run.
    EXPECT_FALSE(ProfileWithNameExists("Joe"));
    EXPECT_FALSE(OriginalProfileExists());
    return;
  }

  // If the policy is disabled or unset, the two profiles are still here.
  EXPECT_EQ(2u, profile_manager()->GetNumberOfProfiles());
  EXPECT_TRUE(ProfileWithNameExists("Joe"));
  EXPECT_TRUE(OriginalProfileExists());
}

// Checks that the new profile is deleted on next startup if Chrome exits during
// the signin flow.
IN_PROC_BROWSER_TEST_P(ProfilePickerCreationFlowEphemeralProfileBrowserTest,
                       PRE_ExitDuringSignin) {
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  ASSERT_TRUE(OriginalProfileExists());
  Profile* profile_being_created = StartSigninFlow();

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
                       ExitDuringSignin) {
  // The profile was deleted, regardless of the policy.
  EXPECT_EQ(1u, profile_manager()->GetNumberOfProfiles());
  // The other profile still exists.
  EXPECT_NE(AreEphemeralProfilesForced(), OriginalProfileExists());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfilePickerCreationFlowEphemeralProfileBrowserTest,
    testing::Values(ForceEphemeralProfilesPolicy::kUnset,
                    ForceEphemeralProfilesPolicy::kDisabled,
                    ForceEphemeralProfilesPolicy::kEnabled));

}  // namespace
