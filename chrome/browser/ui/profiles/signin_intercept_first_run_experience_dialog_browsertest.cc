// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profiles/signin_intercept_first_run_experience_dialog.h"

#include "base/containers/enum_set.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
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
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
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

std::unique_ptr<KeyedService> CreateTestSyncService(content::BrowserContext*) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

// Browser tests for SigninInterceptFirstRunExperienceDialog.
using TestBase = InteractiveFeaturePromoTestMixin<SigninBrowserTestBase>;

class SigninInterceptFirstRunExperienceDialogBrowserTestBase : public TestBase {
 public:
  using DialogEvent = SigninInterceptFirstRunExperienceDialog::DialogEvent;
  using DialogEventSet =
      base::EnumSet<DialogEvent, DialogEvent::kStart, DialogEvent::kMaxValue>;

  SigninInterceptFirstRunExperienceDialogBrowserTestBase()
      : TestBase(UseDefaultTrackerAllowingPromos(
                     {feature_engagement::kIPHProfileSwitchFeature},
                     TrackerInitializationMode::kDoNotWait),
                 ClockMode::kUseTestClock,
                 InitialSessionState::kOutsideGracePeriod,
                 /*use_main_profile=*/true),
        scoped_iph_delay_(
            AvatarToolbarButton::SetScopedIPHMinDelayAfterCreationForTesting(
                base::Seconds(0))) {}

  ~SigninInterceptFirstRunExperienceDialogBrowserTestBase() override = default;

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
        context,
        base::BindRepeating(&policy::FakeUserPolicySigninService::Build));
  }

  void SetUpOnMainThread() override {
    TestBase::SetUpOnMainThread();
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  }

  // Returns true if the profile switch IPH has been shown.
  bool ProfileSwitchPromoHasBeenShown() {
    return RunTestSequence(
        WaitForPromo(feature_engagement::kIPHProfileSwitchFeature));
  }

  void UpdateChromePolicy(const policy::PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
  }

  void SignIn() {
    account_id_ = identity_test_env()
                      ->MakePrimaryAccountAvailable(
                          GetEmail(), signin::ConsentLevel::kSignin)
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
    const auto access_point =
        signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience;
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
    return browser()->GetFeatures().signin_view_controller();
  }

  SigninInterceptFirstRunExperienceDialog* dialog() {
    return static_cast<SigninInterceptFirstRunExperienceDialog*>(
        controller()->GetModalDialogForTesting());
  }

  CoreAccountId account_id() { return account_id_; }

  bool InUnoPhase2ModelWithFastFollows() {
    return base::FeatureList::IsEnabled(
               syncer::kReplaceSyncPromosWithSignInPromos) &&
           base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp);
  }

  void DisableHistorySync() {
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, false);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, false);
    sync_service()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, false);
  }

  virtual std::string GetEmail() { return kConsumerEmail; }

 protected:
  const GURL kSyncConfirmationUrl = AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation"),
      SyncConfirmationStyle::kSigninInterceptModal,
      /*is_sync_promo=*/true);
  const GURL kHistorySyncUrl =
      HistorySyncOptinUI::AppendHistorySyncOptinQueryParams(
          GURL("chrome://history-sync-optin"),
          HistorySyncOptinLaunchContext::kModal);
  const GURL kProfileCustomizationUrl = GURL("chrome://profile-customization");
  const GURL kSyncSettingsUrl = GURL("chrome://settings/syncSetup");

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  // Needed for profile switch IPH testing.
  base::AutoReset<base::TimeDelta> scoped_iph_delay_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

  CoreAccountId account_id_;
};

class SigninInterceptFirstRunExperienceDialogBrowserTest
    : public SigninInterceptFirstRunExperienceDialogBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SigninInterceptFirstRunExperienceDialogBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                          syncer::kUnoPhase2FollowUp};
    } else {
      disabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                           syncer::kUnoPhase2FollowUp};
    }
    scoped_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

class SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTestBase
    : public SigninInterceptFirstRunExperienceDialogBrowserTestBase {
 public:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    TestBase::OnWillCreateBrowserContextServices(context);

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestSyncService));
    policy::UserPolicySigninServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &policy::FakeUserPolicySigninService::BuildForEnterprise));
  }

  void UpdateExtendedAccountInfo() {
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(account_id());
    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("fullname")
                       .SetGivenName("givenname")
                       .SetLocale("en")
                       .SetAvatarUrl("https://example.com")
                       .SetHostedDomain("managed.com")
                       .Build();
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

  std::string GetEmail() override { return kEnterpriseEmail; }
};

class SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest
    : public SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                          syncer::kUnoPhase2FollowUp};
    } else {
      disabled_features = {syncer::kReplaceSyncPromosWithSignInPromos,
                           syncer::kUnoPhase2FollowUp};
    }
    scoped_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Shows and closes the fre dialog.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       ShowAndCloseDialog) {
  SignIn();
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  controller()->CloseModalSignin();
  EXPECT_FALSE(controller()->ShowsModalDialog());
}

// Goes through all steps of the fre dialog. The user enables sync or history
// sync (depending on which screen is offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSync) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);

  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    EXPECT_TRUE(login_ui_test_utils::ConfirmHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
    // The dialog still shows the history sync while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);

    // The dialog still shows the sync confirmation while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);
  }

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  if (InUnoPhase2ModelWithFastFollows()) {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowHistorySyncOptinScreen,
                          DialogEvent::kHistorySyncOptinAccept,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickDone});
  } else {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowSyncConfirmation,
                          DialogEvent::kSyncConfirmationClickConfirm,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickDone});
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// TODO(crbug.com/418143300): Add a test case for the history sync optin screen
// for managed users with a delayed execution of the management handling
// callback to ensure the history sync optin is eventually shown.

// Goes through all steps of the fre dialog and skips profile customization.
// The user enables sync or history sync (depending on which screen is offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncSkipCustomization) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    EXPECT_TRUE(login_ui_test_utils::ConfirmHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
    // The dialog still shows the history sync while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
    // The dialog still shows the sync confirmation while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);
  }

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationSkipButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  if (InUnoPhase2ModelWithFastFollows()) {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowHistorySyncOptinScreen,
                          DialogEvent::kHistorySyncOptinAccept,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickSkip});
  } else {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowSyncConfirmation,
                          DialogEvent::kSyncConfirmationClickConfirm,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickSkip});
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
  // TODO(crbug.com/40209493): test that the Skip button undoes the
  // changes in the theme color and the profile name.
}

// The user enables sync and has a synced extension theme. Tests that the dialog
// waits on the sync confirmation page until the extension theme is applied.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncExtensionTheme) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    // It does not matter if we accept or reject the history syncing.
    EXPECT_TRUE(login_ui_test_utils::RejectHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
    // The dialog still shows the sync confirmation while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);
  }

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kWaitingForExtensionInstallation);

  EXPECT_TRUE(controller()->ShowsModalDialog());
  // The dialog still shows the sync confirmation while waiting for the
  // extension theme to be downloaded and applied.
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      InUnoPhase2ModelWithFastFollows() ? kHistorySyncUrl
                                        : kSyncConfirmationUrl);

  // Trigger a new theme being applied. Use an autogenerated theme instead of an
  // extension theme because it's easier to trigger and doesn't make any
  // difference for this test.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorGREEN);
  theme_service()
      ->GetThemeSyncableService()
      ->NotifyOnSyncStartedForTesting(  // why the former is not enough to
                                        // trigger the update?
          ThemeSyncableService::ThemeSyncState::kApplied);

  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
}

