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
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
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

  void FireDialogTriggerTimer() {
    base::OneShotTimer& timer =
        GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
    ASSERT_TRUE(timer.IsRunning());
    timer.FireNow();
  }

  Profile* GetProfile() { return browser()->profile(); }

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
  const base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
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
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ExplicitlySignedIn) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), GetProfile()->GetPrefs()));

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
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ImplicitlySignedIn) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), GetProfile()->GetPrefs()));

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
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the explicit sign-in pref to true. This should make the user ineligible
  // for the migration, but the timer still runs. This is a test-only scenario
  // and should not happen in production.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);
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
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

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

  // The explicit sign-in pref is set, this marks the user as explicitly
  // signed in.
  EXPECT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // This should set the relevant user selected types.
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll(
      new_selected_types))
      << GetSyncService()->GetUserSettings()->GetSelectedTypes();

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, true, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      ShouldNotMigrateUserIfIneligible) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);

  // Turn sync on.
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id,
      signin::ConsentLevel::kSync);

  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on the accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  // The explicit sign-in pref is not set because a syncing user is not
  // eligible.
  EXPECT_EQ(GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin),
            false);

  histogram_tester_.ExpectUniqueSample(kUserMigratedHistogram, false, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      IncrementDialogShownCount) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the current dialog shown count to 1.
  GetProfile()->GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, 1);

  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);

  // The dialog shown count is not incremented yet.
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 1);

  views::test::WidgetDestroyedWaiter waiter(widget);
  // Simulate closing the dialog.
  GetDiceMigrationService()->GetDialogWidgetForTesting()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
  waiter.Wait();

  // The dialog shown count is now incremented.
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 2);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      DoNotIncrementDialogShownCountIfNotInteractedWith) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the current dialog shown count to 1.
  GetProfile()->GetPrefs()->SetInteger(kDiceMigrationDialogShownCount, 1);

  // Show the migration bubble.
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  views::Widget* widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);

  // The dialog shown count is not incremented yet.
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 1);

  views::test::WidgetDestroyedWaiter waiter(widget);
  // Simulate the dialog being closed without any user interaction.
  signin::ClearPrimaryAccount(GetIdentityManager());
  waiter.Wait();

  // The dialog shown count is not incremented.
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      UpdateDialogLastShownTime) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  base::Time time_now = base::Time::Now();
  ASSERT_LT(
      GetProfile()->GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime),
      time_now);

  // Not logged since the dialog was never shown before.
  histogram_tester_.ExpectTotalCount(kDialogDaysSinceLastShownHistogram, 0);

  // Show the migration bubble.
  FireDialogTriggerTimer();

  views::Widget* widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(widget);

  // The dialog last shown time is not updated yet.
  EXPECT_LT(
      GetProfile()->GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime),
      time_now);

  // Simulate closing the dialog.
  views::test::WidgetDestroyedWaiter waiter(widget);
  GetDiceMigrationService()->GetDialogWidgetForTesting()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
  waiter.Wait();

  // The dialog last shown time is now updated.
  EXPECT_GE(
      GetProfile()->GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime),
      time_now);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_DoNotShowDialogIfShownLessThanWeekAgo) {
  ImplicitlySignIn(kTestEmail);

  // Set the dialog last shown time to
  // (`kOfferMigrationToDiceUsersMinTimeBetweenDialogs` - 1) days ago.
  GetProfile()->GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() -
           base::Days(1)));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       DoNotShowDialogIfShownLessThanWeekAgo) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

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
  GetProfile()->GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get() +
           base::Days(1)));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       ShowDialogIfShownMoreThanAWeekAgo) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

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
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
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
  DiceMigrationServiceSyncTest() : SyncTest(SINGLE_CLIENT) {}

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
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
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
  GetProfile()->GetPrefs()->SetInteger(kDiceMigrationDialogShownCount,
                                       GetParam());
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    LimitDialogShownCount) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  ASSERT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount),
      GetParam());

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
  GetProfile()->GetPrefs()->SetInteger(kDiceMigrationDialogShownCount,
                                       GetParam());
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    DialogVariants) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  ASSERT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount),
      GetParam());

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
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

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

}  // namespace
