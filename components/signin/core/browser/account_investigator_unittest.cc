// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_investigator.h"

#include <map>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramTester;
using base::Time;
using gaia::ListedAccount;
using signin_metrics::AccountRelation;
using signin_metrics::ReportingType;

class AccountInvestigatorTest : public testing::Test {
 protected:
  AccountInvestigatorTest()
      : identity_test_env_(&test_url_loader_factory_, &prefs_),
        investigator_(&prefs_, identity_test_env_.identity_manager()) {
    AccountInvestigator::RegisterPrefs(prefs_.registry());
  }

  ~AccountInvestigatorTest() override { investigator_.Shutdown(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }
  PrefService* pref_service() { return &prefs_; }
  AccountInvestigator* investigator() { return &investigator_; }

  // Wrappers to invoke private methods through friend class.
  base::TimeDelta Delay(const Time previous,
                        const Time now,
                        const base::TimeDelta interval) {
    return AccountInvestigator::CalculatePeriodicDelay(previous, now, interval);
  }
  std::string Hash(const std::vector<ListedAccount>& signed_in_accounts,
                   const std::vector<ListedAccount>& signed_out_accounts) {
    return AccountInvestigator::HashAccounts(signed_in_accounts,
                                             signed_out_accounts);
  }
  AccountRelation Relation(
      const AccountInfo& account_info,
      const std::vector<ListedAccount>& signed_in_accounts,
      const std::vector<ListedAccount>& signed_out_accounts) {
    return AccountInvestigator::DiscernRelation(
        account_info, signed_in_accounts, signed_out_accounts);
  }
  void SharedReport(const std::vector<ListedAccount>& signed_in_accounts,
                    const std::vector<ListedAccount>& signed_out_accounts,
                    const Time now,
                    const ReportingType type) {
    investigator_.SharedCookieJarReport(signed_in_accounts, signed_out_accounts,
                                        now, type);
  }
  void TryPeriodicReport() { investigator_.TryPeriodicReport(); }
  bool* periodic_pending() { return &investigator_.periodic_pending_; }
  bool* previously_authenticated() {
    return &investigator_.previously_authenticated_;
  }
  base::OneShotTimer* timer() { return &investigator_.timer_; }

  void ExpectRelationReport(
      const std::vector<ListedAccount> signed_in_accounts,
      const std::vector<ListedAccount> signed_out_accounts,
      const ReportingType type,
      const AccountRelation expected) {
    HistogramTester histogram_tester;
    investigator_.SignedInAccountRelationReport(signed_in_accounts,
                                                signed_out_accounts, type);
    ExpectRelationReport(type, histogram_tester, expected);
  }

  void ExpectRelationReport(const ReportingType type,
                            const HistogramTester& histogram_tester,
                            const AccountRelation expected) {
    histogram_tester.ExpectUniqueSample(
        "Signin.CookieJar.ChromeAccountRelation" + suffix_[type],
        static_cast<int>(expected), 1);
  }

  // If |relation| is a nullptr, then it should not have been recorded.
  // If |stable_age| is a nullptr, then we're not sure what the expected time
  // should have been, but it still should have been recorded.
  void ExpectSharedReportHistograms(const ReportingType type,
                                    const HistogramTester& histogram_tester,
                                    const base::TimeDelta* stable_age,
                                    const int signed_in_count,
                                    const int signed_out_count,
                                    const int total_count,
                                    const AccountRelation* relation,
                                    const bool is_shared) {
    if (stable_age) {
      histogram_tester.ExpectUniqueSample(
          "Signin.CookieJar.StableAge" + suffix_[type], stable_age->InSeconds(),
          1);
    } else {
      histogram_tester.ExpectTotalCount(
          "Signin.CookieJar.StableAge" + suffix_[type], 1);
    }
    histogram_tester.ExpectUniqueSample(
        "Signin.CookieJar.SignedInCount" + suffix_[type], signed_in_count, 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.CookieJar.SignedOutCount" + suffix_[type], signed_out_count, 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.CookieJar.TotalCount" + suffix_[type], total_count, 1);
    if (relation) {
      histogram_tester.ExpectUniqueSample(
          "Signin.CookieJar.ChromeAccountRelation" + suffix_[type],
          static_cast<int>(*relation), 1);
    } else {
      histogram_tester.ExpectTotalCount(
          "Signin.CookieJar.ChromeAccountRelation" + suffix_[type], 0);
    }
    histogram_tester.ExpectUniqueSample("Signin.IsShared" + suffix_[type],
                                        is_shared, 1);
  }

 private:
  // Timer needs a message loop.
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInvestigator investigator_;
  std::map<ReportingType, std::string> suffix_ = {
      {ReportingType::PERIODIC, "_Periodic"},
      {ReportingType::ON_CHANGE, "_OnChange"}};
};

namespace {

ListedAccount Account(const CoreAccountId& account_id) {
  ListedAccount account;
  account.id = account_id;
  return account;
}

ListedAccount SignedOutAccount(const CoreAccountId& account_id) {
  ListedAccount account;
  account.id = account_id;
  account.signed_out = true;
  return account;
}

AccountInfo ToAccountInfo(ListedAccount account) {
  AccountInfo account_info;
  account_info.account_id = account.id;
  account_info.gaia = account.gaia_id;
  account_info.email = account.email;
  return account_info;
}

// NOTE: IdentityTestEnvironment uses a prefix for generating gaia IDs:
// "gaia_id_for_". For this reason, the tests prefix expected account IDs
// used so that there is a match.
const std::string kGaiaId1 = signin::GetTestGaiaIdForEmail("1@mail.com");
const std::string kGaiaId2 = signin::GetTestGaiaIdForEmail("2@mail.com");
const std::string kGaiaId3 = signin::GetTestGaiaIdForEmail("3@mail.com");

const ListedAccount one(Account(CoreAccountId::FromGaiaId(kGaiaId1)));
const ListedAccount two(Account(CoreAccountId::FromGaiaId(kGaiaId2)));
const ListedAccount three(Account(CoreAccountId::FromGaiaId(kGaiaId3)));

const ListedAccount one_signed_out(
    SignedOutAccount(CoreAccountId::FromGaiaId(kGaiaId1)));

TEST_F(AccountInvestigatorTest, CalculatePeriodicDelay) {
  const Time epoch;
  const base::TimeDelta day(base::Days(1));
  const base::TimeDelta big(base::Days(1000));

  EXPECT_EQ(day, Delay(epoch, epoch, day));
  EXPECT_EQ(day, Delay(epoch + big, epoch + big, day));
  EXPECT_EQ(base::TimeDelta(), Delay(epoch, epoch + big, day));
  EXPECT_EQ(day, Delay(epoch + big, epoch, day));
  EXPECT_EQ(day, Delay(epoch, epoch + day, base::Days(2)));
}

TEST_F(AccountInvestigatorTest, HashAccounts) {
  EXPECT_EQ(Hash({}, {}), Hash({}, {}));
  EXPECT_EQ(Hash({one}, {two}), Hash({one}, {two}));
  EXPECT_EQ(Hash({one, two}, {}), Hash({one, two}, {}));
  EXPECT_EQ(Hash({}, {one, two}), Hash({}, {one, two}));
  EXPECT_EQ(Hash({one, two}, {}), Hash({two, one}, {}));
  EXPECT_EQ(Hash({}, {one, two}), Hash({}, {two, one}));

  EXPECT_NE(Hash({}, {}), Hash({one}, {}));
  EXPECT_NE(Hash({}, {}), Hash({}, {one}));
  EXPECT_NE(Hash({one}, {}), Hash({two}, {}));
  EXPECT_NE(Hash({one}, {}), Hash({one, two}, {}));
  EXPECT_NE(Hash({one}, {}), Hash({}, {one}));
}

TEST_F(AccountInvestigatorTest, DiscernRelation) {
  EXPECT_EQ(AccountRelation::EMPTY_COOKIE_JAR,
            Relation(ToAccountInfo(one), {}, {}));
  EXPECT_EQ(AccountRelation::SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT,
            Relation(ToAccountInfo(one), {one}, {}));
  EXPECT_EQ(AccountRelation::SINGLE_SINGED_IN_MATCH_WITH_SIGNED_OUT,
            Relation(ToAccountInfo(one), {one}, {two}));
  EXPECT_EQ(AccountRelation::WITH_SIGNED_IN_NO_MATCH,
            Relation(ToAccountInfo(one), {two}, {}));
  EXPECT_EQ(AccountRelation::ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT,
            Relation(ToAccountInfo(one), {one, two}, {one}));
  EXPECT_EQ(AccountRelation::ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT,
            Relation(ToAccountInfo(one), {one, two}, {}));
  EXPECT_EQ(AccountRelation::NO_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
            Relation(ToAccountInfo(one), {}, {one, two}));
  EXPECT_EQ(AccountRelation::NO_SIGNED_IN_SINGLE_SIGNED_OUT_MATCH,
            Relation(ToAccountInfo(one), {}, {one}));
  EXPECT_EQ(AccountRelation::WITH_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH,
            Relation(ToAccountInfo(one), {two}, {one}));
  EXPECT_EQ(AccountRelation::NO_SIGNED_IN_WITH_SIGNED_OUT_NO_MATCH,
            Relation(ToAccountInfo(three), {}, {one, two}));
}

TEST_F(AccountInvestigatorTest, SignedInAccountRelationReport) {
  ExpectRelationReport({one}, {}, ReportingType::PERIODIC,
                       AccountRelation::WITH_SIGNED_IN_NO_MATCH);
  identity_test_env()->SetPrimaryAccount("1@mail.com",
                                         signin::ConsentLevel::kSync);
  ExpectRelationReport({one}, {}, ReportingType::PERIODIC,
                       AccountRelation::SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT);
  ExpectRelationReport({two}, {}, ReportingType::ON_CHANGE,
                       AccountRelation::WITH_SIGNED_IN_NO_MATCH);
}

TEST_F(AccountInvestigatorTest, SharedCookieJarReportEmpty) {
  const HistogramTester histogram_tester;
  const base::TimeDelta expected_stable_age;
  SharedReport({}, {}, Time(), ReportingType::PERIODIC);
  ExpectSharedReportHistograms(ReportingType::PERIODIC, histogram_tester,
                               &expected_stable_age, 0, 0, 0, nullptr, false);
}

TEST_F(AccountInvestigatorTest, SharedCookieJarReportWithAccount) {
  identity_test_env()->SetPrimaryAccount("1@mail.com",
                                         signin::ConsentLevel::kSync);
  base::Time now = base::Time::Now();
  pref_service()->SetDouble(prefs::kGaiaCookieChangedTime,
                            now.InSecondsFSinceUnixEpoch());
  const AccountRelation expected_relation(
      AccountRelation::ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT);
  const HistogramTester histogram_tester;
  const base::TimeDelta expected_stable_age(base::Days(1));
  SharedReport({one, two}, {}, now + base::Days(1), ReportingType::ON_CHANGE);
  ExpectSharedReportHistograms(ReportingType::ON_CHANGE, histogram_tester,
                               &expected_stable_age, 2, 0, 2,
                               &expected_relation, false);
}

TEST_F(AccountInvestigatorTest, OnGaiaAccountsInCookieUpdatedError) {
  const HistogramTester histogram_tester;
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/true, {one});
  GoogleServiceAuthError error(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  investigator()->OnAccountsInCookieUpdated(accounts_in_cookie_jar_info, error);
  EXPECT_EQ(
      0u, histogram_tester.GetTotalCountsForPrefix("Signin.CookieJar.").size());
}

TEST_F(AccountInvestigatorTest, OnGaiaAccountsInCookieUpdatedOnChange) {
  const HistogramTester histogram_tester;
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/true, {one});
  investigator()->OnAccountsInCookieUpdated(
      accounts_in_cookie_jar_info, GoogleServiceAuthError::AuthErrorNone());
  ExpectSharedReportHistograms(ReportingType::ON_CHANGE, histogram_tester,
                               nullptr, 1, 0, 1, nullptr, false);
}

TEST_F(AccountInvestigatorTest, OnGaiaAccountsInCookieUpdatedSigninOnly) {
  // Initial update to simulate the update on first-time-run.
  investigator()->OnAccountsInCookieUpdated(
      signin::AccountsInCookieJarInfo(),
      GoogleServiceAuthError::AuthErrorNone());

  const HistogramTester histogram_tester;
  identity_test_env()->SetPrimaryAccount("1@mail.com",
                                         signin::ConsentLevel::kSync);
  pref_service()->SetString(prefs::kGaiaCookieHash, Hash({one}, {}));
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/true, {one});
  investigator()->OnAccountsInCookieUpdated(
      accounts_in_cookie_jar_info, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(
      1u, histogram_tester.GetTotalCountsForPrefix("Signin.CookieJar.").size());
  ExpectRelationReport(ReportingType::ON_CHANGE, histogram_tester,
                       AccountRelation::SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT);
}

TEST_F(AccountInvestigatorTest,
       OnGaiaAccountsInCookieUpdatedSigninSignOutOfContent) {
  const HistogramTester histogram_tester;
  identity_test_env()->SetPrimaryAccount("1@mail.com",
                                         signin::ConsentLevel::kSync);
  signin::AccountsInCookieJarInfo signed_in_account_in_cookies(
      /*accounts_are_fresh=*/true, /*accounts=*/{one});
  investigator()->OnAccountsInCookieUpdated(
      signed_in_account_in_cookies, GoogleServiceAuthError::AuthErrorNone());
  ExpectRelationReport(ReportingType::ON_CHANGE, histogram_tester,
                       AccountRelation::SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT);

  // Simulate a sign out of the content area.
  const HistogramTester histogram_tester2;
  signin::AccountsInCookieJarInfo signed_out_account_in_cookies(
      /*accounts_are_fresh=*/true, /*accounts=*/{one_signed_out});
  investigator()->OnAccountsInCookieUpdated(
      signed_out_account_in_cookies, GoogleServiceAuthError::AuthErrorNone());
  const AccountRelation expected_relation =
      AccountRelation::NO_SIGNED_IN_SINGLE_SIGNED_OUT_MATCH;
  ExpectSharedReportHistograms(ReportingType::ON_CHANGE, histogram_tester2,
                               nullptr, 0, 1, 1, &expected_relation, true);
}

TEST_F(AccountInvestigatorTest, Initialize) {
  EXPECT_FALSE(*previously_authenticated());
  EXPECT_FALSE(timer()->IsRunning());

  investigator()->Initialize();
  EXPECT_FALSE(*previously_authenticated());
  EXPECT_TRUE(timer()->IsRunning());

  investigator()->Shutdown();
  EXPECT_FALSE(timer()->IsRunning());
}

TEST_F(AccountInvestigatorTest, InitializeSignedIn) {
  identity_test_env()->SetPrimaryAccount("1@mail.com",
                                         signin::ConsentLevel::kSync);
  EXPECT_FALSE(*previously_authenticated());

  investigator()->Initialize();
  EXPECT_TRUE(*previously_authenticated());
}

TEST_F(AccountInvestigatorTest, TryPeriodicReportStale) {
  investigator()->Initialize();

  const HistogramTester histogram_tester;
  TryPeriodicReport();
  EXPECT_TRUE(*periodic_pending());
  EXPECT_EQ(
      0u, histogram_tester.GetTotalCountsForPrefix("Signin.CookieJar.").size());

  std::string email("f@bar.com");
  identity_test_env()->SetCookieAccounts(
      {{email, signin::GetTestGaiaIdForEmail(email)}});

  EXPECT_FALSE(*periodic_pending());
  ExpectSharedReportHistograms(ReportingType::PERIODIC, histogram_tester,
                               nullptr, 1, 0, 1, nullptr, false);

  // There's no primary account and thus no break-down into types of primary
  // accounts.
  EXPECT_EQ(0u, histogram_tester
                    .GetTotalCountsForPrefix(
                        "Signin.CookieJar.SignedInCountWithPrimary.")
                    .size());
}

TEST_F(AccountInvestigatorTest, TryPeriodicReportWithPrimary) {
  investigator()->Initialize();

  std::string email("f@bar.com");
  identity_test_env()->SetCookieAccounts(
      {{email, signin::GetTestGaiaIdForEmail(email)}});
  identity_test_env()->MakePrimaryAccountAvailable(email,
                                                   signin::ConsentLevel::kSync);

  const HistogramTester histogram_tester;
  TryPeriodicReport();
  EXPECT_FALSE(*periodic_pending());
  histogram_tester.ExpectUniqueSample(
      "Signin.CookieJar.SignedInCountWithPrimary.SyncConsumer",
      /*bucket=*/1, /*count=*/1);
  histogram_tester.ExpectTotalCount(
      "Signin.CookieJar.SignedInCountWithPrimary.NoSyncConsumer",
      /*count=*/0);
}

// Neither iOS nor Android support unconsented primary accounts.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(AccountInvestigatorTest, TryPeriodicReportWithUnconsentedPrimary) {
  investigator()->Initialize();

