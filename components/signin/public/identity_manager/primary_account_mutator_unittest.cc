// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_mutator.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/platform_test.h"

using signin::ConsentLevel;

namespace {

// Constants used by the different tests.
const char kPrimaryAccountEmail[] = "primary.account@example.com";
const char kAnotherAccountEmail[] = "another.account@example.com";
#if !BUILDFLAG(IS_CHROMEOS_ASH)
const char kUnknownAccountId[] = "{unknown account id}";
#endif

// See RunRevokeConsentTest().
enum class RevokeConsentAction { kRevokeSyncConsent, kClearPrimaryAccount };
enum class AuthExpectation { kAuthNormal, kAuthError };
enum class RemoveAccountExpectation { kKeepAll, kRemoveAll };

// This callback will be invoked every time the IdentityManager::Observer
// method OnPrimaryAccountCleared is invoked. The parameter will be a
// reference to the still valid primary account that was cleared.
using PrimaryAccountClearedCallback =
    base::RepeatingCallback<void(const CoreAccountInfo&)>;

// This callback will be invoked every time the IdentityManager::Observer
// method OnRefreshTokenRemoved is invoked. The parameter will be a reference
// to the account_id whose token was removed.
using RefreshTokenRemovedCallback =
    base::RepeatingCallback<void(const CoreAccountId&)>;

// Helper IdentityManager::Observer that forwards some events to the
// callback passed to the constructor.
class ClearPrimaryAccountTestObserver
    : public signin::IdentityManager::Observer {
 public:
  ClearPrimaryAccountTestObserver(
      signin::IdentityManager* identity_manager,
      PrimaryAccountClearedCallback on_primary_account_cleared,
      RefreshTokenRemovedCallback on_refresh_token_removed)
      : on_primary_account_cleared_(std::move(on_primary_account_cleared)),
        on_refresh_token_removed_(std::move(on_refresh_token_removed)),
        scoped_observation_(this) {
    DCHECK(on_primary_account_cleared_);
    DCHECK(on_refresh_token_removed_);
    scoped_observation_.Observe(identity_manager);
  }

  ClearPrimaryAccountTestObserver(const ClearPrimaryAccountTestObserver&) =
      delete;
  ClearPrimaryAccountTestObserver& operator=(
      const ClearPrimaryAccountTestObserver&) = delete;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
    if (event.GetEventTypeFor(signin::ConsentLevel::kSync) !=
        signin::PrimaryAccountChangeEvent::Type::kCleared) {
      return;
    }
    on_primary_account_cleared_.Run(event.GetPreviousState().primary_account);
  }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    on_refresh_token_removed_.Run(account_id);
  }

 private:
  PrimaryAccountClearedCallback on_primary_account_cleared_;
  RefreshTokenRemovedCallback on_refresh_token_removed_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