// Tests that the profile customization is not shown when the user enables sync
// for an account with a custom passphrase.
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       AcceptSyncCustomPassphrase) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    // It does not matter whether we accept or reject this dialog.
    EXPECT_TRUE(login_ui_test_utils::RejectHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    sync_service()->SetPassphraseRequired();
    sync_service()->FireStateChanged();
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    sync_service()->SetPassphraseRequired();
    sync_service()->FireStateChanged();
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
  }

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !controller()->ShowsModalDialog(); }));
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  if (InUnoPhase2ModelWithFastFollows()) {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowHistorySyncOptinScreen,
                          DialogEvent::kHistorySyncOptinReject});
  } else {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowSyncConfirmation,
                          DialogEvent::kSyncConfirmationClickConfirm});
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// Goes through all steps of the fre dialog.
// The user declines sync or history sync (depending on which screen is
// offered).
IN_PROC_BROWSER_TEST_P(SigninInterceptFirstRunExperienceDialogBrowserTest,
                       DeclineSync) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    EXPECT_TRUE(login_ui_test_utils::RejectHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    // In the Uno phase2 model, declining the history sync screen has not impact
    // on theme syncing.
    theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        ThemeSyncableService::ThemeSyncState::kApplied);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(LoginUIService::ABORT_SYNC);
  }

  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  profile_customization_observer.Wait();
  EXPECT_EQ(
      dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
      kProfileCustomizationUrl);

  SimulateProfileCustomizationDoneButtonClicked();
  EXPECT_FALSE(controller()->ShowsModalDialog());
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  if (InUnoPhase2ModelWithFastFollows()) {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowHistorySyncOptinScreen,
                          DialogEvent::kHistorySyncOptinReject,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickDone});
  } else {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowSyncConfirmation,
                          DialogEvent::kSyncConfirmationClickCancel,
                          DialogEvent::kShowProfileCustomization,
                          DialogEvent::kProfileCustomizationClickDone});
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// Tests the case when the account has a profile color policy. Tests that the
// FRE dialog skips the profile customization step.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    ProfileColorPolicy) {
  SignIn();
  DisableHistorySync();

  content::TestNavigationObserver sync_confirmation_observer(
      kSyncConfirmationUrl);
  content::TestNavigationObserver history_sync_observer(kHistorySyncUrl);
  sync_confirmation_observer.StartWatchingNewWebContents();
  history_sync_observer.StartWatchingNewWebContents();

  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kBrowserThemeColor,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value("#000000"),
                 /*external_data_fetcher=*/nullptr);
  UpdateChromePolicy(policy_map);

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  if (InUnoPhase2ModelWithFastFollows()) {
    history_sync_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
    EXPECT_TRUE(login_ui_test_utils::ConfirmHistorySyncOptinDialog(
        browser(), login_ui_test_utils::kSyncConfirmationDialogTimeout, false));

    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
    // The dialog still shows the sync confirmation while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kHistorySyncUrl);
  } else {
    sync_confirmation_observer.Wait();
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);

    SimulateSyncConfirmationUIClosing(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSync);
    // The dialog still shows the sync confirmation while waiting for the synced
    // theme to be applied.
    EXPECT_TRUE(controller()->ShowsModalDialog());
    EXPECT_EQ(
        dialog()->GetModalDialogWebContentsForTesting()->GetLastCommittedURL(),
        kSyncConfirmationUrl);
  }

  theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
      ThemeSyncableService::ThemeSyncState::kApplied);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !controller()->ShowsModalDialog(); }));
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());

  if (InUnoPhase2ModelWithFastFollows()) {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowHistorySyncOptinScreen,
                          DialogEvent::kHistorySyncOptinAccept});
  } else {
    ExpectRecordedEvents({DialogEvent::kStart,
                          DialogEvent::kShowSyncConfirmation,
                          DialogEvent::kSyncConfirmationClickConfirm});
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// The user chooses to manage sync settings in the sync confirmation dialog.
// The profile customization is not shown in this case.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    SyncSettings) {
  if (InUnoPhase2ModelWithFastFollows()) {
    GTEST_SKIP() << "History opt-in does not have a settings link.";
  }

  SignIn();
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
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    CloseDialogBeforeSyncConfirmationIsShown) {
  if (InUnoPhase2ModelWithFastFollows()) {
    GTEST_SKIP() << "History opt-in does not use the TurnSyncOnHelper object";
  }

  // It's important to use an enterprise email here in order to block the sync
  // confirmation UI until the sync engine starts.
  SignIn();
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
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !TurnSyncOnHelper::HasCurrentTurnSyncOnHelperForTesting(
        GetProfile());
  }));

  // Sync is aborted.
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  ExpectRecordedEvents({DialogEvent::kStart});
  ExpectSigninHistogramsRecorded();
}

