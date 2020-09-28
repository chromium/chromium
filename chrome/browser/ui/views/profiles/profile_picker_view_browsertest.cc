// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

AccountInfo FillAccountInfo(const CoreAccountInfo& core_info,
                            const std::string& given_name) {
  AccountInfo account_info;
  account_info.email = core_info.email;
  account_info.gaia = core_info.gaia;
  account_info.account_id = core_info.account_id;
  account_info.is_under_advanced_protection =
      core_info.is_under_advanced_protection;
  account_info.full_name = "Test Full Name";
  account_info.given_name = given_name;
  account_info.hosted_domain = kNoHostedDomainFound;
  account_info.locale = "en";
  account_info.picture_url = "https://get-avatar.com/foo";
  account_info.is_child_account = false;
  return account_info;
}

// Waits until a new profile is created.
class ProfileWaiter : public ProfileManagerObserver {
 public:
  ProfileWaiter() {
    profile_manager_observer_.Add(g_browser_process->profile_manager());
  }

  ~ProfileWaiter() override = default;

  Profile* WaitForProfileAdded() {
    run_loop_.Run();
    return profile_;
  }

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    profile_manager_observer_.RemoveAll();
    profile_ = profile;
    run_loop_.Quit();
  }

  Profile* profile_ = nullptr;
  ScopedObserver<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  base::RunLoop run_loop_;
};

// Waits until a first non empty paint for given `url`.
class FirstVisuallyNonEmptyPaintObserver : public content::WebContentsObserver {
 public:
  explicit FirstVisuallyNonEmptyPaintObserver(content::WebContents* contents,
                                              const GURL& url)
      : content::WebContentsObserver(contents), url_(url) {}

  void DidFirstVisuallyNonEmptyPaint() override {
    if (web_contents()->GetVisibleURL() == url_)
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  GURL url_;
};

// Fake user policy signin service immediately invoking the callbacks.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<FakeUserPolicySigninService>(
        profile, IdentityManagerFactory::GetForProfile(profile));
  }

  FakeUserPolicySigninService(Profile* profile,
                              signin::IdentityManager* identity_manager)
      : UserPolicySigninService(profile,
                                nullptr,
                                nullptr,
                                nullptr,
                                identity_manager,
                                nullptr) {}

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

class ProfilePickerCreationFlowBrowserTest : public InProcessBrowserTest {
 public:
  ProfilePickerCreationFlowBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kProfilesUIRevamp, features::kNewProfilePicker}, {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::Bind(&ProfilePickerCreationFlowBrowserTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));

    // A hack for DiceTurnSyncOnHelper to actually skip talking to SyncService.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kDisableSync);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&FakeUserPolicySigninService::Build));
  }

  content::WebContents* web_contents() {
    return ProfilePicker::GetWebViewForTesting()->GetWebContents();
  }

 private:
  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      create_services_subscription_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest, ShowChoice) {
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);
  // Wait for web_contents() to get created.
  base::RunLoop().RunUntilIdle();
  FirstVisuallyNonEmptyPaintObserver(
      web_contents(), GURL("chrome://profile-picker/new-profile"))
      .Wait();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerCreationFlowBrowserTest,
                       CreateSignedInProfile) {
  const SkColor kProfileColor = SK_ColorRED;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  BrowserList* active_browser_list = BrowserList::GetInstance();

  ASSERT_EQ(1u, profile_manager->GetNumberOfProfiles());
  EXPECT_EQ(1u, active_browser_list->size());

  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileMenuAddNewProfile);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Simulate a click on the signin button.
  base::MockCallback<base::OnceClosure> switch_failure_callback;
  EXPECT_CALL(switch_failure_callback, Run()).Times(0);
  ProfilePicker::SwitchToSignIn(kProfileColor, switch_failure_callback.Get());

  // Need to wait for the new profile being created before waiting on
  // web_contents() because web_contents is reconstructed with the new profile.
  Profile* profile_being_created = ProfileWaiter().WaitForProfileAdded();
  base::RunLoop().RunUntilIdle();
  FirstVisuallyNonEmptyPaintObserver(
      web_contents(), GaiaUrls::GetInstance()->signin_chrome_sync_dice())
      .Wait();

  // Add an account - simulate a successful Gaia sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_being_created);
  CoreAccountInfo core_account_info =
      signin::MakeAccountAvailable(identity_manager, "joe.consumer@gmail.com");
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      core_account_info.account_id));

  AccountInfo account_info = FillAccountInfo(core_account_info, "Joe");
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  FirstVisuallyNonEmptyPaintObserver(web_contents(),
                                     GURL("chrome://sync-confirmation/"))
      .Wait();

  // Simulate closing the UI with "Yes, I'm in".
  LoginUIServiceFactory::GetForProfile(profile_being_created)
      ->SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  base::RunLoop().RunUntilIdle();

  // Check expectations when the profile creation flow is done.
  EXPECT_EQ(2u, active_browser_list->size());
  EXPECT_FALSE(ProfilePicker::IsOpen());

  ProfileAttributesEntry* entry = nullptr;
  ASSERT_TRUE(g_browser_process->profile_manager()
                  ->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(
                      profile_being_created->GetPath(), &entry));
  EXPECT_FALSE(entry->IsEphemeral());
  EXPECT_EQ(entry->GetLocalProfileName(), base::UTF8ToUTF16("Joe"));
  EXPECT_EQ(ThemeServiceFactory::GetForProfile(profile_being_created)
                ->GetAutogeneratedThemeColor(),
            kProfileColor);
}

}  // namespace
