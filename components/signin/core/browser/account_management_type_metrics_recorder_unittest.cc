// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_management_type_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

using AccountManagementTypesSummary =
    AccountManagementTypeMetricsRecorder::AccountManagementTypesSummary;

using testing::ElementsAre;
using testing::IsEmpty;

class AccountManagementTypeMetricsRecorderTest : public testing::Test {
 public:
  AccountManagementTypeMetricsRecorderTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_env_;
};

TEST_F(AccountManagementTypeMetricsRecorderTest, NoAccounts) {
  base::HistogramTester histograms;

  AccountManagementTypeMetricsRecorder recorder(
      *identity_env_.identity_manager());

  // No accounts, so nothing to check or wait for, so the histograms should get
  // recorded immediately.
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              IsEmpty());
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
              IsEmpty());
}

TEST_F(AccountManagementTypeMetricsRecorderTest,
       SomeAccountsKnownSynchronously) {
  // 2 consumer accounts and 1 enterprise account.
  AccountInfo consumer1 =
      identity_env_.MakeAccountAvailable("consumer1@gmail.com");
  AccountInfo consumer2 =
      identity_env_.MakeAccountAvailable("consumer2@gmail.com");
  AccountInfo enterprise =
      identity_env_.MakeAccountAvailable("managed@enterprise.com");

  // The enterprise-ness of the account is already known.
  // (The consumer-ness of @gmail.com accounts is known synchronously anyway.)
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise.account_id, enterprise.email, enterprise.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  base::HistogramTester histograms;

  AccountManagementTypeMetricsRecorder recorder(
      *identity_env_.identity_manager());

  // The status of all accounts is known synchronously, so histograms should be
  // recorded immediately.
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementType"),
      ElementsAre(
          base::Bucket(AccountManagedStatusFinder::Outcome::kConsumerGmail, 2),
          base::Bucket(AccountManagedStatusFinder::Outcome::kEnterprise, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
      ElementsAre(base::Bucket(
          AccountManagementTypesSummary::k2plusConsumer1Enterprise, 1)));
}

TEST_F(AccountManagementTypeMetricsRecorderTest,
       SomeAccountsKnownAsynchronously) {
  // 1 consumer account and 2 enterprise accounts.
  AccountInfo consumer =
      identity_env_.MakeAccountAvailable("consumer@consumer.com");
  AccountInfo enterprise1 =
      identity_env_.MakeAccountAvailable("managed1@enterprise.com");
  AccountInfo enterprise2 =
      identity_env_.MakeAccountAvailable("managed2@enterprise.com");
  // None of the accounts use well-known domains, so their status isn't known.

  base::HistogramTester histograms;

  AccountManagementTypeMetricsRecorder recorder(
      *identity_env_.identity_manager());

  // The status of the accounts isn't known yet, so histograms should be empty.
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              IsEmpty());
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
              IsEmpty());

  // The status of all accounts becomes known.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      consumer.account_id, consumer.email, consumer.gaia,
      /*hosted_domain=*/"", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise1.account_id, enterprise1.email, enterprise1.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise2.account_id, enterprise2.email, enterprise2.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  // All the histograms should've been recorded.
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementType"),
      ElementsAre(
          base::Bucket(
              AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown, 1),
          base::Bucket(AccountManagedStatusFinder::Outcome::kEnterprise, 2)));
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
      ElementsAre(base::Bucket(
          AccountManagementTypesSummary::k1Consumer2plusEnterprise, 1)));
}

TEST_F(AccountManagementTypeMetricsRecorderTest,
       SomeAccountsKnownSyncAndAsync) {
  AccountInfo enterprise1 =
      identity_env_.MakeAccountAvailable("managed1@enterprise.com");
  AccountInfo enterprise2 =
      identity_env_.MakeAccountAvailable("managed2@enterprise.com");

  // The *first* account's status is known synchronously, but the second
  // account's isn't.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise1.account_id, enterprise1.email, enterprise1.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  base::HistogramTester histograms;

  AccountManagementTypeMetricsRecorder recorder(
      *identity_env_.identity_manager());

  // One account's status should be recorded, but not the other one's, and not
  // the summary.
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              ElementsAre(base::Bucket(
                  AccountManagedStatusFinder::Outcome::kEnterprise, 1)));
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
              IsEmpty());

  // Once the other account's status becomes known, its own status as well as
  // the summary should be recorded.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise2.account_id, enterprise2.email, enterprise2.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              ElementsAre(base::Bucket(
                  AccountManagedStatusFinder::Outcome::kEnterprise, 2)));
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
      ElementsAre(base::Bucket(
          AccountManagementTypesSummary::k0Consumer2plusEnterprise, 1)));
}

TEST_F(AccountManagementTypeMetricsRecorderTest,
       SomeAccountsKnownAsyncAndSync) {
  AccountInfo enterprise1 =
      identity_env_.MakeAccountAvailable("managed1@enterprise.com");
  AccountInfo enterprise2 =
      identity_env_.MakeAccountAvailable("managed2@enterprise.com");

  // The *second* account's status is known synchronously, but the first
  // account's isn't.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise2.account_id, enterprise2.email, enterprise2.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  base::HistogramTester histograms;

  AccountManagementTypeMetricsRecorder recorder(
      *identity_env_.identity_manager());

  // One account's status should be recorded, but not the other one's, and not
  // the summary.
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              ElementsAre(base::Bucket(
                  AccountManagedStatusFinder::Outcome::kEnterprise, 1)));
  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
              IsEmpty());

  // Once the other account's status becomes known, its own status as well as
  // the summary should be recorded.
  identity_env_.SimulateSuccessfulFetchOfAccountInfo(
      enterprise1.account_id, enterprise1.email, enterprise1.gaia,
      /*hosted_domain=*/"enterprise.com", "Full Name", "Given Name", "en-US",
      /*picture_url=*/"");

  EXPECT_THAT(histograms.GetAllSamples("Signin.AccountManagementType"),
              ElementsAre(base::Bucket(
                  AccountManagedStatusFinder::Outcome::kEnterprise, 2)));
  EXPECT_THAT(
      histograms.GetAllSamples("Signin.AccountManagementTypesSummary"),
      ElementsAre(base::Bucket(
          AccountManagementTypesSummary::k0Consumer2plusEnterprise, 1)));
}

}  // namespace

}  // namespace signin
