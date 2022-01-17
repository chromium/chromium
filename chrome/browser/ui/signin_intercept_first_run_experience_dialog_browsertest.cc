// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_intercept_first_run_experience_dialog.h"

#include "base/test/bind.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace {

// Sync might use email address as an heuristic to determine whether an account
// might be managed.
const char kConsumerEmail[] = "test@example.com";
const char kEnterpriseEmail[] = "test@managed.com";

// Fake user policy signin service immediately invoking the callbacks.
// TODO(alexilin): write a common FakeUserPolicySigninService for using in
// sign-in tests instead of maintaining several copies.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
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
    std::move(callback).Run(std::string(), std::string());
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
};

std::unique_ptr<KeyedService> CreateTestTracker(content::BrowserContext*) {
  return feature_engagement::CreateTestTracker();
}

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> CreateTestUserPolicySigninService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<FakeUserPolicySigninService>(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace

// Browser tests for SigninInterceptFirstRunExperienceDialog.
class SigninInterceptFirstRunExperienceDialogBrowserTest
    : public InProcessBrowserTest {
 public:
  SigninInterceptFirstRunExperienceDialogBrowserTest()
      : feature_list_(feature_engagement::kIPHProfileSwitchFeature) {}
  ~SigninInterceptFirstRunExperienceDialogBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SigninInterceptFirstRunExperienceDialogBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(
            context, signin::AccountConsistencyMethod::kDice);
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestTracker));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestUserPolicySigninService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

    // Needed for profile switch IPH testing.
    AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(
        base::Seconds(0));
    FeaturePromoControllerViews::BlockActiveWindowCheckForTesting();
  }

  // Returns true if the profile switch IPH has been shown.
  bool ProfileSwitchPromoHasBeenShown() {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser()->profile());

    base::RunLoop loop;
    tracker->AddOnInitializedCallback(
        base::BindLambdaForTesting([&loop](bool success) {
          DCHECK(success);
          loop.Quit();
        }));
    loop.Run();

    EXPECT_TRUE(tracker->IsInitialized());
    return tracker->GetTriggerState(
               feature_engagement::kIPHProfileSwitchFeature) ==
           feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED;
  }

  void SignIn(const std::string& email) {
    account_id_ =
        identity_test_env()
            ->MakePrimaryAccountAvailable(email, signin::ConsentLevel::kSignin)
            .account_id;
    EXPECT_EQ(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        account_id());
  }

  void SimulateSyncConfirmationUIClosing(
      LoginUIService::SyncConfirmationUIClosedResult result) {
    LoginUIServiceFactory::GetForProfile(browser()->profile())
        ->SyncConfirmationUIClosed(result);
  }

  void SimulateProfileCustomizationUIClosing() {
    dialog()->OnProfileCustomizationDoneButtonClicked();
  }

  // `kSignin` consent level means that Sync should be disabled.
  void ExpectPrimaryAccountWithExactConsentLevel(
      signin::ConsentLevel consent_level) {
    EXPECT_EQ(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        account_id());
    EXPECT_EQ(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync),
        consent_level == signin::ConsentLevel::kSync);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }

  SigninViewController* controller() {
    return browser()->signin_view_controller();
  }

  SigninInterceptFirstRunExperienceDialog* dialog() {
    return static_cast<SigninInterceptFirstRunExperienceDialog*>(
        controller()->GetModalDialogForTesting());
  }

  CoreAccountId account_id() { return account_id_; }

 protected:
  const GURL kSyncConfirmationUrl = GURL("chrome://sync-confirmation");
  const GURL kProfileCustomizationUrl = GURL("chrome://profile-customization");
  const GURL kSyncSettingsUrl = GURL("chrome://settings/syncSetup");

 private:
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::test::ScopedFeatureList feature_list_;

  CoreAccountId account_id_;
};

// Shows and closes the fre dialog.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ShowAndCloseDialog) {
  SignIn(kConsumerEmail);
  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());
  controller()->CloseModalSignin();
  EXPECT_FALSE(controller()->ShowsModalDialog());
}

// Goes through all steps of the fre dialog. The user enables sync.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSync) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationUIClosing();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// Goes through all steps of the fre dialog. The user declines sync.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       DeclineSync) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::ABORT_SYNC);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationUIClosing();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// The user chooses to manage sync settings in the sync confirmation dialog.
// The profile customization is not shown in this case.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       SyncSettings) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::CONFIGURE_SYNC_FIRST);
  // kSync consent level is not revoked.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  // Browser displays a sync settings tab.
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      kSyncSettingsUrl);
  // Sync settings abort the fre dialog.
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// Closes the fre dialog before the sync confirmation is shown. Tests that
// `DiceTurnSyncOnHelper` is eventually destroyed.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       CloseDialogBeforeSyncConfirmationIsShown) {
  // It's important to use an enterprise email here in order to block the sync
  // confirmation UI until the sync engine starts.
  SignIn(kEnterpriseEmail);
  // Delays the sync confirmation UI.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());

  controller()->CloseModalSignin();
  EXPECT_FALSE(controller()->ShowsModalDialog());

  // `DiceTurnSyncOnHelper` should be destroyed after the sync engine is up and
  // running.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service()->FireStateChanged();
  EXPECT_FALSE(DiceTurnSyncOnHelper::HasCurrentDiceTurnSyncOnHelperForTesting(
      browser()->profile()));
  // Sync is aborted.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
}

// Tests the case when sync is disabled by policy. The fre dialog starts with
// the profile customization UI.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       SyncDisabled) {
  SignIn(kEnterpriseEmail);
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  // Sync confirmation is skipped.
  controller()->ShowModalInterceptFirstRunExperienceDialog(account_id());
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);
  // Sync consent is granted even though Sync cannot be enabled.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);

  SimulateProfileCustomizationUIClosing();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}
