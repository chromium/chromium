// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/signin/core/browser/active_directory_account_reconcilor_delegate.h"
#endif

using signin::RevokeTokenAction;
using signin_metrics::AccountReconcilorState;

namespace {

// An AccountReconcilorDelegate that records all calls (Spy pattern).
class SpyReconcilorDelegate : public signin::AccountReconcilorDelegate {
 public:
  int num_reconcile_finished_calls_{0};
  int num_reconcile_timeout_calls_{0};

  bool IsReconcileEnabled() const override { return true; }

  bool IsAccountConsistencyEnforced() const override { return true; }

  gaia::GaiaSource GetGaiaApiSource() const override {
    return gaia::GaiaSource::kChrome;
  }

  bool ShouldAbortReconcileIfPrimaryHasError() const override { return true; }

  CoreAccountId GetFirstGaiaAccountForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const CoreAccountId& primary_account,
      bool first_execution,
      bool will_logout) const override {
    return primary_account;
  }

  std::vector<CoreAccountId> GetChromeAccountsForReconcile(
      const std::vector<CoreAccountId>& chrome_accounts,
      const CoreAccountId& primary_account,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      bool first_execution,
      bool primary_has_error,
      const gaia::MultiloginMode mode) const override {
    return chrome_accounts;
  }

  void OnReconcileFinished(const CoreAccountId& first_account) override {
    ++num_reconcile_finished_calls_;
  }

  base::TimeDelta GetReconcileTimeout() const override {
    // Does not matter as long as it is different from base::TimeDelta::Max().
    return base::TimeDelta::FromMinutes(100);
  }

  void OnReconcileError(const GoogleServiceAuthError& error) override {
    ++num_reconcile_timeout_calls_;
  }
};

// gmock does not allow mocking classes with move-only parameters, preventing
// from mocking the AccountReconcilor class directly (because of the
// unique_ptr<AccountReconcilorDelegate> parameter).
// Introduce a dummy class creating the delegate internally, to avoid the move.
class DummyAccountReconcilorWithDelegate : public AccountReconcilor {
 public:
  DummyAccountReconcilorWithDelegate(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      signin::AccountConsistencyMethod account_consistency,
      bool dice_migration_completed)
      : AccountReconcilor(
            identity_manager,
            client,
            CreateAccountReconcilorDelegate(client,
                                            identity_manager,
                                            account_consistency,
                                            dice_migration_completed)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  // Takes ownership of |delegate|.
  // gmock can't work with move only parameters.
  DummyAccountReconcilorWithDelegate(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      signin::AccountReconcilorDelegate* delegate)
      : AccountReconcilor(
            identity_manager,
            client,
            std::unique_ptr<signin::AccountReconcilorDelegate>(delegate)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  static std::unique_ptr<signin::AccountReconcilorDelegate>
  CreateAccountReconcilorDelegate(
      SigninClient* signin_client,
      signin::IdentityManager* identity_manager,
      signin::AccountConsistencyMethod account_consistency,
      bool dice_migration_completed) {
    switch (account_consistency) {
      case signin::AccountConsistencyMethod::kMirror:
        return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
            identity_manager);
      case signin::AccountConsistencyMethod::kDisabled:
        return std::make_unique<signin::AccountReconcilorDelegate>();
      case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        return std::make_unique<signin::DiceAccountReconcilorDelegate>(
            signin_client, dice_migration_completed);
#else
        NOTREACHED();
        return nullptr;
#endif
    }
    NOTREACHED();
    return nullptr;
  }
};

class MockAccountReconcilor
    : public testing::StrictMock<DummyAccountReconcilorWithDelegate> {
 public:
  MockAccountReconcilor(signin::IdentityManager* identity_manager,
                        SigninClient* client,
                        signin::AccountConsistencyMethod account_consistency,
                        bool dice_migration_completed);

  MockAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  MOCK_METHOD1(PerformMergeAction, void(const CoreAccountId& account_id));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
  MOCK_METHOD1(PerformSetCookiesAction,
               void(const signin::MultiloginParameters& parameters));
};

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
    signin::AccountConsistencyMethod account_consistency,
    bool dice_migration_completed)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
          account_consistency,
          dice_migration_completed) {}

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
          delegate.release()) {}

struct Cookie {
  std::string gaia_id;
  bool is_valid;

  bool operator==(const Cookie& other) const {
    return gaia_id == other.gaia_id && is_valid == other.is_valid;
  }
};

// Converts CookieParams to ListedAccounts.
gaia::ListedAccount ListedAccountFromCookieParams(
    const signin::CookieParams& params,
    const CoreAccountId& account_id) {
  gaia::ListedAccount listed_account;
  listed_account.id = account_id;
  listed_account.email = params.email;
  listed_account.gaia_id = params.gaia_id;
  listed_account.raw_email = params.email;
  listed_account.valid = params.valid;
  listed_account.signed_out = params.signed_out;
  listed_account.verified = params.verified;
  return listed_account;
}

}  // namespace

class AccountReconcilorTest : public ::testing::Test {
 protected:
  AccountReconcilorTest();
  ~AccountReconcilorTest() override;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  TestSigninClient* test_signin_client() { return &test_signin_client_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  MockAccountReconcilor* GetMockReconcilor();
  MockAccountReconcilor* GetMockReconcilor(
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  AccountInfo ConnectProfileToAccount(const std::string& email);

  CoreAccountId PickAccountIdForAccount(const std::string& gaia_id,
                                        const std::string& username);

  void SimulateAddAccountToCookieCompleted(AccountReconcilor* reconcilor,
                                           const CoreAccountId& account_id,
                                           const GoogleServiceAuthError& error);

  void SimulateSetAccountsInCookieCompleted(
      AccountReconcilor* reconcilor,
      signin::SetAccountsInCookieResult result);

  void SimulateLogOutFromCookieCompleted(AccountReconcilor* reconcilor,
                                         const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  void SetAccountConsistency(signin::AccountConsistencyMethod method);

  // Should never be called before |SetAccountConsistency|.
  void SetDiceMigrationCompleted(bool dice_migration_completed);

  PrefService* pref_service() { return &pref_service_; }

  void DeleteReconcilor() { mock_reconcilor_.reset(); }

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::AccountConsistencyMethod account_consistency_;
  bool dice_migration_completed_ = false;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient test_signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<MockAccountReconcilor> mock_reconcilor_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTest);
};

class AccountReconcilorMirrorTest : public AccountReconcilorTest {
 public:
  AccountReconcilorMirrorTest() {
    SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorMirrorTest);
};

// For tests that must be run with multiple account consistency methods.
class AccountReconcilorMethodParamTest
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<signin::AccountConsistencyMethod> {
 public:
  AccountReconcilorMethodParamTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorMethodParamTest);
};

INSTANTIATE_TEST_SUITE_P(Dice_Mirror,
                         AccountReconcilorMethodParamTest,
                         ::testing::Values(
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                             signin::AccountConsistencyMethod::kDice,
#endif
                             signin::AccountConsistencyMethod::kMirror));

AccountReconcilorTest::AccountReconcilorTest()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
      account_consistency_(signin::AccountConsistencyMethod::kDisabled),
      test_signin_client_(&pref_service_, &test_url_loader_factory_),
      identity_test_env_(/*test_url_loader_factory=*/nullptr,
                         &pref_service_,
                         account_consistency_,
                         &test_signin_client_) {
  signin::SetListAccountsResponseHttpNotFound(&test_url_loader_factory_);

  // The reconcilor should not be built before the test can set the account
  // consistency method.
  EXPECT_FALSE(mock_reconcilor_);
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
        identity_test_env_.identity_manager(), &test_signin_client_,
        account_consistency_, dice_migration_completed_);
  }

  return mock_reconcilor_.get();
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor(
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate) {
  mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
      identity_test_env_.identity_manager(), &test_signin_client_,
      std::move(delegate));

  return mock_reconcilor_.get();
}

AccountReconcilorTest::~AccountReconcilorTest() {
  if (mock_reconcilor_)
    mock_reconcilor_->Shutdown();
  test_signin_client_.Shutdown();
}

AccountInfo AccountReconcilorTest::ConnectProfileToAccount(
    const std::string& email) {
  AccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(email);
  return account_info;
}

CoreAccountId AccountReconcilorTest::PickAccountIdForAccount(
    const std::string& gaia_id,
    const std::string& username) {
  return identity_test_env()->identity_manager()->PickAccountIdForAccount(
      gaia_id, username);
}

void AccountReconcilorTest::SimulateAddAccountToCookieCompleted(
    AccountReconcilor* reconcilor,
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  reconcilor->OnAddAccountToCookieCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateSetAccountsInCookieCompleted(
    AccountReconcilor* reconcilor,
    signin::SetAccountsInCookieResult result) {
  reconcilor->OnSetAccountsInCookieCompleted(result);
}

void AccountReconcilorTest::SimulateLogOutFromCookieCompleted(
    AccountReconcilor* reconcilor,
    const GoogleServiceAuthError& error) {
  reconcilor->OnLogOutFromCookieCompleted(error);
}

void AccountReconcilorTest::SimulateCookieContentSettingsChanged(
    content_settings::Observer* observer,
    const ContentSettingsPattern& primary_pattern) {
  observer->OnContentSettingChanged(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, std::string());
}

void AccountReconcilorTest::SetAccountConsistency(
    signin::AccountConsistencyMethod method) {
  account_consistency_ = method;
  dice_migration_completed_ =
      account_consistency_ == signin::AccountConsistencyMethod::kDice;
}

void AccountReconcilorTest::SetDiceMigrationCompleted(
    bool dice_migration_completed) {
  DCHECK_EQ(signin::AccountConsistencyMethod::kDice, account_consistency_);
  dice_migration_completed_ = dice_migration_completed;
}

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
}

enum class IsFirstReconcile {
  kBoth = 0,
  kFirst,
  kNotFirst,
};

struct AccountReconcilorTestTableParam {
  const char* tokens;
  const char* cookies;
  IsFirstReconcile is_first_reconcile;
  const char* gaia_api_calls;
  const char* tokens_after_reconcile;
  const char* cookies_after_reconcile;
  const char* gaia_api_calls_multilogin;
  const char* tokens_after_reconcile_multilogin;
  const char* cookies_after_reconcile_multilogin;
  // Int represents AccountReconcilorDelegate::InconsistencyReason.
  const int inconsistency_reason;
};

