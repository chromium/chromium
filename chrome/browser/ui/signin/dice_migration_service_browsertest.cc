// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kTestEmail[] = "test@gmail.com";
constexpr char kEnterpriseTestEmail[] = "test@google.com";

constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";
constexpr char kSignoutReasonHistogram[] = "Signin.SignOut.Completed";
constexpr char kToastTriggerToShowHistogram[] = "Toast.TriggeredToShow";
constexpr char kForcedMigrationAccountManagedHistogram[] =
    "Signin.ForcedDiceMigration.HasAcceptedAccountManagement";

class DiceMigrationServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            GetProfile());
  }

  void ImplicitlySignIn(const std::string& email) {
    signin::MakeAccountAvailable(
        GetIdentityManager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
            .Build(email));
  }

  bool IsImplicitlySignedIn() {
    return GetIdentityManager()->HasPrimaryAccount(
               signin::ConsentLevel::kSignin) &&
           !GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin);
  }

  bool IsExplicitlySignedIn() {
    return GetIdentityManager()->HasPrimaryAccount(
               signin::ConsentLevel::kSignin) &&
           GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin);
  }

  void FireToastTriggerTimer() {
    base::OneShotTimer& timer =
        GetDiceMigrationService()->GetToastTriggerTimerForTesting();
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

 protected:
  base::ScopedClosureRunner disclaimer_service_resetter_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_ForceMigrateImplicitlySignedInUser) {
  ImplicitlySignIn(kTestEmail);
  ASSERT_TRUE(IsImplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       DoesNotMigrateSignedOutUser) {
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_DoesNotMigrateExplicitlySignedInUser) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSignin);
  ASSERT_TRUE(IsExplicitlySignedIn());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       DoesNotMigrateExplicitlySignedInUser) {
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       PRE_DoesNotMigrateSyncingUser) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
                                      signin::ConsentLevel::kSync);
  // TODO(crbug.com/464457988): Mark sync setup as complete by default in the
  // sign-in helper method.
  GetSyncService()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  ASSERT_TRUE(GetSyncService()->IsSyncFeatureEnabled());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
                       DoesNotMigrateSyncingUser) {
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(kSignoutReasonHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
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
      GetDiceMigrationService()->GetToastTriggerTimerForTesting().IsRunning());
  histogram_tester_.ExpectTotalCount(kToastTriggerToShowHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceBrowserTest,
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
  if (GetDiceMigrationService()->GetToastTriggerTimerForTesting().IsRunning()) {
    histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                         ToastId::kDiceUserMigrated, 0);
    FireToastTriggerTimer();
  }
  histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                       ToastId::kDiceUserMigrated, 1);
}

}  // namespace
