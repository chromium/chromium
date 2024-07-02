// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/signin_intercept_first_run_experience_dialog.h"

#include "base/containers/enum_set.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/skia/include/core/SkColor.h"
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
    std::move(callback).Run(std::string(), std::string(),
                            std::vector<std::string>());
  }

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override {
    std::move(callback).Run(true);
  }
};

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
using TestBase = InteractiveFeaturePromoTestT<SigninBrowserTestBase>;
class SigninInterceptFirstRunExperienceDialogBrowserTest : public TestBase {
 public:
  using DialogEvent = SigninInterceptFirstRunExperienceDialog::DialogEvent;
  using DialogEventSet =
      base::EnumSet<DialogEvent, DialogEvent::kStart, DialogEvent::kMaxValue>;

  SigninInterceptFirstRunExperienceDialogBrowserTest()
      : TestBase(UseDefaultTrackerAllowingPromos(
                     {feature_engagement::kIPHProfileSwitchFeature},
                     TrackerInitializationMode::kDoNotWait),
                 ClockMode::kUseTestClock,
                 InitialSessionState::kOutsideGracePeriod,
                 /*use_main_profile=*/true) {}

  ~SigninInterceptFirstRunExperienceDialogBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    TestBase::SetUpInProcessBrowserTestFixture();

    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    TestBase::OnWillCreateBrowserContextServices(context);

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestUserPolicySigninService));
  }

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

    // Needed for profile switch IPH testing.
    AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(
        base::Seconds(0));
  }

  // Returns true if the profile switch IPH has been shown.
  bool ProfileSwitchPromoHasBeenShown() {
    return RunTestSequence(
        WaitForPromo(feature_engagement::kIPHProfileSwitchFeature));
  }

  void UpdateChromePolicy(const policy::PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
  }

  void SetAccountCookieAndToken(const std::string& email) {
    account_id_ = SetAccountsCookiesAndTokens({email})[0].account_id;
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
    LoginUIServiceFactory::GetForProfile(GetProfile())
        ->SyncConfirmationUIClosed(result);
  }

  void SimulateProfileCustomizationDoneButtonClicked() {
    dialog()->ProfileCustomizationCloseOnCompletion(
        ProfileCustomizationHandler::CustomizationResult::kDone);
  }

  void SimulateProfileCustomizationSkipButtonClicked() {
    dialog()->ProfileCustomizationCloseOnCompletion(
        ProfileCustomizationHandler::CustomizationResult::kSkip);
  }

  void ExpectRecordedEvents(DialogEventSet events) {
    std::vector<base::Bucket> expected_buckets;
    for (DialogEvent event : events) {
      expected_buckets.emplace_back(static_cast<int>(event), 1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.Intercept.FRE.Event"),
                ::testing::ContainerEq(expected_buckets));
  }

  void ExpectSigninHistogramsRecorded() {
    const auto access_point = signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE;
    histogram_tester_.ExpectUniqueSample("Signin.SigninStartedAccessPoint",
                                         access_point, 1);
    histogram_tester_.ExpectUniqueSample("Signin.SigninCompletedAccessPoint",
                                         access_point, 1);
    EXPECT_EQ(user_action_tester_.GetActionCount(
                  "Signin_Signin_FromSigninInterceptFirstRunExperience"),
              1);
  }

  // `kSignin` consent level means that Sync should be disabled.
  void ExpectPrimaryAccountWithExactConsentLevel(
      signin::ConsentLevel consent_level) {
    EXPECT_EQ(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        account_id());
    EXPECT_EQ(consent_level,
              signin::GetPrimaryAccountConsentLevel(identity_manager()));
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  ThemeService* theme_service() {
    return ThemeServiceFactory::GetForProfile(GetProfile());
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
  const GURL kSyncConfirmationUrl = AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation"),
      SyncConfirmationStyle::kSigninInterceptModal,
      /*is_sync_promo=*/true);
  const GURL kProfileCustomizationUrl = GURL("chrome://profile-customization");
  const GURL kSyncSettingsUrl = GURL("chrome://settings/syncSetup");

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  CoreAccountId account_id_;
};

// Shows and closes the fre dialog.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ShowAndCloseDialog) {
  SignIn(kConsumerEmail);
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
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

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  // The dialog still shows the sync confirmation while waiting for the synced
  // theme to be applied.
  EXPECT_TRUE(controller()->ShowsModalDialog());
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  ExpectSigninHistogramsRecorded();
}

// Goes through all steps of the fre dialog and skips profile customization.
// The user enables sync.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncSkipCustomization) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  // The dialog still shows the sync confirmation while waiting for the synced
  // theme to be applied.
  EXPECT_TRUE(controller()->ShowsModalDialog());
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationSkipButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickSkip});
  ExpectSigninHistogramsRecorded();
  // TODO(crbug.com/40209493): test that the Skip button undoes the
  // changes in the theme color and the profile name.
}