std::vector<AccountReconcilorTestTableParam> GenerateTestCasesFromParams(
    const std::vector<AccountReconcilorTestTableParam>& params) {
  std::vector<AccountReconcilorTestTableParam> return_params;
  for (const AccountReconcilorTestTableParam& param : params) {
    if (param.is_first_reconcile == IsFirstReconcile::kBoth) {
      AccountReconcilorTestTableParam param_true = param;
      param_true.is_first_reconcile = IsFirstReconcile::kFirst;
      AccountReconcilorTestTableParam param_false = param;
      param_false.is_first_reconcile = IsFirstReconcile::kNotFirst;
      return_params.push_back(param_true);
      return_params.push_back(param_false);
    } else {
      return_params.push_back(param);
    }
  }
  return return_params;
}

struct ForceDiceMigrationTestTableParam {
  const char* tokens;
  const char* cookies;
  const char* gaia_api_calls;
  const char* tokens_after_reconcile;
  const char* cookies_after_reconcile;
  RevokeTokenAction revoke_token_action;
};

// Pretty prints a AccountReconcilorTestTableParam. Used by gtest.
void PrintTo(const AccountReconcilorTestTableParam& param, ::std::ostream* os) {
  *os << "Tokens: " << param.tokens << ". Cookies: " << param.cookies
      << ". First reconcile: "
      << (param.is_first_reconcile == IsFirstReconcile::kFirst ? "true"
                                                               : "false");
}

class BaseAccountReconcilorTestTable : public AccountReconcilorTest {
 protected:
  BaseAccountReconcilorTestTable(const AccountReconcilorTestTableParam& param)
      : BaseAccountReconcilorTestTable(param.tokens,
                                       param.cookies,
                                       param.is_first_reconcile,
                                       param.gaia_api_calls,
                                       param.tokens_after_reconcile,
                                       param.cookies_after_reconcile) {}

  BaseAccountReconcilorTestTable(const char* tokens,
                                 const char* cookies,
                                 IsFirstReconcile is_first_reconcile,
                                 const char* gaia_api_calls,
                                 const char* tokens_after_reconcile,
                                 const char* cookies_after_reconcile)
      : tokens_(tokens),
        cookies_(cookies),
        is_first_reconcile_(is_first_reconcile),
        gaia_api_calls_(gaia_api_calls),
        tokens_after_reconcile_(tokens_after_reconcile),
        cookies_after_reconcile_(cookies_after_reconcile) {
    accounts_['A'] = {"a@gmail.com",
                      signin::GetTestGaiaIdForEmail("a@gmail.com")};
    accounts_['B'] = {"b@gmail.com",
                      signin::GetTestGaiaIdForEmail("b@gmail.com")};
    accounts_['C'] = {"c@gmail.com",
                      signin::GetTestGaiaIdForEmail("c@gmail.com")};
  }

  struct Account {
    std::string email;
    std::string gaia_id;
  };

  struct Token {
    std::string gaia_id;
    std::string email;
    bool is_authenticated;
    bool has_error;
  };

  // Build Tokens from string.
  std::vector<Token> ParseTokenString(const char* token_string) {
    std::vector<Token> parsed_tokens;
    bool is_authenticated = false;
    bool has_error = false;
    for (int i = 0; token_string[i] != '\0'; ++i) {
      char token_code = token_string[i];
      if (token_code == '*') {
        is_authenticated = true;
        continue;
      }
      if (token_code == 'x') {
        has_error = true;
        continue;
      }
      parsed_tokens.push_back({accounts_[token_code].gaia_id,
                               accounts_[token_code].email, is_authenticated,
                               has_error});
      is_authenticated = false;
      has_error = false;
    }
    return parsed_tokens;
  }

  // Build Cookies from string.
  std::vector<Cookie> ParseCookieString(const char* cookie_string) {
    std::vector<Cookie> parsed_cookies;
    bool valid = true;
    for (int i = 0; cookie_string[i] != '\0'; ++i) {
      char cookie_code = cookie_string[i];
      if (cookie_code == 'x') {
        valid = false;
        continue;
      }
      parsed_cookies.push_back({accounts_[cookie_code].gaia_id, valid});
      valid = true;
    }
    return parsed_cookies;
  }

  // Checks that the tokens in the TokenService match the tokens.
  void VerifyCurrentTokens(const std::vector<Token>& tokens) {
    auto* identity_manager = identity_test_env()->identity_manager();
    EXPECT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(),
              tokens.size());

    signin::ConsentLevel consent_level =
        GetMockReconcilor()->delegate_->GetConsentLevelForPrimaryAccount();
    CoreAccountId primary_account_id =
        identity_manager->GetPrimaryAccountId(consent_level);
    bool authenticated_account_found = false;
    for (const Token& token : tokens) {
      CoreAccountId account_id =
          PickAccountIdForAccount(token.gaia_id, token.email);
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id));
      EXPECT_EQ(
          token.has_error,
          identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              account_id));
      if (token.is_authenticated) {
        EXPECT_EQ(account_id, primary_account_id);
        authenticated_account_found = true;
      }
    }
    if (!authenticated_account_found)
      EXPECT_EQ(CoreAccountId(), primary_account_id);
  }

  void SetupTokens(const char* tokens_string) {
    std::vector<Token> tokens = ParseTokenString(tokens_string);
    Token primary_account;
    for (const Token& token : tokens) {
      CoreAccountId account_id;
      if (token.is_authenticated) {
        account_id = ConnectProfileToAccount(token.email).account_id;
      } else {
        account_id =
            identity_test_env()->MakeAccountAvailable(token.email).account_id;
      }
      if (token.has_error) {
        signin::UpdatePersistentErrorOfRefreshTokenForAccount(
            identity_test_env()->identity_manager(), account_id,
            GoogleServiceAuthError(
                GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      }
    }
    VerifyCurrentTokens(tokens);
  }

  void ConfigureCookieManagerService(const std::vector<Cookie>& cookies) {
    std::vector<signin::CookieParams> cookie_params;
    for (const auto& cookie : cookies) {
      std::string gaia_id = cookie.gaia_id;

      // Figure the account token of this specific account id,
      // ie 'A', 'B', or 'C'.
      char account_key = '\0';
      for (const auto& account : accounts_) {
        if (account.second.gaia_id == gaia_id) {
          account_key = account.first;
          break;
        }
      }
      ASSERT_NE(account_key, '\0');

      cookie_params.push_back({accounts_[account_key].email, gaia_id,
                               cookie.is_valid, false /* signed_out */,
                               true /* verified */});
    }
    signin::SetListAccountsResponseWithParams(cookie_params,
                                              &test_url_loader_factory_);
    identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void RunReconcile() {
    // Setup cookies.
    std::vector<Cookie> cookies = ParseCookieString(cookies_);
    ConfigureCookieManagerService(cookies);

    // Call list accounts now so that the next call completes synchronously.
    identity_test_env()->identity_manager()->GetAccountsInCookieJar();
    base::RunLoop().RunUntilIdle();

    // Setup tokens. This triggers listing cookies so we need to setup cookies
    // before that.
    SetupTokens(tokens_);

    // Setup expectations.
    testing::InSequence mock_sequence;
    bool logout_action = false;
    for (int i = 0; gaia_api_calls_[i] != '\0'; ++i) {
      if (gaia_api_calls_[i] == 'X') {
        logout_action = true;
        EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
            .Times(1);
        cookies.clear();
        continue;
      }
      std::string cookie(1, gaia_api_calls_[i]);
      CoreAccountId account_id_for_cookie = PickAccountIdForAccount(
          accounts_[cookie[0]].gaia_id, accounts_[cookie[0]].email);
      EXPECT_CALL(*GetMockReconcilor(),
                  PerformMergeAction(account_id_for_cookie))
          .Times(1);
      // MergeSession fixes an existing cookie or appends it at the end.
      auto it =
          std::find(cookies.begin(), cookies.end(),
                    Cookie{accounts_[cookie[0]].gaia_id, false /* is_valid */});
      if (it == cookies.end())
        cookies.push_back({accounts_[cookie[0]].gaia_id, true});
      else
        it->is_valid = true;
    }
    if (!logout_action) {
      EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
          .Times(0);
    }

    // Check the expected cookies after reconcile.
    std::vector<Cookie> expected_cookies =
        ParseCookieString(cookies_after_reconcile_);
    ASSERT_EQ(expected_cookies, cookies);

    // Reconcile.
    AccountReconcilor* reconcilor = GetMockReconcilor();
    ASSERT_TRUE(reconcilor->first_execution_);
    reconcilor->first_execution_ =
        is_first_reconcile_ == IsFirstReconcile::kFirst;
    ASSERT_TRUE(reconcilor->delegate_->IsAccountConsistencyEnforced());
    reconcilor->StartReconcile();
    for (int i = 0; gaia_api_calls_[i] != '\0'; ++i) {
      if (gaia_api_calls_[i] == 'X') {
        SimulateLogOutFromCookieCompleted(
            reconcilor, GoogleServiceAuthError::AuthErrorNone());
        continue;
      }
      CoreAccountId account_id =
          PickAccountIdForAccount(accounts_[gaia_api_calls_[i]].gaia_id,
                                  accounts_[gaia_api_calls_[i]].email);
      SimulateAddAccountToCookieCompleted(
          reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
    }
    ASSERT_FALSE(reconcilor->is_reconcile_started_);

    if (tokens_ == tokens_after_reconcile_) {
      EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
    } else {
      // If the tokens were changed by the reconcile, a new reconcile should be
      // scheduled.
      EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
                reconcilor->GetState());
    }

    VerifyCurrentTokens(ParseTokenString(tokens_after_reconcile_));

    testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

    // Another reconcile is sometimes triggered if Chrome accounts have changed.
    // Allow it to finish.
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_))
        .WillRepeatedly(testing::Return());
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .WillRepeatedly(testing::Return());
    ConfigureCookieManagerService({});
    base::RunLoop().RunUntilIdle();
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  std::string GaiaIdForAccountKey(char account_key) {
    return accounts_[account_key].gaia_id;
  }

  std::map<char, Account> accounts_;
  const char* tokens_;
  const char* cookies_;
  IsFirstReconcile is_first_reconcile_;
  const char* gaia_api_calls_;
  const char* tokens_after_reconcile_;
  const char* cookies_after_reconcile_;
};

// Parameterized version of AccountReconcilorTest.
class AccountReconcilorTestTable
    : public BaseAccountReconcilorTestTable,
      public ::testing::WithParamInterface<AccountReconcilorTestTableParam> {
 protected:
  AccountReconcilorTestTable() : BaseAccountReconcilorTestTable(GetParam()) {}

  // Checks that reconcile is idempotent.
  void CheckReconcileIdempotent(
      const std::vector<AccountReconcilorTestTableParam>& params,
      const AccountReconcilorTestTableParam& param,
      bool multilogin) {
    // Simulate another reconcile based on the results of this one: find the
    // corresponding row in the table and check that it does nothing.
    for (const AccountReconcilorTestTableParam& row : params) {
      if (row.is_first_reconcile == IsFirstReconcile::kFirst)
        continue;

      if (!((strcmp(row.tokens, param.tokens_after_reconcile) == 0 &&
             strcmp(row.cookies, param.cookies_after_reconcile) == 0 &&
             !multilogin) ||
            (strcmp(row.tokens, param.tokens_after_reconcile_multilogin) == 0 &&
             strcmp(row.cookies, param.cookies_after_reconcile_multilogin) ==
                 0 &&
             multilogin))) {
        continue;
      }
      if (multilogin) {
        EXPECT_STREQ(row.tokens, row.tokens_after_reconcile_multilogin);
        EXPECT_STREQ(row.cookies, row.cookies_after_reconcile_multilogin);
      } else {
        EXPECT_STREQ(row.tokens, row.tokens_after_reconcile);
        EXPECT_STREQ(row.cookies, row.cookies_after_reconcile);
      }
      return;
    }

    ADD_FAILURE() << "Could not check that reconcile is idempotent.";
  }
};

#if !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorMirrorTest, IdentityManagerRegistration) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithIdentityManager());

  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com");
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(reconcilor->IsRegisteredWithIdentityManager());
}