// Tests the case when sync is disabled by policy. The fre dialog starts with
// the profile customization UI.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    SyncDisabled) {
  SignIn();
  sync_service()->SetAllowedByEnterprisePolicy(false);
  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  // Sync confirmation is skipped.
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ false);
  EXPECT_TRUE(controller()->ShowsModalDialog());
  if (InUnoPhase2ModelWithFastFollows()) {
    theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        ThemeSyncableService::ThemeSyncState::kApplied);
  }
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
  if (!InUnoPhase2ModelWithFastFollows()) {
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// Tests the case when the user went through the forced intercept dialog. The
// FRE dialog should skip the sync confirmation.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    ForcedIntercept) {
  SignIn();
  content::TestNavigationObserver profile_customization_observer(
      kProfileCustomizationUrl);
  profile_customization_observer.StartWatchingNewWebContents();

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ true);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  if (InUnoPhase2ModelWithFastFollows()) {
    theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        ThemeSyncableService::ThemeSyncState::kApplied);
  }

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
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    PromotionalTabsDisabled) {
  SignIn();
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
  if (InUnoPhase2ModelWithFastFollows()) {
    theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        ThemeSyncableService::ThemeSyncState::kApplied);
  }
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

  if (!InUnoPhase2ModelWithFastFollows()) {
    // Note: the account was already signed in, so the histograms should not be
    // recorded. `TurnOnSyncHelper` records them anyway.
    ExpectSigninHistogramsRecorded();
  }
}

// Tests the case when the user went through the forced intercept dialog and the
// account has a profile color policy. Tests that the FRE dialog exits
// immediately and displays the profile switch IPH.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    ForcedIntercept_ProfileColorPolicy) {
  SignIn();
  DisableHistorySync();

  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kBrowserThemeColor,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value("#000000"),
                 /*external_data_fetcher=*/nullptr);
  UpdateChromePolicy(policy_map);

  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /* is_forced_intercept = */ true);

  if (InUnoPhase2ModelWithFastFollows()) {
    theme_service()->GetThemeSyncableService()->NotifyOnSyncStartedForTesting(
        ThemeSyncableService::ThemeSyncState::kApplied);
  }

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !controller()->ShowsModalDialog(); }));

  ExpectPrimaryAccountWithExactConsentLevel(signin::ConsentLevel::kSignin);
  EXPECT_TRUE(ProfileSwitchPromoHasBeenShown());
  ExpectRecordedEvents({DialogEvent::kStart});
}

INSTANTIATE_TEST_SUITE_P(All,
                         SigninInterceptFirstRunExperienceDialogBrowserTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTest,
    ::testing::Bool());

class
    SigninInterceptFirstRunExperienceDialogEnterpriseManagementHandlingBrowserTest
    : public SigninInterceptFirstRunExperienceDialogEnterpriseUserBrowserTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SigninInterceptFirstRunExperienceDialogEnterpriseManagementHandlingBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (std::get<0>(GetParam())) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    } else {
      disabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    if (std::get<1>(GetParam())) {
      enabled_features.push_back(switches::kEnforceManagementDisclaimer);
    } else {
      disabled_features.push_back(switches::kEnforceManagementDisclaimer);
    }
    disabled_features.push_back(syncer::kUnoPhase2FollowUp);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the managed is accepted for managed users, without any action from
// the user, once the dialog is is shown.
IN_PROC_BROWSER_TEST_P(
    SigninInterceptFirstRunExperienceDialogEnterpriseManagementHandlingBrowserTest,
    ManagementIsAutoAccepted) {
  SignIn();
  controller()->ShowModalInterceptFirstRunExperienceDialog(
      account_id(), /*is_forced_intercept=*/false);
  EXPECT_TRUE(controller()->ShowsModalDialog());

  // When `ReplaceSyncWithSignInPromos` is enabled, management handing requires
  // on the extended account info being available.
  UpdateExtendedAccountInfo();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return enterprise_util::UserAcceptedAccountManagement(GetProfile());
  }));

  policy::FakeUserPolicySigninService* fake_policy_service =
      static_cast<policy::FakeUserPolicySigninService*>(
          policy::UserPolicySigninServiceFactory::GetForProfile(GetProfile()));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return fake_policy_service->policy_fetched(); }));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SigninInterceptFirstRunExperienceDialogEnterpriseManagementHandlingBrowserTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));
