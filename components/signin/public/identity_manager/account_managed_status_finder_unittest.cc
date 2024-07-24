// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_managed_status_finder.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(AccountManagedStatusFinderStaticTest, MayBeEnterpriseDomain) {
  // List of example domains that are well-known to not be enterprise domains.
  // As a special case, this includes the empty string (which is often output
  // by extract-domain-from-email helpers if the email is invalid).
  static const char* kNonEnterpriseDomains[] = {
      "aol.com",     "gmail.com",     "googlemail.com",
      "hotmail.it",  "hotmail.co.uk", "hotmail.fr",
      "msn.com",     "live.com",      "qq.com",
      "yahoo.com",   "yahoo.com.tw",  "yahoo.fr",
      "yahoo.co.uk", "yandex.ru",     ""};

  // List of example domains that are potential enterprise domains.
  static const char* kPotentialEnterpriseDomains[] = {
      "google.com",
      "chromium.org",
      "hotmail.enterprise.com",
      "unknown-domain.asdf",
  };

  for (const char* username : kNonEnterpriseDomains) {
    EXPECT_FALSE(AccountManagedStatusFinder::MayBeEnterpriseDomain(username))
        << username;
  }
  for (const char* username : kPotentialEnterpriseDomains) {
    EXPECT_TRUE(AccountManagedStatusFinder::MayBeEnterpriseDomain(username))
        << username;
  }
}

TEST(AccountManagedStatusFinderStaticTest, MayBeEnterpriseUserBasedOnEmail) {
  // List of example emails that are well-known to not be enterprise users. This
  // includes some invalid of malformed emails.
  // clang-format off
  static const char* kNonEnterpriseUsers[] = {
      "fizz@aol.com",       "foo@gmail.com",         "bar@googlemail.com",
      "baz@hotmail.it",     "baz@hotmail.co.uk",     "baz@hotmail.fr",
      "user@msn.com",       "another_user@live.com", "foo@qq.com",
      "i_love@yahoo.com",   "i_love@yahoo.com.tw",   "i_love@yahoo.fr",
      "i_love@yahoo.co.uk", "user@yandex.ru",        "test",
      "test@", ""};
  // clang-format on

  // List of example emails that are potential enterprise users.
  static const char* kPotentialEnterpriseUsers[] = {
      "foo@google.com",
      "chrome_rules@chromium.org",
      "user@hotmail.enterprise.com",
      "user@unknown-domain.asdf",
  };

  for (const char* username : kNonEnterpriseUsers) {
    EXPECT_FALSE(
        AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(username))
        << username;
  }
  for (const char* username : kPotentialEnterpriseUsers) {
    EXPECT_TRUE(
        AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(username))
        << username;
  }
}

class AccountManagedStatusFinderTest : public testing::Test {
 public:
  AccountManagedStatusFinderTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_env_;
};

TEST_F(AccountManagedStatusFinderTest, GmailAccountDeterminedImmediately) {
  AccountInfo account = identity_env_.MakeAccountAvailable("account@gmail.com");

  // Simple case: An @gmail.com account should be immediately determined as
  // non-enterprise.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerGmail);
}

TEST_F(AccountManagedStatusFinderTest, GooglemailAccountDeterminedImmediately) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@googlemail.com");

  // Simple case: An @googlemail.com account should be immediately determined as
  // non-enterprise.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerGmail);
}

TEST_F(AccountManagedStatusFinderTest,
       WellKnownConsumerAccountDeterminedImmediately) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@hotmail.com");

  // An account from a well-known consumer domain should be immediately
  // determined as non-enterprise.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerWellKnown);
}

TEST_F(AccountManagedStatusFinderTest,
       GoogleDotComAccountDeterminedImmediately) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@google.com");

  // Special case: An @google.com account should be immediately identified.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom);
}

TEST_F(AccountManagedStatusFinderTest, EnterpriseAccountDeterminedImmediately) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");

  // The full account info is already available before the
  // AccountManagedStatusFinder gets created.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  // The AccountManagedStatusFinder should be able to immediately identify the
  // enterprise account based on the account info.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kEnterprise);
}

TEST_F(AccountManagedStatusFinderTest,
       NonEnterpriseAccountDeterminedImmediately) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@not-an-enterprise.com");

  // The full account info is already available before the
  // AccountManagedStatusFinder gets created.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  // The AccountManagedStatusFinder should be able to immediately identify the
  // non-enterprise account based on the account info.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown);
}

TEST_F(AccountManagedStatusFinderTest,
       EnterpriseAccountDeterminedAsynchronously) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");

  // Since the extended account info is not available yet, the enterprise
  // account can not be identified immediately - it's only a potential
  // enterprise account for now, so the outcome is still pending.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Once the extended account info becomes available, the
  // AccountManagedStatusFinder should determine that it's an enterprise
  // account (because it has a non-empty hosted domain).
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kEnterprise);
}

TEST_F(AccountManagedStatusFinderTest,
       NonEnterpriseAccountDeterminedAsynchronously) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@not-an-enterprise.com");

  // An account from an unknown domain can not be identified immediately, so the
  // outcome is pending.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Once the extended account info becomes available, the
  // AccountManagedStatusFinder should determine that it's *not* an enterprise
  // account (because the hosted domain is empty).
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown);
}