TEST_F(AccountReconcilorMirrorTest, Reauth) {
  const std::string email = "user@gmail.com";
  AccountInfo account_info = ConnectProfileToAccount(email);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  // Simulate reauth.  The state of the reconcilor should not change.
  auto* account_mutator =
      identity_test_env()->identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator);
  account_mutator->SetPrimaryAccount(account_info.account_id);

  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorMirrorTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount("user@gmail.com");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {
std::vector<Cookie> FakeSetAccountsInCookie(
    const signin::MultiloginParameters& parameters,
    const std::vector<Cookie>& cookies_before_reconcile) {
  std::vector<Cookie> cookies_after_reconcile;
  if (parameters.mode ==
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER) {
    for (const CoreAccountId& account : parameters.accounts_to_send) {
      cookies_after_reconcile.push_back({account.ToString(), true});
    }
  } else {
    std::vector<CoreAccountId> accounts(parameters.accounts_to_send.begin(),
                                        parameters.accounts_to_send.end());
    cookies_after_reconcile = cookies_before_reconcile;
    for (Cookie& param : cookies_after_reconcile) {
      CoreAccountId account = CoreAccountId(param.gaia_id);
      if (base::Contains(accounts, account)) {
        param.is_valid = true;
        accounts.erase(std::find(accounts.begin(), accounts.end(), account));
      } else {
        DCHECK(!param.is_valid);
      }
    }
    for (const CoreAccountId& account : accounts) {
      cookies_after_reconcile.push_back({account.ToString(), true});
    }
  }
  return cookies_after_reconcile;
}
}  // namespace

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kDiceParams = {
    // This table encodes the initial state and expectations of a reconcile.
    // The syntax is:
    // - Tokens:
    //   A, B, C: Accounts for which we have a token in Chrome.
    //   *: The next account is the main Chrome account (i.e. in
    //   IdentityManager).
    //   x: The next account has a token error.
    // - API calls:
    //   U: Multilogin with mode UPDATE
    //   P: Multilogin with mode PRESERVE
    //   X: Logout all accounts.
    //   A, B, C: Merge account.

    // - Cookies:
    //   A, B, C: Accounts in the Gaia cookie (returned by ListAccounts).
    //   x: The next cookie is marked "invalid".
    // - First Run: true if this is the first reconcile (i.e. Chrome startup).
    // -------------------------------------------------------------------------
    // Tokens|Cookies|First Run|Gaia calls|Tokens aft.|Cookies aft.|M.calls| M.Tokens aft.| M.Cookies aft.| AccountReconcilorDelegate::InconsistencyReason |
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "",      "A",   IsFirstReconcile::kBoth,       "X",    "",     "",      "U",    "",     "",      3},
    {  "*AB",   "AB",  IsFirstReconcile::kBoth,       "",     "*AB",  "AB",    "",     "*AB",  "AB",    0},
    {  "*A",    "A",   IsFirstReconcile::kBoth,       "",     "*A",   "A",     "",     "*A" ,  "A",     0},
    {  "*A",    "",    IsFirstReconcile::kBoth,       "A",    "*A",   "A",     "PA",   "*A" ,  "A",     1},
    {  "*A",    "B",   IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A" ,  "A",     1},
    {  "*A",    "AB",  IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A" ,  "A",     5},
    {  "*AB",   "BA",  IsFirstReconcile::kFirst,      "XAB",  "*AB",  "AB",    "UAB",  "*AB",  "AB",    7},
    {  "*AB",   "BA",  IsFirstReconcile::kNotFirst,   "",     "*AB",  "BA",    "",     "*AB",  "BA",    0},

    {  "*AB",   "A",   IsFirstReconcile::kBoth,       "B",    "*AB",  "AB",    "PAB",  "*AB",  "AB",    4},

    {  "*AB",   "B",   IsFirstReconcile::kFirst,      "XAB",  "*AB",  "AB",    "UAB",  "*AB",  "AB",    1},
    {  "*AB",   "B",   IsFirstReconcile::kNotFirst,   "A",    "*AB",  "BA",    "PBA",  "*AB",  "BA",    1},

    {  "*AB",   "",    IsFirstReconcile::kBoth,       "AB",   "*AB",  "AB",    "PAB",  "*AB",  "AB",    1},
    // Sync enabled, token error on primary.

    {  "*xAB",  "AB",  IsFirstReconcile::kBoth,       "X",    "*xA",  "" ,     "U",    "*xA",  "",      2},
    {  "*xAB",  "BA",  IsFirstReconcile::kBoth,       "XB",   "*xAB", "B",     "UB",   "*xAB", "B",     2},
    {  "*xAB",  "A",   IsFirstReconcile::kBoth,       "X",    "*xA",  "" ,     "U",    "*xA",  "",      2},
    {  "*xAB",  "B",   IsFirstReconcile::kBoth,       "",     "*xAB", "B",     "",     "*xAB", "B",     0},
    {  "*xAB",  "",    IsFirstReconcile::kBoth,       "B",    "*xAB", "B",     "PB",   "*xAB", "B",     0},
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",  IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A",   "A",     5},
    {  "*AxB",  "A",   IsFirstReconcile::kBoth,       "",     "*A",   "A",     "",     "*A",   "A",     0},
    {  "*AxB",  "",    IsFirstReconcile::kBoth,       "A",    "*A",   "A",     "PA",   "*A",   "A",     1},
    // The first account in cookies is swapped even when Chrome is running.
    // The swap would happen at next startup anyway and doing it earlier avoids signing the user out.
    {  "*AxB",  "BA",  IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A",   "A",     5},
    {  "*AxB",  "B",   IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A",   "A",     1},
    // Sync enabled, token error on both accounts.
    {  "*xAxB", "AB",  IsFirstReconcile::kBoth,       "X",    "*xA",  "",      "U",    "*xA",  "",      2},
    {  "*xAxB", "BA",  IsFirstReconcile::kBoth,       "X",    "*xA",  "",      "U",    "*xA",  "",      2},
    {  "*xAxB", "A",   IsFirstReconcile::kBoth,       "X",    "*xA",  "",      "U",    "*xA",  "",      2},
    {  "*xAxB", "B",   IsFirstReconcile::kBoth,       "X",    "*xA",  "",      "U",    "*xA",  "",      5},
    {  "*xAxB", "",    IsFirstReconcile::kBoth,       "",     "*xA",  "",      "",     "*xA",  "",      0},
    // Sync disabled.
    {  "AB",    "AB",  IsFirstReconcile::kBoth,       "",     "AB",   "AB",    "",     "AB",   "AB",    0},
    {  "AB",    "BA",  IsFirstReconcile::kBoth,       "",     "AB",   "BA",    "",     "AB",   "BA",    0},
    {  "AB",    "A",   IsFirstReconcile::kBoth,       "B",    "AB",   "AB",    "PAB",  "AB",   "AB",    4},
    {  "AB",    "B",   IsFirstReconcile::kBoth,       "A",    "AB",   "BA",    "PBA",  "AB",   "BA",    4},
    {  "AB",    "",    IsFirstReconcile::kBoth,       "AB",   "AB",   "AB",    "PAB",  "AB",   "AB",    0},
    // Sync disabled, token error on first account.
    {  "xAB",   "AB",  IsFirstReconcile::kFirst,      "XB",   "B",    "B",     "UB",   "B",    "B",     3},
    {  "xAB",   "AB",  IsFirstReconcile::kNotFirst,   "X",    "",     "" ,     "U",    "",     "",      3},

    {  "xAB",   "BA",  IsFirstReconcile::kBoth,       "XB",   "B",    "B",     "UB",   "B",    "B",     5},

    {  "xAB",   "A",   IsFirstReconcile::kFirst,      "XB",   "B",    "B",     "UB",   "B",    "B",     3},
    {  "xAB",   "A",   IsFirstReconcile::kNotFirst,   "X",    "",     "" ,     "U",    "",     "",      3},

    {  "xAB",   "B",   IsFirstReconcile::kBoth,       "",     "B",    "B",     "",     "B",    "B",     0},

    {  "xAB",   "",    IsFirstReconcile::kBoth,       "B",    "B",    "B",     "PB",   "B",    "B",     0},
    // Sync disabled, token error on second account   .
    {  "AxB",   "AB",  IsFirstReconcile::kBoth,       "XA",   "A",    "A",     "UA",   "A",    "A",     5},

    {  "AxB",   "BA",  IsFirstReconcile::kFirst,      "XA",   "A",    "A",     "UA",   "A",    "A",     3},
    {  "AxB",   "BA",  IsFirstReconcile::kNotFirst,   "X",    "",     "" ,     "U",    "",     "",      3},

    {  "AxB",   "A",   IsFirstReconcile::kBoth,       "",     "A",    "A",     "",     "A",    "A",     0},

    {  "AxB",   "B",   IsFirstReconcile::kFirst,      "XA",   "A",    "A",     "UA",   "A",    "A",     3},
    {  "AxB",   "B",   IsFirstReconcile::kNotFirst,   "X",    "",     "" ,     "U",    "",     "",      3},

    {  "AxB",   "",    IsFirstReconcile::kBoth,       "A",    "A",    "A",     "PA",   "A",    "A",     0},
    // Sync disabled, token error on both accounts.
    {  "xAxB",  "AB",  IsFirstReconcile::kBoth,       "X",    "",     "",      "U",    "",     "",      3},
    {  "xAxB",  "BA",  IsFirstReconcile::kBoth,       "X",    "",     "",      "U",    "",     "",      3},
    {  "xAxB",  "A",   IsFirstReconcile::kBoth,       "X",    "",     "",      "U",    "",     "",      3},
    {  "xAxB",  "B",   IsFirstReconcile::kBoth,       "X",    "",     "",      "U",    "",     "",      3},
    {  "xAxB",  "",    IsFirstReconcile::kBoth,       "",     "",     "",      "",     "",     "",      0},
    // Account marked as invalid in cookies.
    // No difference between cookies and tokens, do not do do anything.
    // Do not logout. Regression tests for http://crbug.com/854799
    {  "",     "xA",   IsFirstReconcile::kBoth,       "",     "",     "xA",    "",     "",     "xA",    0},
    {  "",     "xAxB", IsFirstReconcile::kBoth,       "",     "",     "xAxB",  "",     "",     "xAxB",  0},
    {  "xA",   "xA",   IsFirstReconcile::kBoth,       "",     "",     "xA",    "",     "",     "xA",    0},
    {  "xAB",  "xAB",  IsFirstReconcile::kBoth,       "",     "B",    "xAB",   "",     "B",    "xAB",   0},
    {  "AxB",  "AxC",  IsFirstReconcile::kBoth,       "",     "A",    "AxC",   "",     "A",    "AxC",   0},
    {  "B",    "xAB",  IsFirstReconcile::kBoth,       "",     "B",    "xAB",   "",     "B",    "xAB",   0},
    {  "*xA",  "xA",   IsFirstReconcile::kBoth,       "",     "*xA",  "xA",    "",     "*xA",  "xA",    0},
    {  "*xA",  "xB",   IsFirstReconcile::kBoth,       "",     "*xA",  "xB",    "",     "*xA",  "xB",    0},
    {  "*xAB", "xAB",  IsFirstReconcile::kBoth,       "",     "*xAB", "xAB",   "",     "*xAB", "xAB",   0},
    {  "*AxB", "xBA",  IsFirstReconcile::kNotFirst,   "",     "*A",   "xBA",   "",     "*A",   "xBA",   0},
    // Appending a new cookie after the invalid one.
    {  "B",    "xA",   IsFirstReconcile::kBoth,       "B",    "B",    "xAB",   "PB",   "B",    "xAB",   4},
    {  "xAB",  "xA",   IsFirstReconcile::kBoth,       "B",    "B",    "xAB",   "PB",   "B",    "xAB",   4},
    // Refresh existing cookies.
    {  "AB",   "xAB",  IsFirstReconcile::kBoth,       "A",    "AB",   "AB",    "PAB",  "AB",   "AB",    4},
    {  "*AB",  "xBxA", IsFirstReconcile::kNotFirst,   "BA",   "*AB",  "BA",    "PBA",  "*AB",  "BA",    1},
    // Appending and invalidating cookies at the same time.
    {  "xAB",  "xAC",  IsFirstReconcile::kFirst,      "XB",   "B",    "B",     "UB",   "B",    "B",     6},
    {  "xAB",  "xAC",  IsFirstReconcile::kNotFirst,   "X",    "",     "",      "U",    "",     "",      6},

    {  "xAB",  "AxC",  IsFirstReconcile::kFirst,      "XB",   "B",    "B",     "UB",   "B",    "B",     3},
    {  "xAB",  "AxC",  IsFirstReconcile::kNotFirst,   "X",    "",     "",      "U",    "",     "",      3},

    {  "*xAB", "xABC", IsFirstReconcile::kFirst,      "XB",   "*xAB", "B",     "UB",   "*xAB", "B",     5},
    {  "*xAB", "xABC", IsFirstReconcile::kNotFirst,   "X",    "*xA",  "",      "U",    "*xA",  "",      5},

    {  "xAB",  "xABC", IsFirstReconcile::kFirst,      "XB",   "B",    "B",     "UB",   "B",    "B",     5},
    {  "xAB",  "xABC", IsFirstReconcile::kNotFirst,   "X",    "",     "",      "U",    "",     "",      5},
    // Miscellaneous cases.
    // Check that unknown Gaia accounts are signed out.
    {  "*A",   "AB",   IsFirstReconcile::kBoth,       "XA",   "*A",   "A",     "UA",   "*A",   "A",     5},
    // Check that Gaia default account is kept in first position.
    {  "AB",   "BC",   IsFirstReconcile::kBoth,       "XBA",  "AB",   "BA",    "UBA",  "AB",   "BA",    6},
    // Check that Gaia cookie order is preserved for B.
    {  "*ABC", "CB",   IsFirstReconcile::kFirst,      "XABC", "*ABC", "ABC",   "UABC", "*ABC", "ABC",   1},
    // TODO(https://crbug.com/1129931): Merge session should do XCB instead.
    {  "xABC", "ABC",  IsFirstReconcile::kFirst,      "XBC",  "BC",   "BC",    "UCB",  "BC",   "CB",    1},
    // Check that order in the chrome_accounts is not important.
    {  "A*B",  "",     IsFirstReconcile::kBoth,       "BA",   "A*B",  "BA",    "PBA",  "A*B",  "BA",    7},
    {  "*xBA", "BA",   IsFirstReconcile::kFirst,      "X",    "*xB",  "" ,     "U",    "*xB",  "",      2},
    // Required for idempotency check.
    {  "",     "",     IsFirstReconcile::kNotFirst,   "",     "",     "",      "",     "",     "",      0},
    {  "",     "xA",   IsFirstReconcile::kNotFirst,   "",     "",     "xA",    "",     "",     "xA",    0},
    {  "",     "xB",   IsFirstReconcile::kNotFirst,   "",     "",     "xB",    "",     "",     "xB",    0},
    {  "",     "xAxB", IsFirstReconcile::kNotFirst,   "",     "",     "xAxB",  "",     "",     "xAxB",  0},
    {  "",     "xBxA", IsFirstReconcile::kNotFirst,   "",     "",     "xBxA",  "",     "",     "xBxA",  0},
    {  "*A",   "A",    IsFirstReconcile::kNotFirst,   "",     "*A",   "A",     "",     "*A",   "A",     0},
    {  "*A",   "xBA",  IsFirstReconcile::kNotFirst,   "",     "*A",   "xBA",   "",     "*A",   "xBA",   0},
    {  "*A",   "AxB",  IsFirstReconcile::kNotFirst,   "",     "*A",   "AxB",   "",     "*A",   "AxB",   0},
    {  "A",    "A",    IsFirstReconcile::kNotFirst,   "",     "A",    "A",     "",     "A",    "A",     0},
    {  "A",    "xBA",  IsFirstReconcile::kNotFirst,   "",     "A",    "xBA",   "",     "A",    "xBA",   0},
    {  "A",    "AxB",  IsFirstReconcile::kNotFirst,   "",     "A",    "AxB",   "",     "A",    "AxB",   0},
    {  "B",    "B",    IsFirstReconcile::kNotFirst,   "",     "B",    "B",     "",     "B",    "B",     0},
    {  "B",    "xAB",  IsFirstReconcile::kNotFirst,   "",     "B",    "xAB",   "",     "B",    "xAB",   0},
    {  "B",    "BxA",  IsFirstReconcile::kNotFirst,   "",     "B",    "BxA",   "",     "B",    "BxA",   0},
    {  "*xA",  "",     IsFirstReconcile::kNotFirst,   "",     "*xA",  "",      "",     "*xA",  "",      0},
    {  "*xA",  "xAxB", IsFirstReconcile::kNotFirst,   "",     "*xA",  "xAxB",  "",     "*xA",  "xAxB",  0},
    {  "*xA",  "xBxA", IsFirstReconcile::kNotFirst,   "",     "*xA",  "xBxA",  "",     "*xA",  "xBxA",  0},
    {  "*xA",  "xA",   IsFirstReconcile::kNotFirst,   "",     "*xA",  "xA",    "",     "*xA",  "xA",    0},
    {  "*xA",  "xB",   IsFirstReconcile::kNotFirst,   "",     "*xA",  "xB",    "",     "*xA",  "xB",    0},
    {  "*xAB", "B",    IsFirstReconcile::kNotFirst,   "",     "*xAB", "B",     "",     "*xAB", "B",     0},
    {  "*xAB", "BxA",  IsFirstReconcile::kNotFirst,   "",     "*xAB", "BxA",   "",     "*xAB", "BxA",   0},
    {  "*xAB", "xAB",  IsFirstReconcile::kNotFirst,   "",     "*xAB", "xAB",   "",     "*xAB", "xAB",   0},
    {  "*xAB", "xABxC",IsFirstReconcile::kNotFirst,   "",     "*xAB", "xABxC", "",     "*xAB", "xABxC", 0},
    {  "*xB",  "",     IsFirstReconcile::kNotFirst,   "",     "*xB",  "",      "",     "*xB",  "",      0},
    {  "A*B",  "BA",   IsFirstReconcile::kNotFirst,   "",     "A*B",  "BA",    "",     "A*B",  "BA",    0},
    {  "A*B",  "AB",   IsFirstReconcile::kNotFirst,   "",     "A*B",  "AB",    "",     "A*B",  "AB",    0},
    {  "A",    "AxC",  IsFirstReconcile::kNotFirst,   "",     "A",    "AxC",   "",     "A",    "AxC",   0},
    {  "AB",   "BxCA", IsFirstReconcile::kNotFirst,   "",     "AB",   "BxCA",  "",     "AB",   "BxCA",  0},
    {  "B",    "xABxC",IsFirstReconcile::kNotFirst,   "",     "B",    "xABxC", "",     "B",    "xABxC", 0},
    {  "B",    "xAxCB",IsFirstReconcile::kNotFirst,   "",     "B",    "xAxCB", "",     "B",    "xAxCB", 0},
    {  "*ABC", "ACB",  IsFirstReconcile::kNotFirst,   "",     "*ABC", "ACB",   "",     "*ABC", "ACB",   0},
    {  "*ABC", "ABC",  IsFirstReconcile::kNotFirst,   "",     "*ABC", "ABC",   "",     "*ABC", "ABC",   0},
    {  "BC",   "BC",   IsFirstReconcile::kNotFirst,   "",     "BC",   "BC",    "",     "BC",   "BC",    0},
    {  "BC",   "CB",   IsFirstReconcile::kNotFirst,   "",     "BC",   "CB",    "",     "BC",   "CB",    0},
};
// clang-format on

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestTable, TableRowTest) {
  // Enable Dice.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Check that reconcile is idempotent: when called twice in a row it should do
  // nothing on the second call.
  CheckReconcileIdempotent(kDiceParams, GetParam(), /*multilogin=*/false);
  RunReconcile();
  histogram_tester()->ExpectTotalCount("ForceDiceMigration.RevokeTokenAction",
                                       0);
}

INSTANTIATE_TEST_SUITE_P(
    DiceTable,
    AccountReconcilorTestTable,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kDiceParams)));