  std::string email("f@bar.com");
  identity_test_env()->SetCookieAccounts(
      {{email, signin::GetTestGaiaIdForEmail(email)}});
  identity_test_env()->MakePrimaryAccountAvailable(
      email, signin::ConsentLevel::kSignin);

  const HistogramTester histogram_tester;
  TryPeriodicReport();
  EXPECT_FALSE(*periodic_pending());
  histogram_tester.ExpectUniqueSample(
      "Signin.CookieJar.SignedInCountWithPrimary.NoSyncConsumer",
      /*bucket=*/1, /*count=*/1);
  histogram_tester.ExpectTotalCount(
      "Signin.CookieJar.SignedInCountWithPrimary.SyncConsumer",
      /*count=*/0);
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

TEST_F(AccountInvestigatorTest, TryPeriodicReportWithEnterprisePrimary) {
  investigator()->Initialize();

  std::string email("f@bar.com");
  identity_test_env()->SetCookieAccounts(
      {{email, signin::GetTestGaiaIdForEmail(email)}});
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      email, signin::ConsentLevel::kSync);
  account_info.hosted_domain = "bar.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  const HistogramTester histogram_tester;
  TryPeriodicReport();
  EXPECT_FALSE(*periodic_pending());
  histogram_tester.ExpectUniqueSample(
      "Signin.CookieJar.SignedInCountWithPrimary.SyncEnterprise",
      /*bucket=*/1, /*count=*/1);
  histogram_tester.ExpectTotalCount(
      "Signin.CookieJar.SignedInCountWithPrimary.NoSyncEnterprise",
      /*count=*/0);
}

TEST_F(AccountInvestigatorTest, TryPeriodicReportEmpty) {
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(true);
  const HistogramTester histogram_tester;

  TryPeriodicReport();
  EXPECT_FALSE(*periodic_pending());
  ExpectSharedReportHistograms(ReportingType::PERIODIC, histogram_tester,
                               nullptr, 0, 0, 0, nullptr, false);
}

}  // namespace