// The user enables sync and has a synced extension theme. Tests that the dialog
// waits on the sync confirmation page until the extension theme is applied.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncExtensionTheme) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  // The dialog still shows the sync confirmation while waiting for the synced
  // theme to be applied.
  EXPECT_TRUE(controller()->ShowsModalDialog());
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kWaitingForExtensionInstallation);
  // The dialog still shows the sync confirmation while waiting for the
  // extension theme to be downloaded and applied.
  EXPECT_TRUE(controller()->ShowsModalDialog());
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  // Trigger a new theme being applied. Use an autogenerated theme instead of an
  // extension theme because it's easier to trigger and doesn't make any
  // difference for this test.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorGREEN);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// Tests that the profile customzation is not shown when the user enables sync
// for an account with a custom passphrase.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncCustomPassphrase) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  sync_service()->SetPassphraseRequired();
  sync_service()->FireStateChanged();
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm});
  ExpectSigninHistogramsRecorded();
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

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
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

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickCancel,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  ExpectSigninHistogramsRecorded();
}

// Tests the case when the account has a profile color policy. Tests that the
// FRE dialog skips the profile customization step.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ProfileColorPolicy) {
  SignIn(kEnterpriseEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kBrowserThemeColor,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value("#000000"),
                 /*external_data_fetcher=*/nullptr);
  UpdateChromePolicy(policy_map);

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  sync_confirmation_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  SimulateSyncConfirmationUIClosing(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  // The dialog still shows the sync confirmation while waiting for the synced
  // theme to be applied.
  EXPECT_TRUE(controller()->ShowsModalDialog());
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kSyncConfirmationUrl);

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickConfirm});
  ExpectSigninHistogramsRecorded();
}

// The user chooses to manage sync settings in the sync confirmation dialog.
// The profile customization is not shown in this case.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       SyncSettings) {
  SignIn(kConsumerEmail);
  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
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
  ExpectRecordedEvents({DialogEvent::kStart, DialogEvent::kShowSyncConfirmation,
                        DialogEvent::kSyncConfirmationClickSettings});
  ExpectSigninHistogramsRecorded();
}

// Closes the fre dialog before the sync confirmation is shown. Tests that
// `TurnSyncOnHelper` is eventually destroyed.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       CloseDialogBeforeSyncConfirmationIsShown) {
  // It's important to use an enterprise email here in order to block the sync
  // confirmation UI until the sync engine starts.
  SignIn(kEnterpriseEmail);
  // Delays the sync confirmation UI.
  sync_service()->SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);

  // The dialog is shown asynchronously. We wait for the posted tasks to run,
  // but nothing will be shown until we update the Sync service state.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller()->ShowsModalDialog());

  controller()->CloseModalSignin();
  EXPECT_FALSE(controller()->ShowsModalDialog());

  // `TurnSyncOnHelper` should be destroyed after the sync engine is up and
  // running.
  sync_service()->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service()->FireStateChanged();
  EXPECT_FALSE(
      TurnSyncOnHelper::HasCurrentTurnSyncOnHelperForTesting(GetProfile()));
  // Sync is aborted.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  ExpectRecordedEvents({DialogEvent::kStart});
  ExpectSigninHistogramsRecorded();
}

// Tests the case when sync is disabled by policy. The fre dialog starts with
// the profile customization UI.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       SyncDisabled) {
  SignIn(kEnterpriseEmail);
  sync_service()->SetAllowedByEnterprisePolicy(false);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  // Sync confirmation is skipped.
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);
  // Sync consent should not be granted since the user hasn't seen any consent
  // UI.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  ExpectSigninHistogramsRecorded();
}

// Tests the case when the user went through the forced intercept dialog. The
// FRE dialog should skip the sync confirmation.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ForcedIntercept) {
  SignIn(kEnterpriseEmail);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ true);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();

  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
}

// Tests the case when promotional tabs are disabled by policy. The FRE dialog
// should skip the sync confirmation.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       PromotionalTabsDisabled) {
  SignIn(kEnterpriseEmail);
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kPromotionalTabsEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(false),
                 /*external_data_fetcher=*/nullptr);
  UpdateChromePolicy(policy_map);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  // Sync confirmation is skipped.
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);
  // Sync consent should not be granted since the user hasn't seen any consent
  // UI.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart,
                        DialogEvent::kShowProfileCustomization,
                        DialogEvent::kProfileCustomizationClickDone});
  ExpectSigninHistogramsRecorded();
}

// Tests the case when the user went through the forced intercept dialog and the
// account has a profile color policy. Tests that the FRE dialog exits
// immediately and displays the profile switch IPH.
IN_PROC_BROWSER_TEST_F(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ForcedIntercept_ProfileColorPolicy) {
  SignIn(kEnterpriseEmail);
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kBrowserThemeColor,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value("#000000"),
                 /*external_data_fetcher=*/nullptr);
  UpdateChromePolicy(policy_map);

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ true);

  // Wait for the dialog creation posted tasks to complete.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(controller()->ShowsModalDialog());
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart});
}