class AccountReconcilorTestForceDiceMigration
    : public BaseAccountReconcilorTestTable,
      public ::testing::WithParamInterface<ForceDiceMigrationTestTableParam> {
 public:
  AccountReconcilorTestForceDiceMigration()
      : BaseAccountReconcilorTestTable(GetParam().tokens,
                                       GetParam().cookies,
                                       IsFirstReconcile::kFirst,
                                       GetParam().gaia_api_calls,
                                       GetParam().tokens_after_reconcile,
                                       GetParam().cookies_after_reconcile) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTestForceDiceMigration);
};

// clang-format off
const std::vector<ForceDiceMigrationTestTableParam> kForceDiceParams = {
    {"*A",   "AB",   "XA", "*A",    "A"   , RevokeTokenAction::kNone},
    {"*AxB", "AB",   "XA", "*A",    "A"   , RevokeTokenAction::kNone},
    {"AxB",  "AB",   "XA", "A",     "A"   , RevokeTokenAction::kNone},
    {"xAxB", "AB",   "X",  "",      ""    , RevokeTokenAction::kNone},
    {"*A",   "",     "",   "*xA",   ""    , RevokeTokenAction::kInvalidatePrimaryAccountToken},
    {"*A",   "B",    "X",  "*xA",   ""    , RevokeTokenAction::kInvalidatePrimaryAccountToken},
    {"*AB",  "B",    "",   "*xAB",  "B"   , RevokeTokenAction::kInvalidatePrimaryAccountToken},
    {"*AxB", "B",    "X",  "*xA",   ""    , RevokeTokenAction::kInvalidatePrimaryAccountToken},
    {"*ABC", "CB",   "",   "*xABC", "CB"  , RevokeTokenAction::kInvalidatePrimaryAccountToken},
    {"*AB",  "A",    "",   "*A",    "A"   , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AB",   "A",    "",   "A",     "A"   , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AB",   "",     "",   "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "",     "",   "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "A",    "X",  "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "xA",   "",   "",      "xA"  , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "B",    "",   "B",     "B"   , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AxB",  "B",    "X",  "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AxB",  "",     "",   "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAxB", "",     "",   "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"B",    "xA",   "",   "",      "xA"  , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AB",   "xAB",  "",   "B",     "xAB" , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "xAC",  "X",  "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"xAB",  "AxC",  "X",  "",      ""    , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"AB",   "BC",   "XB", "B",     "B"   , RevokeTokenAction::kRevokeSecondaryAccountsTokens},
    {"*AB",  "",     "",   "*xA",   ""    , RevokeTokenAction::kRevokeTokensForPrimaryAndSecondaryAccounts},
    {"*xAB", "",     "",   "*xA",   ""    , RevokeTokenAction::kRevokeTokensForPrimaryAndSecondaryAccounts},
    {"*AxB", "",     "",   "*xA",   ""    , RevokeTokenAction::kRevokeTokensForPrimaryAndSecondaryAccounts},
    {"*AB",  "xBxA", "",   "*xA",   "xBxA", RevokeTokenAction::kRevokeTokensForPrimaryAndSecondaryAccounts}
  };
// clang-format on

// Checks one row of the kForceDiceParams table above.
TEST_P(AccountReconcilorTestForceDiceMigration, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  EXPECT_FALSE(test_signin_client()->is_dice_migration_completed());
  SetDiceMigrationCompleted(false);
  RunReconcile();
  EXPECT_TRUE(test_signin_client()->is_dice_migration_completed());
  EXPECT_FALSE(
      GetMockReconcilor()->delegate_->ShouldRevokeTokensNotInCookies());
  histogram_tester()->ExpectUniqueSample("ForceDiceMigration.RevokeTokenAction",
                                         GetParam().revoke_token_action, 1);
}

// Check that the result state of the reconcile is in a final state (reconcile
// started from this state is a no-op).
TEST_P(AccountReconcilorTestForceDiceMigration, TableRowTestCheckNoOp) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(cookies_after_reconcile_);
  ConfigureCookieManagerService(cookies);

  // Call list accounts now so that the next call completes synchronously.
  identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  base::RunLoop().RunUntilIdle();

  // Setup tokens. This triggers listing cookies so we need to setup cookies
  // before that.
  SetupTokens(tokens_after_reconcile_);

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  EXPECT_FALSE(reconcilor->delegate_->ShouldRevokeTokensNotInCookies());
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

INSTANTIATE_TEST_SUITE_P(DiceMigrationTable,
                         AccountReconcilorTestForceDiceMigration,
                         ::testing::ValuesIn(kForceDiceParams));

// Parameterized version of AccountReconcilorTest that tests Dice
// implementation with Multilogin endpoint.
class AccountReconcilorTestDiceMultilogin : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestDiceMultilogin() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTestDiceMultilogin);
};

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestDiceMultilogin, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  scoped_feature_list_.InitAndEnableFeature(kUseMultiloginEndpoint);

  CheckReconcileIdempotent(kDiceParams, GetParam(), /*multilogin=*/true);

  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);
  std::vector<Cookie> cookies_after_reconcile = cookies;

  // Call list accounts now so that the next call completes synchronously.
  identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  base::RunLoop().RunUntilIdle();

  // Setup tokens. This triggers listing cookies so we need to setup cookies
  // before that.
  SetupTokens(GetParam().tokens);

  // Setup expectations.
  testing::InSequence mock_sequence;
  if (GetParam().gaia_api_calls_multilogin[0] != '\0') {
    gaia::MultiloginMode mode =
        GetParam().gaia_api_calls_multilogin[0] == 'U'
            ? gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER
            : gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
    // Generate expected array of accounts in cookies and set fake gaia
    // response.
    std::vector<CoreAccountId> accounts_to_send;
    for (int i = 1; GetParam().gaia_api_calls_multilogin[i] != '\0'; ++i) {
      accounts_to_send.push_back(CoreAccountId(
          accounts_[GetParam().gaia_api_calls_multilogin[i]].gaia_id));
    }
    const signin::MultiloginParameters params(mode, accounts_to_send);
    cookies_after_reconcile = FakeSetAccountsInCookie(params, cookies);
    if (accounts_to_send.empty() &&
        (mode ==
         gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER)) {
      EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
          .Times(1);
    } else {
      EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params))
          .Times(1);
    }
  }
  // Reconcile.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ =
      GetParam().is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
  reconcilor->StartReconcile();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  if (GetParam().tokens == GetParam().tokens_after_reconcile_multilogin) {
    EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  } else {
    // If the tokens were changed by the reconcile, a new reconcile should be
    // scheduled.
    EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
              reconcilor->GetState());
  }
  VerifyCurrentTokens(
      ParseTokenString(GetParam().tokens_after_reconcile_multilogin));

  std::vector<Cookie> cookies_after =
      ParseCookieString(GetParam().cookies_after_reconcile_multilogin);
  EXPECT_EQ(cookies_after, cookies_after_reconcile);

  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Another reconcile is sometimes triggered if Chrome accounts have
  // changed. Allow it to finish.
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .WillRepeatedly(testing::Return());
  ConfigureCookieManagerService({});
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    DiceTableMultilogin,
    AccountReconcilorTestDiceMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kDiceParams)));