// Helper for testing of RevokeSyncConsent/ClearPrimaryAccount(). This function
// requires lots of tests due to having different behaviors based on its
// arguments. But the setup and execution of these test is all the boiler plate
// you see here:
// 1) Ensure you have 2 accounts, both with refresh tokens
// 2) Clear the primary account
// 3) Assert clearing succeeds and refresh tokens are optionally removed based
//    on arguments.
//
// Optionally, it's possible to specify whether a normal auth process will
// take place, or whether an auth error should happen, useful for some tests.
void RunRevokeConsentTest(
    RevokeConsentAction action,
    signin::AccountConsistencyMethod account_consistency_method,
    RemoveAccountExpectation account_expectation,
    AuthExpectation auth_expection = AuthExpectation::kAuthNormal) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment(
      /*test_url_loader_factory=*/nullptr, /*pref_service=*/nullptr,
      account_consistency_method);

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // With the exception of ClearPrimaryAccount_AuthInProgress, every other
  // ClearPrimaryAccount_* test requires a primary account to be signed in.
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          account_info.account_id, signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
            setPrimaryAccountResult);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  EXPECT_EQ(identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
            account_info.account_id);
  EXPECT_EQ(identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                .email,
            kPrimaryAccountEmail);

  if (auth_expection == AuthExpectation::kAuthError) {
    // Set primary account to have authentication error.
    SetRefreshTokenForPrimaryAccount(identity_manager);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager, account_info.account_id,
        GoogleServiceAuthError(
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  }

  // Additionally, ClearPrimaryAccount_* tests also need a secondary account.
  AccountInfo secondary_account_info =
      environment.MakeAccountAvailable(kAnotherAccountEmail);
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.account_id));

  // Grab this before clearing for token checks below.
  auto former_primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);

  // Make sure we exit the run loop.
  base::RunLoop run_loop;
  PrimaryAccountClearedCallback primary_account_cleared_callback =
      base::BindRepeating([](base::RepeatingClosure quit_closure,
                             const CoreAccountInfo&) { quit_closure.Run(); },
                          run_loop.QuitClosure());

  // Track Observer token removal notification.
  base::flat_set<CoreAccountId> observed_removals;
  RefreshTokenRemovedCallback refresh_token_removed_callback =
      base::BindRepeating(
          [](base::flat_set<CoreAccountId>* observed_removals,
             const CoreAccountId& removed_account) {
            observed_removals->insert(removed_account);
          },
          &observed_removals);

  ClearPrimaryAccountTestObserver scoped_observation(
      identity_manager, primary_account_cleared_callback,
      refresh_token_removed_callback);
  switch (action) {
    case RevokeConsentAction::kRevokeSyncConsent:
      primary_account_mutator->RevokeSyncConsent(
          signin_metrics::ProfileSignout::kTest,
          signin_metrics::SignoutDelete::kIgnoreMetric);
      break;
    case RevokeConsentAction::kClearPrimaryAccount:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      NOTREACHED();
#else
      primary_account_mutator->ClearPrimaryAccount(
          signin_metrics::ProfileSignout::kTest,
          signin_metrics::SignoutDelete::kIgnoreMetric);
      break;
#endif
  }
  run_loop.Run();

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // NOTE: IdentityManager _may_ still possess this token (see switch below),
  // but it is no longer considered part of the primary account.
  EXPECT_FALSE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  switch (account_expectation) {
    case RemoveAccountExpectation::kKeepAll:
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(observed_removals.empty());
      break;
    case RemoveAccountExpectation::kRemoveAll:
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(
          base::Contains(observed_removals, former_primary_account.account_id));
      EXPECT_TRUE(
          base::Contains(observed_removals, secondary_account_info.account_id));
      break;
  }
}

void RunRevokeSyncConsentTest(
    signin::AccountConsistencyMethod account_consistency_method,
    RemoveAccountExpectation account_expectation,
    AuthExpectation auth_expection = AuthExpectation::kAuthNormal) {
  RunRevokeConsentTest(RevokeConsentAction::kRevokeSyncConsent,
                       account_consistency_method, account_expectation,
                       auth_expection);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void RunClearPrimaryAccountTest(
    signin::AccountConsistencyMethod account_consistency_method) {
  RunRevokeConsentTest(
      RevokeConsentAction::kClearPrimaryAccount, account_consistency_method,
      RemoveAccountExpectation::kRemoveAll, AuthExpectation::kAuthNormal);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

using PrimaryAccountMutatorTest = PlatformTest;

// Checks that setting the primary account works.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment;

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(environment.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          account_info.account_id, signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
            setPrimaryAccountResult);

  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_EQ(identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
            account_info.account_id);
}

// Tests that various preconditions of SetPrimaryAccount() not being satisfied
// should cause the setting of the primary account to fail. Not run on
// ChromeOS, where those preconditions do not exist.
// TODO(https://crbug.com/983124): Run these tests on ChromeOS if/once we
// enable those preconditions on that platform
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Checks that setting the primary account fails if the account is not known by
// the identity system.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_NoAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment;

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          CoreAccountId(kUnknownAccountId), signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(
      signin::PrimaryAccountMutator::PrimaryAccountError::kAccountInfoEmpty,
      setPrimaryAccountResult);
}

// Checks that setting the primary account fails if the account is unknown.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_UnknownAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment;

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          CoreAccountId(kUnknownAccountId), signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(
      signin::PrimaryAccountMutator::PrimaryAccountError::kAccountInfoEmpty,
      setPrimaryAccountResult);
}

