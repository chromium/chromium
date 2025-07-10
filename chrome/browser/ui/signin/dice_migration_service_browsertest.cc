// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"

namespace {

class DiceMigrationServiceBrowserTest : public SyncTest {
 public:
  DiceMigrationServiceBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  DiceMigrationService* GetDiceMigrationService() {
    return DiceMigrationServiceFactory::GetForProfile(GetProfile(0));
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetProfile(0));
  }

  std::string GetAccountEmail() const {
    return GetClient(0)->GetEmailForAccount(SyncTestAccount::kDefaultAccount);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, NotSignedIn) {
  ASSERT_TRUE(SetupClients());

  // The user is not signed in.
  ASSERT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
  EXPECT_FALSE(GetDiceMigrationService()->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, Syncing) {
  ASSERT_TRUE(SetupSync());

  // The user is syncing.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
  EXPECT_FALSE(GetDiceMigrationService()->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ExplicitlySignedIn) {
  ASSERT_TRUE(SetupClients());

  signin::MakePrimaryAccountAvailable(GetIdentityManager(), GetAccountEmail(),
                                      signin::ConsentLevel::kSignin);

  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), preferences_helper::GetPrefs(0)));

  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
  EXPECT_FALSE(GetDiceMigrationService()->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, ImplicitlySignedIn) {
  ASSERT_TRUE(SetupClients());

  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(GetAccountEmail()));

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), preferences_helper::GetPrefs(0)));

  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
  EXPECT_TRUE(GetDiceMigrationService()->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest, MigrateUser) {
  constexpr syncer::UserSelectableTypeSet new_selected_types = {
      syncer::UserSelectableType::kPreferences,
      syncer::UserSelectableType::kThemes,
      syncer::UserSelectableType::kPasswords,
      syncer::UserSelectableType::kAutofill,
  };

  ASSERT_TRUE(SetupClients());

  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(GetAccountEmail()));

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
  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();

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

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       ShouldNotMigrateUserIfIneligible) {
  ASSERT_TRUE(SetupClients());

  signin::MakeAccountAvailable(
      GetIdentityManager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `kWebSignin` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(GetAccountEmail()));

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(preferences_helper::GetPrefs(0)->GetBoolean(
      prefs::kExplicitBrowserSignin));

  // Show the migration bubble.
  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();

  // Turn sync on.
  ASSERT_TRUE(SetupSync());

  views::Widget* dialog_widget =
      GetDiceMigrationService()->GetDialogWidgetForTesting();
  ASSERT_TRUE(dialog_widget);
  views::test::WidgetDestroyedWaiter waiter(dialog_widget);
  // Simulate clicking on the accept button.
  dialog_widget->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);
  waiter.Wait();

  // The explicit sign-in pref is not set because a syncing user is not
  // eligible.
  EXPECT_EQ(preferences_helper::GetPrefs(0)->GetBoolean(
                prefs::kExplicitBrowserSignin),
            false);
}

}  // namespace