class AccountReconcilorDiceEndpointParamTest
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AccountReconcilorDiceEndpointParamTest() {
    SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
    if (IsMultiloginEnabled())
      scoped_feature_list_.InitAndEnableFeature(kUseMultiloginEndpoint);
  }
  bool IsMultiloginEnabled() { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorDiceEndpointParamTest);
};

// Tests that the AccountReconcilor is always registered.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceTokenServiceRegistration) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com");
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  // Reconcilor should not logout all accounts from the cookies when
  // the primary account is cleared in IdentityManager.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(::testing::_))
      .Times(0);

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

// Tests that reconcile starts even when Sync is not enabled.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceReconcileWithoutSignin) {
  // Add a token in Chrome but do not sign in. Making account available (setting
  // a refresh token) triggers listing cookies so we need to setup cookies
  // before that.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  const CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable("user@gmail.com").account_id;

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<CoreAccountId> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that nothing happens when there is no Chrome account and no Gaia
// cookie.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceReconcileNoop) {
  // No Chrome account and no cookie.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the first Gaia account is re-used when possible.
TEST_P(AccountReconcilorDiceEndpointParamTest,
       DiceReconcileReuseGaiaFirstAccount) {
  // Add account "other" to the Gaia cookie.
  signin::SetListAccountsResponseTwoAccounts(
      "other@gmail.com", signin::GetTestGaiaIdForEmail("other@gmail.com"),
      "foo@gmail.com", "9999", &test_url_loader_factory_);

  // Add accounts "user" and "other" to the token service.
  const AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("user@gmail.com");
  const CoreAccountId account_id_1 = account_info_1.account_id;
  const AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id_2 = account_info_2.account_id;

  auto* identity_manager = identity_test_env()->identity_manager();
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_EQ(2u, accounts.size());
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_1));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_2));

  if (!IsMultiloginEnabled()) {
    testing::InSequence mock_sequence;
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
    // Account 2 is added first.
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_2));
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1));
  } else {
    std::vector<CoreAccountId> accounts_to_send = {account_id_2, account_id_1};
    // Send accounts to Gaia in order of chrome accounts. Account 2 is added
    // first.
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateLogOutFromCookieCompleted(reconcilor,
                                      GoogleServiceAuthError::AuthErrorNone());
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_1, GoogleServiceAuthError::AuthErrorNone());
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_2, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the first account is kept in cache and reused when cookies are
// lost.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceLastKnownFirstAccount) {
  // Add accounts to the token service and the Gaia cookie in a different order.
  // Making account available (setting a refresh token) triggers listing cookies
  // so we need to setup cookies before that.
  signin::SetListAccountsResponseTwoAccounts(
      "other@gmail.com", signin::GetTestGaiaIdForEmail("other@gmail.com"),
      "user@gmail.com", signin::GetTestGaiaIdForEmail("user@gmail.com"),
      &test_url_loader_factory_);

  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("user@gmail.com");
  const CoreAccountId account_id_1 = account_info_1.account_id;
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id_2 = account_info_2.account_id;

  auto* identity_manager = identity_test_env()->identity_manager();
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_EQ(2u, accounts.size());

  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_1));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_2));

  // Do one reconcile. It should do nothing but to populating the last known
  // account.
  {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
        .Times(0);

    AccountReconcilor* reconcilor = GetMockReconcilor();
    reconcilor->StartReconcile();
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  }

  // Delete the cookies.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);

  if (!IsMultiloginEnabled()) {
    // Reconcile again and check that account_id_2 is added first.
    testing::InSequence mock_sequence;

    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_2))
        .Times(1);
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1))
        .Times(1);
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
  } else {
    // Since Gaia can't know about cached account, make sure that we reorder
    // chrome accounts accordingly even in PRESERVE mode.
    std::vector<CoreAccountId> accounts_to_send = {account_id_2, account_id_1};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_2, GoogleServiceAuthError::AuthErrorNone());
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_1, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that the reconcilor does not log out unverified accounts.
TEST_P(AccountReconcilorDiceEndpointParamTest, UnverifiedAccountNoop) {
  // Add a unverified account to the Gaia cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", true /* valid */, false /* signed_out */,
       false /* verified */},
      &test_url_loader_factory_);

  // Check that nothing happens.
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that the reconcilor does not log out unverified accounts when adding
// a new account to the Gaia cookie.
TEST_P(AccountReconcilorDiceEndpointParamTest, UnverifiedAccountMerge) {
  // Add a unverified account to the Gaia cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", true /* valid */, false /* signed_out */,
       false /* verified */},
      &test_url_loader_factory_);

  // Add a token to Chrome.
  const CoreAccountId chrome_account_id =
      identity_test_env()->MakeAccountAvailable("other@gmail.com").account_id;

  if (!IsMultiloginEnabled()) {
    // Check that the Chrome account is merged and the unverified account is not
    // logged out.
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(chrome_account_id))
        .Times(1);
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
  } else {
    // In PRESERVE mode it is up to Gaia to not delete existing accounts in
    // cookies and not sign out unveridied accounts.
    std::vector<CoreAccountId> accounts_to_send = {chrome_account_id};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, chrome_account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

INSTANTIATE_TEST_SUITE_P(TestDiceEndpoint,
                         AccountReconcilorDiceEndpointParamTest,
                         ::testing::ValuesIn({false, true}));

TEST_F(AccountReconcilorTest, DiceDeleteCookie) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  const CoreAccountId primary_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("user@gmail.com")
          .account_id;
  const CoreAccountId secondary_account_id =
      identity_test_env()->MakeAccountAvailable("other@gmail.com").account_id;

  auto* identity_manager = identity_test_env()->identity_manager();
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(primary_account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshToken(secondary_account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          secondary_account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();

  // With scoped deletion, only secondary tokens are revoked.
  {
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
        reconcilor->GetScopedSyncDataDeletion();
    reconcilor->OnAccountsCookieDeletedByUserAction();
    EXPECT_TRUE(
        identity_manager->HasAccountWithRefreshToken(primary_account_id));
    EXPECT_FALSE(
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account_id));
    EXPECT_FALSE(
        identity_manager->HasAccountWithRefreshToken(secondary_account_id));
  }

  identity_test_env()->SetRefreshTokenForAccount(secondary_account_id);
  reconcilor->OnAccountsCookieDeletedByUserAction();

  // Without scoped deletion, the primary token is also invalidated.
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(primary_account_id));
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            identity_manager
                ->GetErrorStateOfRefreshTokenForAccount(primary_account_id)
                .GetInvalidGaiaCredentialsReason());
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(secondary_account_id));

  // If the primary account has an error, always revoke it.
  identity_test_env()->SetRefreshTokenForAccount(primary_account_id);
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), primary_account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  {
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
        reconcilor->GetScopedSyncDataDeletion();
    reconcilor->OnAccountsCookieDeletedByUserAction();
    EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                  CREDENTIALS_REJECTED_BY_CLIENT,
              identity_manager
                  ->GetErrorStateOfRefreshTokenForAccount(primary_account_id)
                  .GetInvalidGaiaCredentialsReason());
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kMirrorParams = {
// This table encodes the initial state and expectations of a reconcile.
// See kDiceParams for documentation of the syntax.
// -------------------------------------------------------------------------
// Tokens | Cookies | First Run            | Gaia calls | Tokens after | Cookies after
// -------------------------------------------------------------------------

// First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
// tokens. Make the Sync account the default account in the Gaia cookie.
// Sync enabled.
{  "*AB",   "AB",   IsFirstReconcile::kBoth, "",          "*AB",         "AB"},
{  "*AB",   "BA",   IsFirstReconcile::kBoth, "U",         "*AB",         "AB"},
{  "*AB",   "A",    IsFirstReconcile::kBoth, "U",         "*AB",         "AB"},
{  "*AB",   "B",    IsFirstReconcile::kBoth, "U",         "*AB",         "AB"},
{  "*AB",   "",     IsFirstReconcile::kBoth, "U",         "*AB",         "AB"},
// Sync enabled, token error on primary.
// Sync enabled, token error on secondary.
{  "*AxB",  "AB",   IsFirstReconcile::kBoth, "U",         "*AxB",        "A"},
{  "*AxB",  "BA",   IsFirstReconcile::kBoth, "U",         "*AxB",        "A"},
{  "*AxB",  "A",    IsFirstReconcile::kBoth, "",          "*AxB",        ""},
{  "*AxB",  "B",    IsFirstReconcile::kBoth, "U",         "*AxB",        "A"},
{  "*AxB",  "",     IsFirstReconcile::kBoth, "U",         "*AxB",        "A"},

// Cookies can be refreshed in pace, without logout.
{  "*AB",   "xBxA", IsFirstReconcile::kBoth, "U",         "*AB",         "AB"},

// Check that unknown Gaia accounts are signed out.
{  "*A",    "AB",   IsFirstReconcile::kBoth, "U",         "*A",          "A"},
// Check that the previous case is idempotent.
{  "*A",    "A",    IsFirstReconcile::kBoth, "",          "*A",          ""},
};
// clang-format on