// Checks that trying to set the primary account fails when there is already a
// primary account.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_AlreadyHasPrimaryAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment;

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo another_account_info =
      environment.MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          primary_account_info.account_id, signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::kNoError,
            setPrimaryAccountResult);

  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  setPrimaryAccountResult = primary_account_mutator->SetPrimaryAccount(
      another_account_info.account_id, signin::ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(signin::PrimaryAccountMutator::PrimaryAccountError::
                kSyncConsentAlreadySet,
            setPrimaryAccountResult);

  EXPECT_EQ(identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
            primary_account_info.account_id);
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Checks that trying to set the primary account fails if setting the primary
// account is not allowed.
TEST_F(PrimaryAccountMutatorTest,
       SetPrimaryAccount_SettingPrimaryAccountForbidden) {
  base::test::TaskEnvironment task_environment;

  sync_preferences::TestingPrefServiceSyncable pref_service;
  signin::IdentityTestEnvironment environment(
      /*test_url_loader_factory=*/nullptr, &pref_service);

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  // Configure prefs so that setting the primary account is disallowed.
  pref_service.SetBoolean(prefs::kSigninAllowed, false);

  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  signin::PrimaryAccountMutator::PrimaryAccountError setPrimaryAccountResult =
      primary_account_mutator->SetPrimaryAccount(
          primary_account_info.account_id, signin::ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  EXPECT_EQ(
      signin::PrimaryAccountMutator::PrimaryAccountError::kSigninNotAllowed,
      setPrimaryAccountResult);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// End of tests of preconditions not being satisfied causing the setting of
// the primary account to fail.

// Tests of clearing the primary account. Not run on ChromeOS, which does not
// support clearing the primary account.
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_NotSignedIn) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment environment;

  signin::IdentityManager* identity_manager = environment.identity_manager();
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // Trying to signout an account that hasn't signed in first should fail.
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(primary_account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric));
}

// Test that ClearPrimaryAccount() clears the primary account, revokes the
// sync consent and removes all accounts, independently of the account
// consistency method.
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount) {
  const signin::AccountConsistencyMethod kTestedAccountConsistencyMethods[] = {
      signin::AccountConsistencyMethod::kDisabled,
      signin::AccountConsistencyMethod::kMirror,
      signin::AccountConsistencyMethod::kDice,
  };
  for (signin::AccountConsistencyMethod account_consistency_method :
       kTestedAccountConsistencyMethods) {
    RunClearPrimaryAccountTest(account_consistency_method);
  }
}

// Test that revoking the sync consent when account consistency is disabled
// also clears the primary account and removes all accounts.
TEST_F(PrimaryAccountMutatorTest, RevokeSyncConsent_DisabledConsistency) {
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kDisabled,
                           RemoveAccountExpectation::kRemoveAll);
}

// Test that revoking sync consent when Mirror account consistency is enabled
// clears the primary account (except for lacros and Android, where users are
// allowed to revoke sync consent).
TEST_F(PrimaryAccountMutatorTest, RevokeSyncConsent_MirrorConsistency) {
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kMirror,
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_ANDROID)
                           RemoveAccountExpectation::kKeepAll
#else
                           RemoveAccountExpectation::kRemoveAll
#endif
  );
}

// Test that revoking the sync consent when DICE account consistency is
// enabled does not clear the primary account.
TEST_F(PrimaryAccountMutatorTest, RevokeSyncConsent_DiceConsistency) {
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kDice,
                           RemoveAccountExpectation::kKeepAll);
}

#else  //! BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PrimaryAccountMutatorTest, CROS_ASH_RevokeSyncConsent) {
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kDisabled,
                           RemoveAccountExpectation::kKeepAll);
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kMirror,
                           RemoveAccountExpectation::kKeepAll);
}

TEST_F(PrimaryAccountMutatorTest, CROS_ASH_RevokeSyncConsent_AuthError) {
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kDisabled,
                           RemoveAccountExpectation::kKeepAll,
                           AuthExpectation::kAuthError);
  RunRevokeSyncConsentTest(signin::AccountConsistencyMethod::kMirror,
                           RemoveAccountExpectation::kKeepAll,
                           AuthExpectation::kAuthError);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
