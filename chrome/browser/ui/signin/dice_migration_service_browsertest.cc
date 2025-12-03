// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kEnterpriseTestEmail[] = "test@google.com";
constexpr char kIndeterminableTestEmail[] = "test@indeterminable.com";

constexpr char kDialogCloseReasonHistogram[] =
    "Signin.DiceMigrationDialog.CloseReason";
constexpr char kDialogTimerStartedHistogram[] =
    "Signin.DiceMigrationDialog.TimerStarted";
constexpr char kDialogPreviouslyShownCountHistogram[] =
    "Signin.DiceMigrationDialog.PreviouslyShownCount";
constexpr char kDialogDaysSinceLastShownHistogram[] =
    "Signin.DiceMigrationDialog.DaysSinceLastShown";
constexpr char kDialogShownHistogram[] = "Signin.DiceMigrationDialog.Shown";
constexpr char kAccountManagedStatusHistogram[] =
    "Signin.DiceMigrationDialog.AccountManagedStatus";
constexpr char kUserMigratedHistogram[] = "Signin.DiceMigrationDialog.Migrated";
constexpr char kDialogNotShownReasonHistogram[] =
    "Signin.DiceMigrationDialog.NotShownReason";
constexpr char kRestoredFromBackupHistogram[] =
    "Signin.DiceMigration.RestoredFromBackup";
constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";
constexpr char kSignoutReasonHistogram[] = "Signin.SignOut.Completed";
constexpr char kToastTriggerToShowHistogram[] = "Toast.TriggeredToShow";
constexpr char kForcedMigrationAccountManagedHistogram[] =
    "Signin.ForcedDiceMigration.HasAcceptedAccountManagement";

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of `DiceMigrationServiceBrowserTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)    \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) { \
    ImplicitlySignIn(kTestEmail);                       \
  }                                                     \
  IN_PROC_BROWSER_TEST_F(test_suite, test_name)

bool ContainsViewWithId(const views::View* view, ui::ElementIdentifier id) {
  for (const views::View* child : view->children()) {
    if (child->GetVisible() &&
        (child->GetProperty(views::kElementIdentifierKey) == id ||
         // Recurse into the child.
         ContainsViewWithId(child, id))) {
      return true;
    }
  }
  return false;
}

class DiceMigrationServiceBrowserTest : public InProcessBrowserTest {
 public:
  DiceMigrationServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kOfferMigrationToDiceUsers},
        // DICe migration dialog is not shown when forced migration flag is
        // enabled.
        /*disabled_features=*/{switches::kForcedDiceMigration});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            GetProfile());
  }

  void ImplicitlySignIn(const std::string& email) {
    AccountInfo account_info = signin::MakeAccountAvailable(
        GetIdentityManager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
            .Build(email));
  }

  bool IsImplicitlySignedIn() {
    return GetIdentityManager()->HasPrimaryAccount(
               signin::ConsentLevel::kSignin) &&
           signin::IsImplicitBrowserSigninOrExplicitDisabled(
               GetIdentityManager(), GetPrefs());
  }

  bool IsExplicitlySignedIn() {
    return GetIdentityManager()->HasPrimaryAccount(
               signin::ConsentLevel::kSignin) &&
           !signin::IsImplicitBrowserSigninOrExplicitDisabled(
               GetIdentityManager(), GetPrefs());
  }

  void FireDialogTriggerTimer() {
    base::OneShotTimer& timer =
        GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
    ASSERT_TRUE(timer.IsRunning());
    timer.FireNow();
  }

  Profile* GetProfile() { return browser()->profile(); }

  PrefService* GetPrefs() { return GetProfile()->GetPrefs(); }

  DiceMigrationService* GetDiceMigrationService() {
    DiceMigrationService* service =
        DiceMigrationServiceFactory::GetForProfileIfExists(GetProfile());
    EXPECT_TRUE(service);
    return service;
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetProfile());
  }

  syncer::SyncService* GetSyncService() {
    return SyncServiceFactory::GetForProfile(GetProfile());
  }

  AvatarToolbarButton* GetAvatarToolbarButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetAvatarToolbarButton();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedClosureRunner disclaimer_service_resetter_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, NotSignedIn) {
  // The user is not signed in.
  ASSERT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The timer to trigger the dialog is not started.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, PRE_Syncing) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSync);
  // TODO(crbug.com/464457988): Mark sync setup as complete by default in the
  // sign-in helper method.
  GetSyncService()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  ASSERT_TRUE(GetSyncService()->IsSyncFeatureEnabled());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, Syncing) {
  // The user is syncing.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ExplicitlySignedIn) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSignin);
  ASSERT_TRUE(IsExplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ExplicitlySignedIn) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), GetPrefs()));

  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ImplicitlySignedIn) {
  ImplicitlySignIn(kTestEmail);
  ASSERT_TRUE(IsImplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ImplicitlySignedIn) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  EXPECT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, true, 1);

  // Trigger the timer.
  FireDialogTriggerTimer();
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogShownHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kDialogNotShownReasonHistogram, 0);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      ShouldNotShowDialogIfNotEligibleAnymore) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Set the explicit sign-in pref to true. This should make the user ineligible
  // for the migration, but the timer still runs. This is a test-only scenario
  // and should not happen in production.
  GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Show the migration bubble.
  FireDialogTriggerTimer();

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogShownHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kNotEligible, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest, MigrateUser) {
  constexpr syncer::UserSelectableTypeSet new_selected_types = {
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  };

  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // These types are only enabled upon explicitly signing in.
  ASSERT_FALSE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAny(
      new_selected_types));

  // Show migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  EXPECT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  EXPECT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  EXPECT_FALSE(IsImplicitlySignedIn());
  EXPECT_TRUE(IsExplicitlySignedIn());

  // This should set the relevant user selected types.
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll(
      new_selected_types))
      << GetSyncService()->GetUserSettings()->GetSelectedTypes();

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      ShouldNotMigrateUserIfIneligible) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  // Set the explicit sign-in pref to true. This should make the user
  // ineligible for the migration. The dialog is still shown but it's okay since
  // this is a test-only scenario and should not happen in production.
  GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on the accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  EXPECT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  // The prefs are not updated.
  EXPECT_EQ(GetPrefs()->GetBoolean(
                prefs::kPrefsThemesSearchEnginesAccountStorageEnabled),
            false);

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, false, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      IncrementDialogShownCount) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Set the current dialog shown count to 1.
  GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, 1);

  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);

  // The dialog shown count is now incremented.
  EXPECT_EQ(GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 2);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      UpdateDialogLastShownTime) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  base::Time time_now = base::Time::Now();
  ASSERT_LT(GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime), time_now);

  // Not logged since the dialog was never shown before.
  histogram_tester_.ExpectTotalCount(kDialogDaysSinceLastShownHistogram, 0);

  // Show the migration bubble.
  FireDialogTriggerTimer();

  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // The dialog last shown time is now updated.
  EXPECT_GE(GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime), time_now);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      DoNotUpdateDialogShownCountAndTimeUponInteraction) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Set the current dialog shown count to 1.
  GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, 1);

  base::Time time_now = base::Time::Now();
  ASSERT_LT(GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime), time_now);

  // Show the migration bubble.
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  views::Widget* widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);

  // The dialog shown count is incremented.
  ASSERT_EQ(GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 2);
  // The dialog last shown time is updated.
  ASSERT_GE(GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime), time_now);
  time_now = GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime);

  views::test::WidgetDestroyedWaiter waiter(widget);
  // Simulate clicking on accept button.
  widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  // The dialog shown count and time are not updated anymore.
  EXPECT_EQ(GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 2);
  EXPECT_EQ(GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime), time_now);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_DoNotShowDialogIfShownLessThanWeekAgo) {
  ImplicitlySignIn(kTestEmail);

  // Set the dialog last shown time to
  // (`kOfferMigrationToDiceUsersMinTimeBetweenDialogs` - 1) days ago.
  GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() -
           base::Days(1)));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       DoNotShowDialogIfShownLessThanWeekAgo) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, false, 1);

  histogram_tester_.ExpectUniqueSample(
      kDialogDaysSinceLastShownHistogram,
      (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() -
       base::Days(1))
          .InDays(),
      1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::
          kMinTimeBetweenDialogsNotPassed,
      1);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ShowDialogIfShownMoreThanAWeekAgo) {
  ImplicitlySignIn(kTestEmail);

  // Set the dialog last shown time to
  // (`kOfferMigrationToDiceUsersMinTimeBetweenDialogs` + 1) days ago.
  GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() +
           base::Days(1)));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       ShowDialogIfShownMoreThanAWeekAgo) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  EXPECT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram, true, 1);

  histogram_tester_.ExpectUniqueSample(
      kDialogDaysSinceLastShownHistogram,
      (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() +
       base::Days(1))
          .InDays(),
      1);
  histogram_tester_.ExpectTotalCount(kDialogNotShownReasonHistogram, 0);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest, ConsumerAccount) {
  // The account managed status is known.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kConsumerGmail);

  // Simulate the timer firing.
  FireDialogTriggerTimer();

  histogram_tester_.ExpectUniqueSample(
      kAccountManagedStatusHistogram,
      signin::AccountManagedStatusFinderOutcome::kConsumerGmail, 1);

  // The dialog is shown.
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogShownHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kDialogNotShownReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, PRE_EnterpriseAccount) {
  // Implicitly sign in with a known enterprise test account.
  ImplicitlySignIn(kEnterpriseTestEmail);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, EnterpriseAccount) {
  // The account managed status is known.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom);

  // Simulate the timer firing.
  FireDialogTriggerTimer();

  histogram_tester_.ExpectUniqueSample(
      kAccountManagedStatusHistogram,
      signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom, 1);

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectTotalCount(kDialogShownHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kManagedAccount, 1);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_TimerFinishedButAccountManagedStatusNotKnown) {
  // Implicitly sign in with a test account whose managed status is not known.
  ImplicitlySignIn(kIndeterminableTestEmail);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       TimerFinishedButAccountManagedStatusNotKnown) {
  // The account managed status is not known yet.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kPending);

  FireDialogTriggerTimer();

  histogram_tester_.ExpectUniqueSample(
      kAccountManagedStatusHistogram,
      signin::AccountManagedStatusFinderOutcome::kPending, 1);

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Simulate the account managed status becoming known when refresh tokens are
  // loaded.
  signin::AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
      "indeterminable.com");
  signin::SetRefreshTokenForPrimaryAccount(GetIdentityManager());

  // The dialog is now shown.
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      AccountManagedStatusKnownButTimerPending) {
  // The account managed status is known.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kConsumerGmail);

  // The dialog trigger timer is running.
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  histogram_tester_.ExpectTotalCount(kAccountManagedStatusHistogram, 0);

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Simulate the timer firing.
  FireDialogTriggerTimer();

  // The dialog is now shown.
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      StopTimerUponPersistentAuthError) {
  // The timer has started.
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Simulate a persistent auth error.
  signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager());

  // The timer is stopped.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kPrimaryAccountCleared, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      CloseDialogUponPersistentAuthError) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate a persistent auth error. This should cause the implicitly
  // signed-in account to be removed, thereby becoming similar to the case of
  // the user signing out.
  signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager());
  waiter.Wait();

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kPrimaryAccountCleared, 1);
}

// This can happen due to race condition between the timer firing and the dialog
// being closed.
DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      AcceptDialogAfterPersistentAuthError) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Simulate a persistent auth error.
  signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager());

  // The dialog is not destroyed yet due to the race condition.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  // No migration is performed.
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest, StopTimerUponSignout) {
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Sign out.
  signin::ClearPrimaryAccount(GetIdentityManager());

  // The timer is stopped.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kPrimaryAccountCleared, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest, CloseDialogUponSignout) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Sign out.
  signin::ClearPrimaryAccount(GetIdentityManager());
  waiter.Wait();

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kPrimaryAccountCleared, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      StopTimerUponPrimaryAccountChange) {
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Change the primary account.
  ImplicitlySignIn("test2@gmail.com");

  // The timer is stopped.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kPrimaryAccountChanged, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      CloseDialogUponPrimaryAccountChange) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Change the primary account.
  ImplicitlySignIn("test2@gmail.com");
  waiter.Wait();

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kPrimaryAccountChanged, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      StopTimerIfSyncTurnedOn) {
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Turn sync on.
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id,
      signin::ConsentLevel::kSync);

  // The timer is stopped.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kSyncTurnedOn, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      CloseDialogIfSyncTurnedOn) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Turn sync on.
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id,
      signin::ConsentLevel::kSync);
  waiter.Wait();

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kSyncTurnedOn, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      CloseDialogUponAvatarButtonPress) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Press the avatar button.
  GetAvatarToolbarButton()->ButtonPressed();
  waiter.Wait();

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kAvatarButtonClicked, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      PressingAvatarButtonBeforeDialogIsShown) {
  // Press the avatar button.
  GetAvatarToolbarButton()->ButtonPressed();

  // Show the migration bubble.
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  histogram_tester_.ExpectTotalCount(kDialogCloseReasonHistogram, 0);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      CloseDialogUponBrowserClose) {
  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Browser is closed.
  CloseBrowserAsynchronously(browser());
  waiter.Wait();

  histogram_tester_.ExpectUniqueSample(
      kDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kUnspecified, 1);
}

class DiceMigrationServiceSyncTest : public SyncTest {
 public:
  DiceMigrationServiceSyncTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kOfferMigrationToDiceUsers},
        /*disabled_features=*/{switches::kForcedDiceMigration});
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetProfile(0));
  }

  DiceMigrationService* GetDiceMigrationService() {
    DiceMigrationService* service =
        DiceMigrationServiceFactory::GetForProfileIfExists(GetProfile(0));
    EXPECT_TRUE(service);
    return service;
  }

  void TriggerDialog() {
    // This should allow account managed status to be known.
    signin::WaitForRefreshTokensLoaded(GetIdentityManager());
    // The account managed status is known.
    signin::AccountManagedStatusFinder account_managed_status_finder(
        GetIdentityManager(),
        GetIdentityManager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin),
        base::DoNothing());
    ASSERT_EQ(account_managed_status_finder.GetOutcome(),
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail);

    base::OneShotTimer& timer =
        GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
    ASSERT_TRUE(timer.IsRunning());
    timer.FireNow();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceSyncTest, PRE_MigrateUser) {
  ASSERT_TRUE(SetupClients());

  // Implicitly sign in.
  AccountInfo account_info = signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kTestEmail));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceSyncTest, MigrateUser) {
  constexpr syncer::UserSelectableTypeSet new_selected_types = {
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  };

  ASSERT_TRUE(SetupClients());

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(preferences_helper::GetPrefs(0)->GetBoolean(
      prefs::kExplicitBrowserSignin));

  // These types are only enabled upon explicitly signing in.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().HasAny(
      new_selected_types));
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().HasAny({
      syncer::PREFERENCES,
      syncer::THEMES,
      syncer::PASSWORDS,
      syncer::CONTACT_INFO,
  }));

  // Show migration bubble.
  TriggerDialog();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  EXPECT_TRUE(PrefValueChecker(preferences_helper::GetPrefs(0),
                               prefs::kExplicitBrowserSignin, base::Value(true))
                  .Wait());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // This should set the relevant user selected types.
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().HasAll(
      new_selected_types))
      << GetSyncService(0)->GetUserSettings()->GetSelectedTypes();

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().HasAll(
      {syncer::PREFERENCES, syncer::THEMES, syncer::PASSWORDS,
       syncer::CONTACT_INFO}))
      << GetSyncService(0)->GetActiveDataTypes();
}

class DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount
    : public DiceMigrationServiceBrowserTest,
      public testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    testing::Range(0, DiceMigrationService::kMaxDialogShownCount + 1));

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    PRE_LimitDialogShownCount) {
  ImplicitlySignIn(kTestEmail);
  GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, GetParam());
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    LimitDialogShownCount) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  ASSERT_EQ(GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), GetParam());

  // The timer is started only if the preconditions are met, i.e. the dialog
  // shown count is below the limit.
  const bool should_timer_be_running =
      GetParam() < DiceMigrationService::kMaxDialogShownCount;
  EXPECT_EQ(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning(),
      should_timer_be_running);
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogTimerStartedHistogram,
                                       should_timer_be_running, 1);
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    PRE_DialogVariants) {
  ImplicitlySignIn(kTestEmail);
  GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, GetParam());
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    DialogVariants) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  ASSERT_EQ(GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), GetParam());

  histogram_tester_.ExpectUniqueSample(kDialogPreviouslyShownCountHistogram,
                                       GetParam(), 1);

  // Show the migration bubble.
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();

  // Skip this test for `kMaxDialogShownCount` since no dialog is shown in this
  // case.
  if (GetParam() == DiceMigrationService::kMaxDialogShownCount) {
    ASSERT_FALSE(timer.IsRunning());
    return;
  }

  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  // Both the variants have the accept button.
  ASSERT_TRUE(ContainsViewWithId(dialog_widget->GetContentsView(),
                                 DiceMigrationService::kAcceptButtonElementId));

  if (GetParam() < DiceMigrationService::kMaxDialogShownCount - 1) {
    // Non-"final" variant has the cancel button but not the close-x button.
    EXPECT_TRUE(
        ContainsViewWithId(dialog_widget->GetRootView(),
                           DiceMigrationService::kCancelButtonElementId));
    EXPECT_FALSE(
        ContainsViewWithId(dialog_widget->GetRootView(),
                           views::BubbleFrameView::kCloseButtonElementId));
  } else {
    // "Final" variant has the close-x button but not the cancel button.
    EXPECT_FALSE(
        ContainsViewWithId(dialog_widget->GetRootView(),
                           DiceMigrationService::kCancelButtonElementId));
    EXPECT_TRUE(
        ContainsViewWithId(dialog_widget->GetRootView(),
                           views::BubbleFrameView::kCloseButtonElementId));
  }
}

class DiceMigrationServiceBrowserTestWithMockedTime
    : public DiceMigrationServiceBrowserTest {
 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    DiceMigrationServiceBrowserTest::SetUpBrowserContextKeyedServices(context);
    DiceMigrationServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&DiceMigrationServiceBrowserTestWithMockedTime::
                                CreateDiceMigrationServiceWithTaskRunner,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateDiceMigrationServiceWithTaskRunner(
      content::BrowserContext* context) {
    return std::make_unique<DiceMigrationService>(
        Profile::FromBrowserContext(context), task_runner_);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
};

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTestWithMockedTime,
                      ShowDialogBetweenRange) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // The timer is running, the dialog is not shown.
  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Fast forward to the minimum delay - 1 second. The timer is still running
  // and the dialog is not shown.
  task_runner_->FastForwardBy(
      switches::kOfferMigrationToDiceUsersMinDelay.Get() - base::Seconds(1));
  EXPECT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Fast forward to the maximum delay. The timer is stopped and the dialog is
  // shown.
  task_runner_->FastForwardBy(
      switches::kOfferMigrationToDiceUsersMaxDelay.Get() -
      switches::kOfferMigrationToDiceUsersMinDelay.Get() + base::Seconds(1));
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

class DiceMigrationServiceRestoreFromBackupBrowserTestFlagDisabled
    : public DiceMigrationServiceBrowserTest {
 public:
  DiceMigrationServiceRestoreFromBackupBrowserTestFlagDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        switches::kRollbackDiceMigration);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

DICE_MIGRATION_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagDisabled,
    SavePrefsForBackup) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Pre-migration prefs.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  ASSERT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(IsImplicitlySignedIn());
  ASSERT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  // The prefs are saved for backup.
  const base::Value* value = GetPrefs()->GetUserPrefValue(kDiceMigrationBackup);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  EXPECT_EQ(
      value->GetDict(),
      base::Value::Dict()
          .SetByDottedPath(prefs::kExplicitBrowserSignin, false)
          .SetByDottedPath(
              prefs::kPrefsThemesSearchEnginesAccountStorageEnabled, false));
}

class DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled
    : public DiceMigrationServiceBrowserTest {
 public:
  DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled() {
    // Only enable the rollback feature in the main test to allow testing the
    // rollback flow.
    scoped_feature_list_.InitWithFeatureState(switches::kRollbackDiceMigration,
                                              !content::IsPreTest());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    PRE_PRE_Restore) {
  ImplicitlySignIn(kTestEmail);
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    PRE_Restore) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Pre-migration prefs.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  // Only payments is selected for implicitly signed-in users, till the
  // kReplaceSyncPromosWithSignInPromos flag is enabled.
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    ASSERT_EQ(
        GetSyncService()->GetUserSettings()->GetSelectedTypes(),
        syncer::UserSelectableTypeSet({syncer::UserSelectableType::kPayments}));
  }

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);

  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  ASSERT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  ASSERT_TRUE(IsExplicitlySignedIn());
  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);

  // This should enable additional user selected types.
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll({
      syncer::UserSelectableType::kPayments,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  }));

  // The prefs are saved for backup.
  const base::Value* value = GetPrefs()->GetUserPrefValue(kDiceMigrationBackup);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  ASSERT_EQ(
      value->GetDict(),
      base::Value::Dict()
          .SetByDottedPath(prefs::kExplicitBrowserSignin, false)
          .SetByDottedPath(
              prefs::kPrefsThemesSearchEnginesAccountStorageEnabled, false));

  EXPECT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    Restore) {
  histogram_tester_.ExpectUniqueSample(kRestoredFromBackupHistogram, true, 1);

  // The user is implicitly signed in.
  EXPECT_TRUE(IsImplicitlySignedIn());

  // Prefs restored from backup.
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  EXPECT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  // Only payments is selected for implicitly signed-in users. None of the other
  // user selectable types are selected, unless the
  // kReplaceSyncPromosWithSignInPromos flag is enabled.
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    EXPECT_EQ(
        GetSyncService()->GetUserSettings()->GetSelectedTypes(),
        syncer::UserSelectableTypeSet({syncer::UserSelectableType::kPayments}));
  }

  // The timer is not running, the dialog is not shown.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Restoration pref is set.
  EXPECT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));
  // The migration pref is reset.
  EXPECT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  // The dialog shown count is reset.
  EXPECT_EQ(0, GetPrefs()->GetInteger(kDiceMigrationDialogShownCount));
  // The dialog last shown time is not cleared.
  EXPECT_FALSE(
      GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime).is_null());
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    PRE_PRE_RestoreFailed) {
  ImplicitlySignIn(kTestEmail);
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    PRE_RestoreFailed) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Pre-migration prefs.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  // Only payments is selected for implicitly signed-in users, unless
  // kReplaceSyncPromosWithSignInPromos flag is enabled.
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    ASSERT_EQ(
        GetSyncService()->GetUserSettings()->GetSelectedTypes(),
        syncer::UserSelectableTypeSet({syncer::UserSelectableType::kPayments}));
  }

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);

  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  ASSERT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  ASSERT_TRUE(IsExplicitlySignedIn());
  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);

  // This should enable additional user selected types.
  ASSERT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll({
      syncer::UserSelectableType::kPayments,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  }));

  // The prefs are saved for backup.
  const base::Value* value = GetPrefs()->GetUserPrefValue(kDiceMigrationBackup);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_dict());
  ASSERT_EQ(
      value->GetDict(),
      base::Value::Dict()
          .SetByDottedPath(prefs::kExplicitBrowserSignin, false)
          .SetByDottedPath(
              prefs::kPrefsThemesSearchEnginesAccountStorageEnabled, false));

  // Simulate backup pref missing.
  GetPrefs()->ClearPref(kDiceMigrationBackup);
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestFlagEnabled,
    RestoreFailed) {
  histogram_tester_.ExpectUniqueSample(kRestoredFromBackupHistogram, false, 1);

  // The user is implicitly signed in.
  EXPECT_TRUE(IsExplicitlySignedIn());

  // Prefs are not restored from backup.
  EXPECT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  EXPECT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  // The user selected types are unchanged.
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll({
      syncer::UserSelectableType::kPayments,
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  }));

  // The timer is not running, the dialog is not shown.
  ASSERT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Restoration pref is not set.
  EXPECT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));
  // The migration pref is not reset.
  EXPECT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
}

class DiceMigrationServiceRestoreFromBackupBrowserTestReMigration
    : public DiceMigrationServiceBrowserTest {
 public:
  DiceMigrationServiceRestoreFromBackupBrowserTestReMigration() {
    // Only enable the rollback feature for the last pre-test to allow DICe
    // migration to be tested in the main test.
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    // This calculates the number of times "PRE_" appears in the test name.
    int pre_count = 0;
    while (test_name.starts_with("PRE_")) {
      pre_count++;
      test_name.remove_prefix(4);
    }
    scoped_feature_list_.InitWithFeatureState(switches::kRollbackDiceMigration,
                                              pre_count == 1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestReMigration,
    PRE_PRE_PRE_PostRestore) {
  ImplicitlySignIn(kTestEmail);
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestReMigration,
    PRE_PRE_PostRestore) {
  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Pre-migration prefs.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  ASSERT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));

  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  ASSERT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  ASSERT_TRUE(IsExplicitlySignedIn());

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestReMigration,
    PRE_PostRestore) {
  histogram_tester_.ExpectUniqueSample(kRestoredFromBackupHistogram, true, 1);

  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Prefs restored from backup.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));
  ASSERT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));

  // Set the dialog last shown time to
  // (`kOfferMigrationToDiceUsersMinTimeBetweenDialogs` + 1) days ago to allow
  // the dialog to be shown again.
  GetProfile()->GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() +
           base::Days(1)));
}

IN_PROC_BROWSER_TEST_F(
    DiceMigrationServiceRestoreFromBackupBrowserTestReMigration,
    PostRestore) {
  histogram_tester_.ExpectTotalCount(kRestoredFromBackupHistogram, 0);

  // The user is implicitly signed in.
  ASSERT_TRUE(IsImplicitlySignedIn());

  // Prefs restored from backup.
  ASSERT_FALSE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));
  ASSERT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  FireDialogTriggerTimer();

  // The dialog is shown.
  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  ASSERT_TRUE(GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  ASSERT_FALSE(GetPrefs()->GetBoolean(kDiceMigrationRestoredFromBackup));

  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  ASSERT_TRUE(GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_TRUE(GetPrefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  ASSERT_TRUE(IsExplicitlySignedIn());

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);
  histogram_tester_.ExpectTotalCount(kRestoredFromBackupHistogram, 0);
}

// Regression test for crbug.com/437344538. The dialog should not be shown if
// the avatar button is not unavailable. This test reproduces it with a web
// app window, which has no avatar button.
class DiceMigrationServiceBrowserTestWithWebApps
    : public DiceMigrationServiceBrowserTest {
 public:
  void InstallAndLaunchWebApp() {
    web_app_frame_toolbar_helper_.InstallAndLaunchWebApp(
        browser(), GURL("https://test.org"));
  }

 private:
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTestWithWebApps,
                      NoDialogShown) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  InstallAndLaunchWebApp();

  ASSERT_TRUE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());

  // Show the migration dialog.
  FireDialogTriggerTimer();

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  histogram_tester_.ExpectUniqueSample(kDialogShownHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kDialogNotShownReasonHistogram,
      DiceMigrationService::DialogNotShownReason::kAvatarButtonUnavailable, 1);
}

class DiceMigrationServiceForcedMigrationBrowserTest
    : public DiceMigrationServiceBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kForcedDiceMigration};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       PRE_ForceMigrateImplicitlySignedInUser) {
  ImplicitlySignIn(kTestEmail);
  ASSERT_TRUE(IsImplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       ForceMigrateImplicitlySignedInUser) {
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // The user is signed in to the web only.
  signin::WaitForRefreshTokensLoaded(GetIdentityManager());
  EXPECT_THAT(
      GetIdentityManager()->GetAccountsWithRefreshTokens(),
      testing::ElementsAre(testing::Field(&AccountInfo::email, kTestEmail)));

  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(
      kSignoutReasonHistogram,
      signin_metrics::ProfileSignout::kForcedDiceMigration, 1);
  // No toast is shown.
  histogram_tester_.ExpectTotalCount(kToastTriggerToShowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       DoesNotMigrateSignedOutUser) {
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       PRE_DoesNotMigrateExplicitlySignedInUser) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSignin);
  ASSERT_TRUE(IsExplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       DoesNotMigrateExplicitlySignedInUser) {
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       PRE_DoesNotMigrateSyncingUser) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSync);
  // TODO(crbug.com/464457988): Mark sync setup as complete by default in the
  // sign-in helper method.
  GetSyncService()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  ASSERT_TRUE(GetSyncService()->IsSyncFeatureEnabled());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       DoesNotMigrateSyncingUser) {
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       PRE_EnterpriseAccountWithoutAccountManagement) {
  ImplicitlySignIn(kEnterpriseTestEmail);
  ASSERT_TRUE(IsImplicitlySignedIn());

  // The account managed status is known.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom);

  // The user has not accepted account management.
  ASSERT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       EnterpriseAccountWithoutAccountManagement) {
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // The user is signed in to the web only.
  signin::WaitForRefreshTokensLoaded(GetIdentityManager());
  EXPECT_THAT(GetIdentityManager()->GetAccountsWithRefreshTokens(),
              testing::ElementsAre(
                  testing::Field(&AccountInfo::email, kEnterpriseTestEmail)));

  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kForcedMigrationAccountManagedHistogram,
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      kSignoutReasonHistogram,
      signin_metrics::ProfileSignout::kForcedDiceMigration, 1);
  // No toast is shown.
  ASSERT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  histogram_tester_.ExpectTotalCount(kToastTriggerToShowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       PRE_EnterpriseAccountWithAccountManagement) {
  ImplicitlySignIn(kEnterpriseTestEmail);
  ASSERT_TRUE(IsImplicitlySignedIn());

  // The account managed status is known.
  signin::AccountManagedStatusFinder account_managed_status_finder(
      GetIdentityManager(),
      GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin),
      base::DoNothing());
  ASSERT_EQ(account_managed_status_finder.GetOutcome(),
            signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom);

  // The user has accepted account management.
  enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
  ASSERT_TRUE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationBrowserTest,
                       EnterpriseAccountWithAccountManagement) {
  // The user is explicitly signed in.
  EXPECT_TRUE(IsExplicitlySignedIn());
  // The user is signed in to the web.
  signin::WaitForRefreshTokensLoaded(GetIdentityManager());
  EXPECT_THAT(GetIdentityManager()->GetAccountsWithRefreshTokens(),
              testing::ElementsAre(
                  testing::Field(&AccountInfo::email, kEnterpriseTestEmail)));

  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kForcedMigrationAccountManagedHistogram,
                                       true, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
  // The toast is shown after the timer finishes. However, it is possible that
  // the timer has already finished and the toast is shown before the
  // expectation below is checked.
  if (GetDiceMigrationService()
          ->GetDialogTriggerTimerForTesting()
          .IsRunning()) {
    histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                         ToastId::kDiceUserMigrated, 0);
    FireDialogTriggerTimer();
  }
  histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                       ToastId::kDiceUserMigrated, 1);
}

}  // namespace