// Parameterized version of AccountReconcilorTest that tests Mirror
// implementation with Multilogin endpoint.
class AccountReconcilorTestMirrorMultilogin
    : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestMirrorMultilogin() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTestMirrorMultilogin);
};

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestMirrorMultilogin, TableRowTest) {
  // Enable Mirror.
  SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);

  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);

  // Call list accounts now so that the next call completes synchronously.
  identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  base::RunLoop().RunUntilIdle();

  // Setup tokens.
  SetupTokens(GetParam().tokens);

  // Setup expectations.
  testing::InSequence mock_sequence;
  bool logout_action = false;
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'X') {
      logout_action = true;
      EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
          .Times(1);
      cookies.clear();
      continue;
    }
    if (GetParam().gaia_api_calls[i] == 'U') {
      std::vector<CoreAccountId> accounts_to_send;
      for (int i = 0; GetParam().cookies_after_reconcile[i] != '\0'; ++i) {
        char cookie = GetParam().cookies_after_reconcile[i];
        std::string account_to_send = GaiaIdForAccountKey(cookie);
        accounts_to_send.push_back(PickAccountIdForAccount(
            account_to_send,
            accounts_[GetParam().cookies_after_reconcile[i]].email));
      }
      const signin::MultiloginParameters ml_params(
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(ml_params))
          .Times(1);
    }
  }
  if (!logout_action) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
  }

  // Reconcile.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ =
      GetParam().is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
  reconcilor->StartReconcile();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  VerifyCurrentTokens(ParseTokenString(GetParam().tokens_after_reconcile));

  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Another reconcile is sometimes triggered if Chrome accounts have
  // changed. Allow it to finish.
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .WillRepeatedly(testing::Return());
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
      .WillRepeatedly(testing::Return());
  ConfigureCookieManagerService({});
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    MirrorTableMultilogin,
    AccountReconcilorTestMirrorMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kMirrorParams)));