TEST_F(AccountManagedStatusFinderTest, KeepsWaitingOnPartialAccountInfoUpdate) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@not-an-enterprise.com");

  // An account from an unknown domain can not be identified immediately, so the
  // outcome is pending.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  ASSERT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // The AccountInfo gets updated partially, but it's not known yet whether it's
  // an enterprise account (because the hosted domain remains unset). The
  // AccountManagedStatusFinder should keep waiting.
  EXPECT_CALL(outcome_determined, Run).Times(0);
  account.picture_url = "https://account.picture.url";
  identity_env_.UpdateAccountInfoForAccount(account);
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Once the full extended account info becomes available, the
  // AccountManagedStatusFinder should determine the account type.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown);
}

TEST_F(AccountManagedStatusFinderTest,
       KeepsWaitingOnDifferentAccountInfoUpdate) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@not-an-enterprise.com");
  AccountInfo different_account =
      identity_env_.MakeAccountAvailable("different@not-an-enterprise.com");

  // An account from an unknown domain can not be identified immediately, so the
  // outcome is pending.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  ASSERT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // The AccountInfo for a different account becomes available. The
  // AccountManagedStatusFinder should ignore this irrelevant update.
  EXPECT_CALL(outcome_determined, Run).Times(0);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      different_account.account_id, different_account.email,
      different_account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Similarly, the AccountManagedStatusFinder should ignore if the different
  // account gets removed.
  EXPECT_CALL(outcome_determined, Run).Times(0);
  identity_env_.RemoveRefreshTokenForAccount(different_account.account_id);
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Once the full extended account info for the interesting account becomes
  // available, the AccountManagedStatusFinder should determine the account
  // type.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown);
}

TEST_F(AccountManagedStatusFinderTest, ErrorOnNonExistentAccount) {
  AccountInfo account = identity_env_.MakeAccountAvailable("account@gmail.com");

  // The account gets removed before the AccountManagedStatusFinder is created.
  identity_env_.RemoveRefreshTokenForAccount(account.account_id);

  // The AccountManagedStatusFinder should detect this and immediately report an
  // error.
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    base::DoNothing());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kError);
}

TEST_F(AccountManagedStatusFinderTest, ErrorOnAccountRemoved) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");

  // The account exists at the time the AccountManagedStatusFinder is created,
  // but its status is not known yet.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  ASSERT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Before the status can be determined, the account gets removed.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.RemoveRefreshTokenForAccount(account.account_id);

  // The AccountManagedStatusFinder should detect this and report an error.
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kError);
}

TEST_F(AccountManagedStatusFinderTest,
       GmailAccountDeterminedImmediatelyAfterRefreshTokensAreLoaded) {
  AccountInfo account = identity_env_.MakeAccountAvailable("account@gmail.com");
  // Simulate refresh tokens not being loaded state.
  identity_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Outcome is pending until tokens are loaded.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // An @gmail.com account should be immediately determined as such after
  // refresh tokens are loaded.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.ReloadAccountsFromDisk();
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerGmail);
}

TEST_F(
    AccountManagedStatusFinderTest,
    WellKnownConsumerAccountDeterminedImmediatelyAfterRefreshTokensAreLoaded) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@hotmail.com");
  // Simulate refresh tokens not being loaded state.
  identity_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Outcome is pending until tokens are loaded.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // An account from a well-known consumer domain should be immediately
  // determined as such after refresh tokens are loaded.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.ReloadAccountsFromDisk();
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kConsumerWellKnown);
}

TEST_F(AccountManagedStatusFinderTest,
       ErrorOnNonExistentAccountImmediatelyAfterRefreshTokensAreLoaded) {
  AccountInfo account = identity_env_.MakeAccountAvailable("account@gmail.com");
  // The account gets removed before the AccountManagedStatusFinder is created.
  identity_env_.RemoveRefreshTokenForAccount(account.account_id);
  // Simulate refresh tokens not being loaded state.
  identity_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Outcome is pending until tokens are loaded.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // The AccountManagedStatusFinder should detect non-existent account and
  // immediately report an error after refresh tokens are loaded.
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.ReloadAccountsFromDisk();
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kError);
}

TEST_F(AccountManagedStatusFinderTest,
       EnterpriseAccountDeterminedAsynchronouslyAfterRefreshTokensAreLoaded) {
  AccountInfo account =
      identity_env_.MakeAccountAvailable("account@enterprise.com");
  // Simulate refresh tokens not being loaded state.
  identity_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Outcome is pending until tokens are loaded.
  base::MockCallback<base::OnceClosure> outcome_determined;
  AccountManagedStatusFinder finder(identity_env_.identity_manager(), account,
                                    outcome_determined.Get());
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Since the extended account info is not available yet, the enterprise
  // account can not be identified immediately after refresh tokens are loaded -
  // it's only a potential enterprise account for now, so the outcome is still
  // pending.
  identity_env_.ReloadAccountsFromDisk();
  EXPECT_EQ(finder.GetOutcome(), AccountManagedStatusFinder::Outcome::kPending);

  // Once the extended account info becomes available, the
  // AccountManagedStatusFinder should determine that it's an enterprise
  // account (because it has a non-empty hosted domain).
  EXPECT_CALL(outcome_determined, Run);
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  EXPECT_EQ(finder.GetOutcome(),
            AccountManagedStatusFinder::Outcome::kEnterprise);
}

}  // namespace signin
