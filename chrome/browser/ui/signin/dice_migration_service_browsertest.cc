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

namespace {
constexpr char kTestEmail[] = "test@gmail.com";

class DiceMigrationServiceBrowserTest : public SyncTest {
 public:
  DiceMigrationServiceBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  DiceMigrationService* GetDiceMigrationService() {
    return DiceMigrationServiceFactory::GetForProfile(GetProfile(0));
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetProfile(0));
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

  signin::MakePrimaryAccountAvailable(GetIdentityManager(), kTestEmail,
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
          .Build(kTestEmail));

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(signin::IsImplicitBrowserSigninOrExplicitDisabled(
      GetIdentityManager(), preferences_helper::GetPrefs(0)));

  GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
  EXPECT_TRUE(GetDiceMigrationService()->IsDialogShowing());
}

}  // namespace