#if defined(OS_CHROMEOS)
class AccountReconcilorTestActiveDirectory : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestActiveDirectory() = default;

  void SetUp() override {
    SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
  }

 private:
  chromeos::ScopedStubInstallAttributes install_attributes_{
      chromeos::StubInstallAttributes::CreateActiveDirectoryManaged(
          "realm.com",
          "device_id")};

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTestActiveDirectory);
};

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kActiveDirectoryParams = {
// This table encodes the initial state and expectations of a reconcile.
// See kDiceParams for documentation of the syntax.
// -------------------------------------------------------------------------
// Tokens  |Cookies |First Run     |Gaia calls|Tokens aft.|Cookies aft |
// -------------------------------------------------------------------------
{  "ABC",   "ABC",   IsFirstReconcile::kBoth,   "" ,    "ABC",   "ABC" },
{  "ABC",   "",      IsFirstReconcile::kBoth,   "U",    "ABC",   "ABC" },
{  "",      "ABC",   IsFirstReconcile::kBoth,   "X",    "",      "",   },
// Order of Gaia accounts can be different from chrome accounts.
{  "ABC",   "CBA",   IsFirstReconcile::kBoth,   "" ,    "ABC",   "CBA" },
{  "ABC",   "CB",    IsFirstReconcile::kBoth,   "U",    "ABC",   "CBA" },
// Gaia accounts which are not present in chrome accounts should be removed. In
// this case Gaia accounts are going to be in the same order as chrome accounts.
// this case Gaia accounts are going to be in thcousame order as chromcnts.
{  "A",     "AB",    IsFirstReconcile::kBoth,   "U",   "A",     "A"   },
{  "AB",    "CBA",   IsFirstReconcile::kBoth,   "U",   "AB",    "AB"  },
{  "AB",    "C",     IsFirstReconcile::kBoth,   "U",   "AB",    "AB"  },
// Cookies can be refreshed in pace, without logout.
{  "AB",    "xAxB",  IsFirstReconcile::kBoth,   "U",    "AB",    "AB"  },
// Token error on the account - remove it from cookies
{  "AxB",   "AB",    IsFirstReconcile::kBoth,   "U",   "AxB",    "A"   },
{  "xAxB",  "AB",    IsFirstReconcile::kBoth,   "X",   "xAxB",   ""    },
};
// clang-format on

TEST_P(AccountReconcilorTestActiveDirectory, TableRowTestMultilogin) {
  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);

  // Call list accounts now so that the next call completes synchronously.
  identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  base::RunLoop().RunUntilIdle();

  // Setup tokens.
  std::vector<Token> tokens = ParseTokenString(GetParam().tokens);
  SetupTokens(GetParam().tokens);

  testing::InSequence mock_sequence;
  MockAccountReconcilor* reconcilor = GetMockReconcilor(
      std::make_unique<signin::ActiveDirectoryAccountReconcilorDelegate>());

  // Setup expectations.
  bool logout_action = false;
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'X') {
      logout_action = true;
      EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction()).Times(1);
      cookies.clear();
      continue;
    }
    if (GetParam().gaia_api_calls[i] == 'U') {
      std::vector<CoreAccountId> accounts_to_send;
      for (int i = 0; GetParam().cookies_after_reconcile[i] != '\0'; ++i) {
        char cookie = GetParam().cookies_after_reconcile[i];
        std::string account_to_send = GaiaIdForAccountKey(cookie);
        accounts_to_send.push_back(PickAccountIdForAccount(
            account_to_send,
            accounts_[GetParam().cookies_after_reconcile[i]].email));
      }
      const signin::MultiloginParameters params(
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*reconcilor, PerformSetCookiesAction(params)).Times(1);
    }
  }
  if (!logout_action) {
    EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction()).Times(0);
  }

  // Reconcile.
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ =
      GetParam().is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
  reconcilor->StartReconcile();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  VerifyCurrentTokens(ParseTokenString(GetParam().tokens_after_reconcile));

  testing::Mock::VerifyAndClearExpectations(reconcilor);

  // Another reconcile is sometimes triggered if Chrome accounts have
  // changed. Allow it to finish.
  EXPECT_CALL(*reconcilor, PerformSetCookiesAction(testing::_))
      .WillRepeatedly(testing::Return());
  EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction())
      .WillRepeatedly(testing::Return());
  ConfigureCookieManagerService({});
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    ActiveDirectoryTable,
    AccountReconcilorTestActiveDirectory,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kActiveDirectoryParams)));
#endif  // defined(OS_CHROMEOS)

// Tests that reconcile cannot start before the tokens are loaded, and is
// automatically started when tokens are loaded.
TEST_F(AccountReconcilorMirrorTest, TokensNotLoaded) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  // No reconcile when tokens are not loaded.
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  // When tokens are loaded, reconcile starts automatically.
  identity_test_env()->ReloadAccountsFromDisk();

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

TEST_F(AccountReconcilorMirrorTest, GetAccountsFromCookieSuccess) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia, false /* valid */,
       false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
            reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_TRUE(accounts_in_cookie_jar_info.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_info.signed_in_accounts.size());
  ASSERT_EQ(account_id, accounts_in_cookie_jar_info.signed_in_accounts[0].id);
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.signed_out_accounts.size());
}

// Checks that calling EnableReconcile() while the reconcilor is already running
// doesn't have any effect. Regression test for https://crbug.com/1043651
TEST_F(AccountReconcilorMirrorTest, EnableReconcileWhileAlreadyRunning) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia, false /* valid */,
       false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
            reconcilor->GetState());
  reconcilor->StartReconcile();
  EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  reconcilor->EnableReconcile();
  EXPECT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_TRUE(accounts_in_cookie_jar_info.accounts_are_fresh);
  ASSERT_EQ(1u, accounts_in_cookie_jar_info.signed_in_accounts.size());
  ASSERT_EQ(account_id, accounts_in_cookie_jar_info.signed_in_accounts[0].id);
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.signed_out_accounts.size());
}

TEST_F(AccountReconcilorMirrorTest, GetAccountsFromCookieFailure) {
  ConnectProfileToAccount("user@gmail.com");
  signin::SetListAccountsResponseWithUnexpectedServiceResponse(
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
            reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(accounts_in_cookie_jar_info.accounts_are_fresh);
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.signed_in_accounts.size());
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.signed_out_accounts.size());
  // List accounts retries once on |UNEXPECTED_SERVICE_RESPONSE| errors with
  // backoff protection.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_ERROR, reconcilor->GetState());
}

// Regression test for https://crbug.com/923716
TEST_F(AccountReconcilorMirrorTest, ExtraCookieChangeNotification) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  signin::CookieParams cookie_params = {
      account_info.email, account_info.gaia, false /* valid */,
      false /* signed_out */, true /* verified */};

  signin::SetListAccountsResponseOneAccountWithParams(
      cookie_params, &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
            reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  // Add extra cookie change notification. Reconcilor should ignore it.
  gaia::ListedAccount listed_account =
      ListedAccountFromCookieParams(cookie_params, account_id);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info = {
      /*accounts_are_fresh=*/true, {listed_account}, {}};
  reconcilor->OnAccountsInCookieUpdated(
      accounts_in_cookie_jar_info, GoogleServiceAuthError::AuthErrorNone());

  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileNoop) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileCookiesDisabled) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  std::vector<gaia::ListedAccount> accounts;
  // This will be the first call to ListAccounts.
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(accounts_in_cookie_jar_info.accounts_are_fresh);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileContentSettings) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  test_signin_client()->set_are_signin_cookies_allowed(false);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  test_signin_client()->set_are_signin_cookies_allowed(true);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileContentSettingsGaiaUrl) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GaiaUrls::GetInstance()->gaia_url()));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileContentSettingsNonGaiaUrl) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       StartReconcileContentSettingsInvalidPattern) {
  const CoreAccountId account_id =
      ConnectProfileToAccount("user@gmail.com").account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  builder->Invalid();

  SimulateCookieContentSettingsChanged(reconcilor, builder->Build());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

// This test is needed until chrome changes to use gaia obfuscated id.
// The primary account manager and token service use the gaia "email" property,
// which preserves dots in usernames and preserves case.
// gaia::ParseListAccountsData() however uses gaia "displayEmail" which does not
// preserve case, and then passes the string through gaia::CanonicalizeEmail()
// which removes dots.  This tests makes sure that an email like
// "Dot.S@hmail.com", as seen by the token service, will be considered the same
// as "dots@gmail.com" as returned by gaia::ParseListAccountsData().
TEST_F(AccountReconcilorMirrorTest, StartReconcileNoopWithDots) {
  if (identity_test_env()->identity_manager()->GetAccountIdMigrationState() !=
      signin::IdentityManager::AccountIdMigrationState::MIGRATION_NOT_STARTED) {
    return;
  }

  AccountInfo account_info = ConnectProfileToAccount("Dot.S@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileNoopMultiple) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, account_info_2.email,
      account_info_2.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookie) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com").account_id;

    std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.UpTo3mins.Success"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
                  "Signin.Reconciler.Duration.UpTo3mins.Success"),
              testing::ContainerEq(expected_counts));
}

TEST_F(AccountReconcilorTest, AuthErrorTriggersListAccount) {
  class TestGaiaCookieObserver : public signin::IdentityManager::Observer {
   public:
    void OnAccountsInCookieUpdated(
        const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
        const GoogleServiceAuthError& error) override {
      cookies_updated_ = true;
    }

    bool cookies_updated_ = false;
  };

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kDice;
  SetAccountConsistency(account_consistency);
#else
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kMirror;
  SetAccountConsistency(account_consistency);
#endif

  // Add one account to Chrome and instantiate the reconcilor.
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  TestGaiaCookieObserver observer;
  identity_test_env()->identity_manager()->AddObserver(&observer);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  if (account_consistency == signin::AccountConsistencyMethod::kDice) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(1);
  }

  // Set an authentication error.
  ASSERT_FALSE(observer.cookies_updated_);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  base::RunLoop().RunUntilIdle();

  // Check that a call to ListAccount was triggered.
  EXPECT_TRUE(observer.cookies_updated_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  identity_test_env()->identity_manager()->RemoveObserver(&observer);
}

