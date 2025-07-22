// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of `DiceMigrationServiceBrowserTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)    \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) { \
    ImplicitlySignIn();                                 \
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
  void ImplicitlySignIn() {
    AccountInfo account_info = signin::MakeAccountAvailable(
        GetIdentityManager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
            .Build(kTestEmail));
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, NotSignedIn) {
  // The user is not signed in.
  ASSERT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The timer to trigger the dialog is not started.
  EXPECT_FALSE(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning());
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
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
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ImplicitlySignedIn) {
  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kTestEmail));
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

  // Trigger the timer.
  GetDiceMigrationService()->GetDialogTriggerTimerForTesting().FireNow();
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
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
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);
  // Simulate clicking on accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  EXPECT_TRUE(PrefValueChecker(GetProfile()->GetPrefs(),
                               prefs::kExplicitBrowserSignin, base::Value(true))
                  .Wait());

  // This should set the relevant user selected types.
  EXPECT_TRUE(GetSyncService()->GetUserSettings()->GetSelectedTypes().HasAll(
      new_selected_types))
      << GetSyncService()->GetUserSettings()->GetSelectedTypes();
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceBrowserTest,
                      ShouldNotMigrateUserIfIneligible) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Show the migration bubble.
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

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
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();
  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // The dialog shown count is incremented.
  EXPECT_EQ(
      GetProfile()->GetPrefs()->GetInteger(kDiceMigrationDialogShownCount), 2);
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

  // Show the migration bubble.
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  // The dialog last shown time is updated.
  EXPECT_GE(
      GetProfile()->GetPrefs()->GetTime(kDiceMigrationDialogLastShownTime),
      time_now);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_DoNotShowDialogIfShownLessThanWeekAgo) {
  ImplicitlySignIn();

  // Set the dialog last shown time to (`kMinTimeBetweenDialogInDays` - 1) days
  // ago.
  GetProfile()->GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (DiceMigrationService::kMinTimeBetweenDialogInDays - base::Days(1)));
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
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ShowDialogIfShownMoreThanAWeekAgo) {
  ImplicitlySignIn();

  // Set the dialog last shown time to (`kMinTimeBetweenDialogInDays` + 1) days
  // ago.
  GetProfile()->GetPrefs()->SetTime(
      kDiceMigrationDialogLastShownTime,
      base::Time::Now() -
          (DiceMigrationService::kMinTimeBetweenDialogInDays + base::Days(1)));
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
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  // The dialog is shown.
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, PRE_EnterpriseAccount) {
  // Implicitly sign in with a known enterprise test account.
  AccountInfo account_info = signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kEnterpriseTestEmail));
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
  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_TimerFinishedButAccountManagedStatusNotKnown) {
  // Implicitly sign in with a test account whose managed status is not known.
  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kIndeterminableTestEmail));
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

  base::OneShotTimer& timer =
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting();
  ASSERT_TRUE(timer.IsRunning());
  timer.FireNow();

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

  // The dialog is not shown.
  EXPECT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());

  // Simulate the timer firing.
  GetDiceMigrationService()->GetDialogTriggerTimerForTesting().FireNow();

  // The dialog is now shown.
  EXPECT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
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
  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kTestEmail));
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
  EXPECT_EQ(
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().IsRunning(),
      GetParam() < DiceMigrationService::kMaxDialogShownCount);
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_P(
    DiceMigrationServiceBrowserTestWithParameterizedDialogShownCount,
    PRE_DialogVariants) {
  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kTestEmail));
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

}  // namespace