#if !defined(OS_CHROMEOS)
// This test does not run on ChromeOS because it clears the primary account,
// which is not a flow that exists on ChromeOS.

TEST_F(AccountReconcilorMirrorTest, SignoutAfterErrorDoesNotRecordUma) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com").account_id;

    std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kPersistentError);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  identity_test_env()->ClearPrimaryAccount();

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.UpTo3mins.Failure"] = 1;
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorMirrorTest, StartReconcileRemoveFromCookie) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, "other@gmail.com", "12345",
      &test_url_loader_factory_);

    std::vector<CoreAccountId> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

// Check that reconcile is aborted if there is token error on primary account.
TEST_F(AccountReconcilorMirrorTest, TokenErrorOnPrimary) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, "other@gmail.com", "67890",
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookieTwice) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id2 = account_info2.account_id;

  const std::string email3 = "third@gmail.com";
  const std::string gaia_id3 = signin::GetTestGaiaIdForEmail(email3);
  const CoreAccountId account_id3 = PickAccountIdForAccount(gaia_id3, email3);

  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send_1 = {account_id, account_id2};
  const signin::MultiloginParameters ml_params_1(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send_1);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(ml_params_1));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);


  // Do another pass after I've added a third account to the token service
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, account_info2.email,
      account_info2.gaia, &test_url_loader_factory_);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);

  // This will cause the reconcilor to fire.
  identity_test_env()->MakeAccountAvailable(email3);
  std::vector<CoreAccountId> accounts_to_send_2 = {account_id, account_id2,
                                                   account_id3};
  const signin::MultiloginParameters ml_params_2(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send_2);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(ml_params_2));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileBadPrimary) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;

  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id2 = account_info2.account_id;
  signin::SetListAccountsResponseTwoAccounts(
      account_info2.email, account_info2.gaia, account_info.email,
      account_info.gaia, &test_url_loader_factory_);

    std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileOnlyOnce) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, Lock) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);

  class TestAccountReconcilorObserver : public AccountReconcilor::Observer {
   public:
    void OnStateChanged(AccountReconcilorState state) override {
      if (state == AccountReconcilorState::ACCOUNT_RECONCILOR_RUNNING) {
        ++started_count_;
      }
    }
    void OnBlockReconcile() override { ++blocked_count_; }
    void OnUnblockReconcile() override { ++unblocked_count_; }

    int started_count_ = 0;
    int blocked_count_ = 0;
    int unblocked_count_ = 0;
  };

  TestAccountReconcilorObserver observer;
  ScopedObserver<AccountReconcilor, AccountReconcilor::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(reconcilor);

  // Lock prevents reconcile from starting, as long as one instance is alive.
  std::unique_ptr<AccountReconcilor::Lock> lock_1 =
      std::make_unique<AccountReconcilor::Lock>(reconcilor);
  EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
  reconcilor->StartReconcile();
  // lock_1 is blocking the reconcile.
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
  {
    AccountReconcilor::Lock lock_2(reconcilor);
    EXPECT_EQ(2, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
    lock_1.reset();
    // lock_1 is no longer blocking, but lock_2 is still alive.
    EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
    EXPECT_EQ(0, observer.started_count_);
    EXPECT_EQ(0, observer.unblocked_count_);
    EXPECT_EQ(1, observer.blocked_count_);
  }

  // All locks are deleted, reconcile starts.
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(1, observer.started_count_);
  EXPECT_EQ(1, observer.unblocked_count_);
  EXPECT_EQ(1, observer.blocked_count_);

  // Lock aborts current reconcile, and restarts it later.
  {
    AccountReconcilor::Lock lock(reconcilor);
    EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
    EXPECT_FALSE(reconcilor->is_reconcile_started_);
  }
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(2, observer.started_count_);
  EXPECT_EQ(2, observer.unblocked_count_);
  EXPECT_EQ(2, observer.blocked_count_);

  // Reconcile can complete successfully after being restarted.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

// Checks that an "invalid" Gaia account can be refreshed in place, without
// performing a full logout.
TEST_P(AccountReconcilorMethodParamTest,
       StartReconcileWithSessionInfoExpiredDefault) {
  signin::AccountConsistencyMethod account_consistency = GetParam();
  SetAccountConsistency(account_consistency);
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id2 = account_info2.account_id;
  signin::SetListAccountsResponseWithParams(
      {{account_info.email, account_info.gaia, false /* valid */,
        false /* signed_out */, true /* verified */},
       {account_info2.email, account_info2.gaia, true /* valid */,
        false /* signed_out */, true /* verified */}},
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  if (!reconcilor->IsMultiloginEndpointEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    switch (account_consistency) {
      case signin::AccountConsistencyMethod::kMirror: {
        signin::MultiloginParameters params(
            gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
            {account_id, account_id2});
        EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
        break;
      }
      case signin::AccountConsistencyMethod::kDice: {
        signin::MultiloginParameters params(
            gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
            {account_id2, account_id});
        EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
        break;
      }
      case signin::AccountConsistencyMethod::kDisabled:
        NOTREACHED();
        break;
    }
  }

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  if (!reconcilor->IsMultiloginEndpointEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       AddAccountToCookieCompletedWithBogusAccount) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info.email, account_info.gaia, false /* valid */,
       false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

    std::vector<CoreAccountId> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, NoLoopWithBadPrimary) {
  // Connect profile to a primary account and then add a secondary account.
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  const CoreAccountId account_id1 = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com");
  const CoreAccountId account_id2 = account_info2.account_id;

    std::vector<CoreAccountId> accounts_to_send = {account_id1, account_id2};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  // The primary account is in auth error, so it is not in the cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {account_info2.email, account_info2.gaia, false /* valid */,
       false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // The primary cannot be added to cookie, so it fails.
  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kPersistentError);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_NE(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Now that we've tried once, the token service knows that the primary
  // account has an auth error.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id1, error);

  // A second attempt to reconcile should be a noop.
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
}

TEST_F(AccountReconcilorMirrorTest, WontMergeAccountsWithError) {
  // Connect profile to a primary account and then add a secondary account.
  const CoreAccountId account_id1 =
      ConnectProfileToAccount("user@gmail.com").account_id;
  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable("other@gmail.com").account_id;

  // Mark the secondary account in auth error state.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The cookie starts empty.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
    std::vector<CoreAccountId> accounts_to_send = {account_id1};
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, signin::SetAccountsInCookieResult::kSuccess);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
}

// Test that delegate timeout is called when the delegate offers a valid
// timeout.
TEST_F(AccountReconcilorTest, DelegateTimeoutIsCalled) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  auto spy_delegate0 = std::make_unique<SpyReconcilorDelegate>();
  SpyReconcilorDelegate* spy_delegate = spy_delegate0.get();
  AccountReconcilor* reconcilor = GetMockReconcilor(std::move(spy_delegate0));
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  ASSERT_TRUE(timer->IsRunning());

  // Simulate a timeout
  timer->Fire();
  EXPECT_EQ(1, spy_delegate->num_reconcile_timeout_calls_);
  EXPECT_EQ(0, spy_delegate->num_reconcile_finished_calls_);
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

// Test that delegate timeout is not called when the delegate does not offer a
// valid timeout.
TEST_F(AccountReconcilorMirrorTest, DelegateTimeoutIsNotCalled) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile();
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_FALSE(timer->IsRunning());
}

TEST_F(AccountReconcilorTest, DelegateTimeoutIsNotCalledIfTimeoutIsNotReached) {
  AccountInfo account_info = ConnectProfileToAccount("user@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  auto spy_delegate0 = std::make_unique<SpyReconcilorDelegate>();
  SpyReconcilorDelegate* spy_delegate = spy_delegate0.get();
  AccountReconcilor* reconcilor = GetMockReconcilor(std::move(spy_delegate0));
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  ASSERT_TRUE(timer->IsRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(0, spy_delegate->num_reconcile_timeout_calls_);
  EXPECT_EQ(1, spy_delegate->num_reconcile_finished_calls_);
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, ScopedSyncedDataDeletionDestructionOrder) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> data_deletion =
      reconcilor->GetScopedSyncDataDeletion();
  DeleteReconcilor();
  // data_deletion is destroyed after the reconcilor, this should not crash.
}

TEST_F(AccountReconcilorTest, LockDestructionOrder) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  AccountReconcilor::Lock lock(reconcilor);
  DeleteReconcilor();
  // |lock| is destroyed after the reconcilor, this should not crash.
}

// Checks that multilogin with empty list of accounts in UPDATE mode is changed
// into a Logout call.
TEST_F(AccountReconcilorTest, MultiloginLogout) {
  // Delegate implementation always returning UPDATE mode with no accounts.
  class MultiloginLogoutDelegate : public signin::AccountReconcilorDelegate {
    bool IsReconcileEnabled() const override { return true; }
    bool IsAccountConsistencyEnforced() const override { return true; }
    std::vector<CoreAccountId> GetChromeAccountsForReconcile(
        const std::vector<CoreAccountId>& chrome_accounts,
        const CoreAccountId& primary_account,
        const std::vector<gaia::ListedAccount>& gaia_accounts,
        bool first_execution,
        bool primary_has_error,
        const gaia::MultiloginMode mode) const override {
      return {};
    }
    gaia::MultiloginMode CalculateModeForReconcile(
        const std::vector<CoreAccountId>& chrome_accounts,
        const std::vector<gaia::ListedAccount>& gaia_accounts,
        const CoreAccountId& primary_account,
        bool first_execution,
        bool primary_has_error) const override {
      return gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
    }
  };

  MockAccountReconcilor* reconcilor =
      GetMockReconcilor(std::make_unique<MultiloginLogoutDelegate>());
  signin::SetListAccountsResponseOneAccount("user@gmail.com", "123456",
                                            &test_url_loader_factory_);

  // Logout call to Gaia.
  EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction());
  // No multilogin call.
  EXPECT_CALL(*reconcilor, PerformSetCookiesAction(testing::_)).Times(0);

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}
