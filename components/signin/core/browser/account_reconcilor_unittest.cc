// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/signin/core/browser/account_reconcilor.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"
#endif

using signin_metrics::AccountReconcilorState;
using testing::_;
using testing::InSequence;

namespace {

#if BUILDFLAG(ENABLE_MIRROR)
// This should match the variable in the .cc file.
const int kForcedReconciliationWaitTimeInSeconds = 15;
#endif  // BUILDFLAG(ENABLE_MIRROR)

const char kFakeEmail[] = "user@gmail.com";
const char kFakeEmail2[] = "other@gmail.com";
const char kFakeGaiaId[] = "12345";

// An AccountReconcilorDelegate that records all calls (Spy pattern).
class SpyReconcilorDelegate : public signin::AccountReconcilorDelegate {
 public:
  int num_reconcile_finished_calls_{0};
  int num_reconcile_timeout_calls_{0};

  bool IsReconcileEnabled() const override { return true; }

  gaia::GaiaSource GetGaiaApiSource() const override {
    return gaia::GaiaSource::kChrome;
  }

  bool ShouldAbortReconcileIfPrimaryHasError() const override { return true; }

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
    return base::Minutes(100);
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
#if BUILDFLAG(IS_CHROMEOS)
      account_manager::AccountManagerFacade* account_manager_facade,
#endif
      signin::AccountConsistencyMethod account_consistency)
      : AccountReconcilor(identity_manager,
                          client,
#if BUILDFLAG(IS_CHROMEOS)
                          account_manager_facade,
#endif
                          CreateAccountReconcilorDelegate(identity_manager,
                                                          account_consistency,
                                                          client)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  // Takes ownership of |delegate|.
  // gmock can't work with move only parameters.
  DummyAccountReconcilorWithDelegate(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
#if BUILDFLAG(IS_CHROMEOS)
      account_manager::AccountManagerFacade* account_manager_facade,
#endif
      signin::AccountReconcilorDelegate* delegate)
      : AccountReconcilor(
            identity_manager,
            client,
#if BUILDFLAG(IS_CHROMEOS)
            account_manager_facade,
#endif
            std::unique_ptr<signin::AccountReconcilorDelegate>(delegate)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  static std::unique_ptr<signin::AccountReconcilorDelegate>
  CreateAccountReconcilorDelegate(
      signin::IdentityManager* identity_manager,
      signin::AccountConsistencyMethod account_consistency,
      SigninClient* client) {
    switch (account_consistency) {
      case signin::AccountConsistencyMethod::kMirror:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        return std::make_unique<signin::MirrorLandingAccountReconcilorDelegate>(
            identity_manager, client->GetInitialPrimaryAccount().has_value());
#else
        return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
            identity_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      case signin::AccountConsistencyMethod::kDisabled:
        return std::make_unique<signin::AccountReconcilorDelegate>();
      case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        return std::make_unique<signin::DiceAccountReconcilorDelegate>(
            identity_manager, client);
#else
        NOTREACHED_IN_MIGRATION();
        return nullptr;
#endif
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

class MockAccountReconcilor
    : public testing::StrictMock<DummyAccountReconcilorWithDelegate> {
 public:
  MockAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
#if BUILDFLAG(IS_CHROMEOS)
      account_manager::AccountManagerFacade* account_manager_facade,
#endif
      signin::AccountConsistencyMethod account_consistency);

  MockAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
#if BUILDFLAG(IS_CHROMEOS)
      account_manager::AccountManagerFacade* account_manager_facade,
#endif
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
  MOCK_METHOD1(PerformSetCookiesAction,
               void(const signin::MultiloginParameters& parameters));
};

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
#if BUILDFLAG(IS_CHROMEOS)
    account_manager::AccountManagerFacade* account_manager_facade,
#endif
    signin::AccountConsistencyMethod account_consistency)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
#if BUILDFLAG(IS_CHROMEOS)
          account_manager_facade,
#endif
          account_consistency) {
}

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
#if BUILDFLAG(IS_CHROMEOS)
    account_manager::AccountManagerFacade* account_manager_facade,
#endif
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
#if BUILDFLAG(IS_CHROMEOS)
          account_manager_facade,
#endif
          delegate.release()) {
}

struct Cookie {
  std::string gaia_id;
  bool is_valid;

  bool operator==(const Cookie& other) const = default;
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

class TestAccountReconcilorObserver : public AccountReconcilor::Observer {
 public:
  void OnStateChanged(AccountReconcilorState state) override {
    if (state == AccountReconcilorState::kRunning) {
      ++started_count_;
    }
    if (state == AccountReconcilorState::kError) {
      ++error_count_;
    }
  }
  void OnBlockReconcile() override { ++blocked_count_; }
  void OnUnblockReconcile() override { ++unblocked_count_; }

  int started_count_ = 0;
  int blocked_count_ = 0;
  int unblocked_count_ = 0;
  int error_count_ = 0;
};

}  // namespace

class AccountReconcilorTest : public ::testing::Test {
 public:
  AccountReconcilorTest(const AccountReconcilorTest&) = delete;
  AccountReconcilorTest& operator=(const AccountReconcilorTest&) = delete;

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
  MockAccountReconcilor* CreateMockReconcilor(
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  AccountInfo ConnectProfileToAccount(const std::string& email);

  CoreAccountId PickAccountIdForAccount(const std::string& gaia_id,
                                        const std::string& username);

  void SimulateSetAccountsInCookieCompleted(
      AccountReconcilor* reconcilor,
      const std::vector<CoreAccountId>& accounts_to_send,
      signin::SetAccountsInCookieResult result);

  void SimulateLogOutFromCookieCompleted(AccountReconcilor* reconcilor,
                                         const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  void SetAccountConsistency(signin::AccountConsistencyMethod method);

  PrefService* pref_service() { return &pref_service_; }

  void DeleteReconcilor() {
    if (mock_reconcilor_)
      mock_reconcilor_->Shutdown();
    mock_reconcilor_.reset();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

  signin::ConsentLevel consent_level_for_reconcile_ =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // TODO(crbug.com/40067189): Migrate away from
      // `ConsentLevel::kSync` on Ash.
      signin::ConsentLevel::kSync;
#else
      signin::ConsentLevel::kSignin;
#endif

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::AccountConsistencyMethod account_consistency_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient test_signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
#if BUILDFLAG(IS_CHROMEOS)
  account_manager::MockAccountManagerFacade mock_facade_;
#endif
  std::unique_ptr<MockAccountReconcilor> mock_reconcilor_;
  base::HistogramTester histogram_tester_;
};

class AccountReconcilorMirrorTest : public AccountReconcilorTest {
 public:
  AccountReconcilorMirrorTest() {
    SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
  }

  AccountReconcilorMirrorTest(const AccountReconcilorMirrorTest&) = delete;
  AccountReconcilorMirrorTest& operator=(const AccountReconcilorMirrorTest&) =
      delete;
};

// For tests that must be run with multiple account consistency methods.
class AccountReconcilorMethodParamTest
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<signin::AccountConsistencyMethod> {
 public:
  AccountReconcilorMethodParamTest() = default;

  AccountReconcilorMethodParamTest(const AccountReconcilorMethodParamTest&) =
      delete;
  AccountReconcilorMethodParamTest& operator=(
      const AccountReconcilorMethodParamTest&) = delete;
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
                         &test_signin_client_) {
  AccountReconcilor::RegisterProfilePrefs(pref_service_.registry());
  signin::SetListAccountsResponseHttpNotFound(&test_url_loader_factory_);

  // The reconcilor should not be built before the test can set the account
  // consistency method.
  EXPECT_FALSE(mock_reconcilor_);
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
        identity_test_env_.identity_manager(), &test_signin_client_,
#if BUILDFLAG(IS_CHROMEOS)
        &mock_facade_,
#endif
        account_consistency_);
  }

  return mock_reconcilor_.get();
}

MockAccountReconcilor* AccountReconcilorTest::CreateMockReconcilor(
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate) {
  DCHECK(!mock_reconcilor_);
  mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
      identity_test_env_.identity_manager(), &test_signin_client_,
#if BUILDFLAG(IS_CHROMEOS)
      &mock_facade_,
#endif
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
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      email, consent_level_for_reconcile_);
  return account_info;
}

CoreAccountId AccountReconcilorTest::PickAccountIdForAccount(
    const std::string& gaia_id,
    const std::string& username) {
  return identity_test_env()->identity_manager()->PickAccountIdForAccount(
      gaia_id, username);
}

void AccountReconcilorTest::SimulateSetAccountsInCookieCompleted(
    AccountReconcilor* reconcilor,
    const std::vector<CoreAccountId>& accounts_to_send,
    signin::SetAccountsInCookieResult result) {
  reconcilor->OnSetAccountsInCookieCompleted(accounts_to_send, result);
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
      ContentSettingsTypeSet(ContentSettingsType::COOKIES));
}

void AccountReconcilorTest::SetAccountConsistency(
    signin::AccountConsistencyMethod method) {
  account_consistency_ = method;
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

// Pretty prints a AccountReconcilorTestTableParam. Used by gtest.
void PrintTo(const AccountReconcilorTestTableParam& param, ::std::ostream* os) {
  *os << "Tokens: " << param.tokens << ". Cookies: " << param.cookies
      << ". First reconcile: "
      << (param.is_first_reconcile == IsFirstReconcile::kFirst ? "true"
                                                               : "false");
}

class BaseAccountReconcilorTestTable : public AccountReconcilorTest {
 protected:
  BaseAccountReconcilorTestTable() {
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

  virtual void CreateReconclior() { GetMockReconcilor(); }

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

  Account GetAccount(const CoreAccountId& account_id) {
    for (const auto& pair : accounts_) {
      const Account& account = pair.second;
      if (PickAccountIdForAccount(account.gaia_id, account.email) == account_id)
        return account;
    }
    NOTREACHED_IN_MIGRATION();
    return Account();
  }

  // Simulates the effect of a Multilogin call on the cookies.
  std::vector<Cookie> FakeSetAccountsInCookie(
      const signin::MultiloginParameters& parameters,
      const std::vector<Cookie>& cookies_before_reconcile) {
    std::vector<Cookie> cookies_after_reconcile;
    if (parameters.mode ==
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER) {
      for (const CoreAccountId& account_id : parameters.accounts_to_send) {
        cookies_after_reconcile.push_back(
            {GetAccount(account_id).gaia_id, true});
      }
    } else {
      std::vector<std::string> gaia_ids;
      for (const auto& account_id : parameters.accounts_to_send)
        gaia_ids.push_back(GetAccount(account_id).gaia_id);
      cookies_after_reconcile = cookies_before_reconcile;
      for (Cookie& cookie : cookies_after_reconcile) {
        if (base::Contains(gaia_ids, cookie.gaia_id)) {
          cookie.is_valid = true;
          gaia_ids.erase(base::ranges::find(gaia_ids, cookie.gaia_id));
        } else {
          DCHECK(!cookie.is_valid);
        }
      }
      for (const std::string& gaia_id : gaia_ids)
        cookies_after_reconcile.push_back({gaia_id, true});
    }
    return cookies_after_reconcile;
  }

  // Runs the test corresponding to one row of the table.
  void RunRowTest(const AccountReconcilorTestTableParam& param) {
    // Setup cookies.
    std::vector<Cookie> cookies = ParseCookieString(param.cookies);
    ConfigureCookieManagerService(cookies);
    std::vector<Cookie> cookies_after_reconcile = cookies;

    // Call list accounts now so that the next call completes synchronously.
    identity_test_env()->identity_manager()->GetAccountsInCookieJar();
    base::RunLoop().RunUntilIdle();

    // Setup tokens. This triggers listing cookies so we need to setup cookies
    // before that.
    SetupTokens(param.tokens);
    if (testing::Test::IsSkipped()) {
      return;
    }
    CreateReconclior();

    // Setup expectations.
    InSequence mock_sequence;
    bool should_logout = false;
    std::vector<CoreAccountId> accounts_to_send;
    if (param.gaia_api_calls[0] != '\0') {
      if (param.gaia_api_calls[0] == 'X') {
        should_logout = true;
        EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
            .Times(1);
        cookies_after_reconcile.clear();
      } else {
        gaia::MultiloginMode mode =
            param.gaia_api_calls[0] == 'U'
                ? gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER
                : gaia::MultiloginMode::
                      MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
        // Generate expected array of accounts in cookies and set fake gaia
        // response.
        for (int i = 1; param.gaia_api_calls[i] != '\0'; ++i) {
          const Account& account = accounts_[param.gaia_api_calls[i]];
          accounts_to_send.push_back(
              PickAccountIdForAccount(account.gaia_id, account.email));
        }
        DCHECK(!accounts_to_send.empty());
        const signin::MultiloginParameters params(mode, accounts_to_send);
        cookies_after_reconcile = FakeSetAccountsInCookie(params, cookies);
        EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params))
            .Times(1);
      }
    }
    // Reconcile.
    AccountReconcilor* reconcilor = GetMockReconcilor();
    ASSERT_TRUE(reconcilor);
    ASSERT_TRUE(reconcilor->first_execution_);
    reconcilor->first_execution_ =
        param.is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
    reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
    if (param.gaia_api_calls[0] != '\0') {
      if (should_logout) {
        SimulateLogOutFromCookieCompleted(
            reconcilor, GoogleServiceAuthError::AuthErrorNone());
      } else {
        SimulateSetAccountsInCookieCompleted(
            reconcilor, accounts_to_send,
            signin::SetAccountsInCookieResult::kSuccess);
      }
    }

    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    if (param.tokens == param.tokens_after_reconcile) {
      EXPECT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
    } else {
      // If the tokens were changed by the reconcile, a new reconcile should be
      // scheduled.
      EXPECT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
    }
    VerifyCurrentTokens(ParseTokenString(param.tokens_after_reconcile));

    std::vector<Cookie> cookies_after =
        ParseCookieString(param.cookies_after_reconcile);
    EXPECT_EQ(cookies_after, cookies_after_reconcile);

    testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

    // Another reconcile is sometimes triggered if Chrome accounts have
    // changed. Allow it to finish.
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
        .WillRepeatedly(testing::Return());
    ConfigureCookieManagerService({});
    base::RunLoop().RunUntilIdle();
  }

  std::map<char, Account> accounts_;
};

// Parameterized version of AccountReconcilorTest.
class AccountReconcilorTestTable
    : public BaseAccountReconcilorTestTable,
      public ::testing::WithParamInterface<AccountReconcilorTestTableParam> {
 protected:
  AccountReconcilorTestTable() = default;

  // Checks that reconcile is idempotent.
  void CheckReconcileIdempotent(
      const std::vector<AccountReconcilorTestTableParam>& params,
      const AccountReconcilorTestTableParam& param) {
    // Simulate another reconcile based on the results of this one: find the
    // corresponding row in the table and check that it does nothing.
    for (const AccountReconcilorTestTableParam& row : params) {
      if (row.is_first_reconcile == IsFirstReconcile::kFirst)
        continue;

      if (!(strcmp(row.tokens, param.tokens_after_reconcile) == 0 &&
            strcmp(row.cookies, param.cookies_after_reconcile) == 0)) {
        continue;
      }
      EXPECT_STREQ(row.tokens, row.tokens_after_reconcile);
      EXPECT_STREQ(row.cookies, row.cookies_after_reconcile);
      return;
    }

    ADD_FAILURE() << "Could not check that reconcile is idempotent.";
  }
};

// On Lacros, the reconcilor is always registered as reconcile is always
// enabled.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(AccountReconcilorMirrorTest, IdentityManagerRegistration) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithIdentityManager());

  identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, consent_level_for_reconcile_);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(reconcilor->IsRegisteredWithIdentityManager());
}

TEST_F(AccountReconcilorMirrorTest, Reauth) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  // Simulate reauth.  The state of the reconcilor should not change.
  auto* account_mutator =
      identity_test_env()->identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator);
  account_mutator->SetPrimaryAccount(account_info.account_id,
                                     consent_level_for_reconcile_);

  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(AccountReconcilorMirrorTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount(kFakeEmail);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

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
    // Tokens|Cookies| First Run            |Gaia calls|Tokens aft.|Cookies aft.
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "",      "A",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "*AB",   "AB",  IsFirstReconcile::kBoth,     "",     "*AB",  "AB"      },
    {  "*A",    "A",   IsFirstReconcile::kBoth,     "",     "*A" ,  "A"       },
    {  "*A",    "",    IsFirstReconcile::kBoth,     "PA",   "*A" ,  "A"       },
    {  "*A",    "B",   IsFirstReconcile::kBoth,     "UA",   "*A" ,  "A"       },
    {  "*A",    "AB",  IsFirstReconcile::kBoth,     "UA",   "*A" ,  "A"       },
    {  "*AB",   "BA",  IsFirstReconcile::kFirst,    "UAB",  "*AB",  "AB"      },
    {  "*AB",   "BA",  IsFirstReconcile::kNotFirst, "",     "*AB",  "BA"      },

    {  "*AB",   "A",   IsFirstReconcile::kBoth,     "PAB",  "*AB",  "AB"      },

    {  "*AB",   "B",   IsFirstReconcile::kFirst,    "UAB",  "*AB",  "AB"      },
    {  "*AB",   "B",   IsFirstReconcile::kNotFirst, "PBA",  "*AB",  "BA"      },

    {  "*AB",   "",    IsFirstReconcile::kBoth,     "PAB",  "*AB",  "AB"      },
    // Sync enabled, token error on primary.

    {  "*xAB",  "AB",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAB",  "BA",  IsFirstReconcile::kBoth,     "UB",   "*xAB", "B"       },
    {  "*xAB",  "A",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAB",  "B",   IsFirstReconcile::kBoth,     "",     "*xAB", "B"       },
    {  "*xAB",  "",    IsFirstReconcile::kBoth,     "PB",   "*xAB", "B"       },
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",  IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    {  "*AxB",  "A",   IsFirstReconcile::kBoth,     "",     "*A",   "A"       },
    {  "*AxB",  "",    IsFirstReconcile::kBoth,     "PA",   "*A",   "A"       },
    // The first account in cookies is swapped even when Chrome is running.
    // The swap would happen at next startup anyway and doing it earlier avoids
    // signing the user out.
    {  "*AxB",  "BA",  IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    {  "*AxB",  "B",   IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    // Sync enabled, token error on both accounts.
    {  "*xAxB", "AB",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "BA",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "A",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "B",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "",    IsFirstReconcile::kBoth,     "",     "*xA",  ""        },
    // Sync disabled.
    {  "AB",    "AB",  IsFirstReconcile::kBoth,     "",     "AB",   "AB"      },
    {  "AB",    "BA",  IsFirstReconcile::kBoth,     "",     "AB",   "BA"      },
    {  "AB",    "A",   IsFirstReconcile::kBoth,     "PAB",  "AB",   "AB"      },
    {  "AB",    "B",   IsFirstReconcile::kBoth,     "PBA",  "AB",   "BA"      },
    {  "AB",    "",    IsFirstReconcile::kBoth,     "PAB",  "AB",   "AB"      },
    // Sync disabled, token error on first account.
    {  "xAB",   "AB",  IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",   "AB",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "xAB",   "BA",  IsFirstReconcile::kBoth,     "UB",   "B",    "B"       },

    {  "xAB",   "A",   IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",   "A",   IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "xAB",   "B",   IsFirstReconcile::kBoth,     "",     "B",    "B"       },

    {  "xAB",   "",    IsFirstReconcile::kBoth,     "PB",   "B",    "B"       },
    // Sync disabled, token error on second account
    {  "AxB",   "AB",  IsFirstReconcile::kBoth,     "UA",   "A",    "A"       },

    {  "AxB",   "BA",  IsFirstReconcile::kFirst,    "UA",   "A",    "A"       },
    {  "AxB",   "BA",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "AxB",   "A",   IsFirstReconcile::kBoth,     "",     "A",    "A"       },

    {  "AxB",   "B",   IsFirstReconcile::kFirst,    "UA",   "A",    "A"       },
    {  "AxB",   "B",   IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "AxB",   "",    IsFirstReconcile::kBoth,     "PA",   "A",    "A"       },
    // Sync disabled, token error on both accounts.
    {  "xAxB",  "AB",  IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "BA",  IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "A",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "B",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "",    IsFirstReconcile::kBoth,     "",     "",     ""        },
    // Account marked as invalid in cookies.
    // No difference between cookies and tokens, do not do do anything.
    // Do not logout. Regression tests for http://crbug.com/854799
    {  "",     "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "",     "xAxB", IsFirstReconcile::kBoth,     "",     "",     "xAxB"    },
    {  "xA",   "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "xAB",  "xAB",  IsFirstReconcile::kBoth,     "",     "B",    "xAB"     },
    {  "AxB",  "AxC",  IsFirstReconcile::kBoth,     "",     "A",    "AxC"     },
    {  "B",    "xAB",  IsFirstReconcile::kBoth,     "",     "B",    "xAB"     },
    {  "*xA",  "xA",   IsFirstReconcile::kBoth,     "",     "*xA",  "xA"      },
    {  "*xA",  "xB",   IsFirstReconcile::kBoth,     "",     "*xA",  "xB"      },
    {  "*xAB", "xAB",  IsFirstReconcile::kBoth,     "",     "*xAB", "xAB"     },
    {  "*AxB", "xBA",  IsFirstReconcile::kNotFirst, "",     "*A",   "xBA"     },
    // Appending a new cookie after the invalid one.
    {  "B",    "xA",   IsFirstReconcile::kBoth,     "PB",   "B",    "xAB"     },
    {  "xAB",  "xA",   IsFirstReconcile::kBoth,     "PB",   "B",    "xAB"     },
    // Refresh existing cookies.
    {  "AB",   "xAB",  IsFirstReconcile::kBoth,     "PAB",  "AB",   "AB"      },
    {  "*AB",  "xBxA", IsFirstReconcile::kNotFirst, "PBA",  "*AB",  "BA"      },
    // Appending and invalidating cookies at the same time.
    {  "xAB",  "xAC",  IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",  "xAC",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "xAB",  "AxC",  IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",  "AxC",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "*xAB", "xABC", IsFirstReconcile::kFirst,    "UB",   "*xAB", "B"       },
    {  "*xAB", "xABC", IsFirstReconcile::kNotFirst, "X",    "*xA",  ""        },

    {  "xAB",  "xABC", IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",  "xABC", IsFirstReconcile::kNotFirst, "X",    "",     ""        },
    // Miscellaneous cases.
    // Check that unknown Gaia accounts are signed o.
    {  "*A",   "AB",   IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    // Check that Gaia default account is kept in first position.
    {  "AB",   "BC",   IsFirstReconcile::kBoth,     "UBA",  "AB",   "BA"      },
    // Check that Gaia cookie order is preserved for B.
    {  "*ABC", "CB",   IsFirstReconcile::kFirst,    "UABC", "*ABC", "ABC"     },
    // TODO(crbug.com/40149592): Merge session should do XCB instead.
    {  "xABC", "ABC",  IsFirstReconcile::kFirst,    "UCB",  "BC",   "CB"      },
    // Check that order in the chrome_accounts is not important.
    {  "A*B",  "",     IsFirstReconcile::kBoth,     "PBA",  "A*B",  "BA"      },
    {  "*xBA", "BA",   IsFirstReconcile::kFirst,    "X",    "*xB",  ""        },
    // Required for idempotency check.
    {  "",     "",     IsFirstReconcile::kNotFirst, "",     "",     ""        },
    {  "",     "xA",   IsFirstReconcile::kNotFirst, "",     "",     "xA"      },
    {  "",     "xB",   IsFirstReconcile::kNotFirst, "",     "",     "xB"      },
    {  "",     "xAxB", IsFirstReconcile::kNotFirst, "",     "",     "xAxB"    },
    {  "",     "xBxA", IsFirstReconcile::kNotFirst, "",     "",     "xBxA"    },
    {  "*A",   "A",    IsFirstReconcile::kNotFirst, "",     "*A",   "A"       },
    {  "*A",   "xBA",  IsFirstReconcile::kNotFirst, "",     "*A",   "xBA"     },
    {  "*A",   "AxB",  IsFirstReconcile::kNotFirst, "",     "*A",   "AxB"     },
    {  "A",    "A",    IsFirstReconcile::kNotFirst, "",     "A",    "A"       },
    {  "A",    "xBA",  IsFirstReconcile::kNotFirst, "",     "A",    "xBA"     },
    {  "A",    "AxB",  IsFirstReconcile::kNotFirst, "",     "A",    "AxB"     },
    {  "B",    "B",    IsFirstReconcile::kNotFirst, "",     "B",    "B"       },
    {  "B",    "xAB",  IsFirstReconcile::kNotFirst, "",     "B",    "xAB"     },
    {  "B",    "BxA",  IsFirstReconcile::kNotFirst, "",     "B",    "BxA"     },
    {  "*xA",  "",     IsFirstReconcile::kNotFirst, "",     "*xA",  ""        },
    {  "*xA",  "xAxB", IsFirstReconcile::kNotFirst, "",     "*xA",  "xAxB"    },
    {  "*xA",  "xBxA", IsFirstReconcile::kNotFirst, "",     "*xA",  "xBxA"    },
    {  "*xA",  "xA",   IsFirstReconcile::kNotFirst, "",     "*xA",  "xA"      },
    {  "*xA",  "xB",   IsFirstReconcile::kNotFirst, "",     "*xA",  "xB"      },
    {  "*xAB", "B",    IsFirstReconcile::kNotFirst, "",     "*xAB", "B"       },
    {  "*xAB", "BxA",  IsFirstReconcile::kNotFirst, "",     "*xAB", "BxA"     },
    {  "*xAB", "xAB",  IsFirstReconcile::kNotFirst, "",     "*xAB", "xAB"     },
    {  "*xAB", "xABxC",IsFirstReconcile::kNotFirst, "",     "*xAB", "xABxC"   },
    {  "*xB",  "",     IsFirstReconcile::kNotFirst, "",     "*xB",  ""        },
    {  "A*B",  "BA",   IsFirstReconcile::kNotFirst, "",     "A*B",  "BA"      },
    {  "A*B",  "AB",   IsFirstReconcile::kNotFirst, "",     "A*B",  "AB"      },
    {  "A",    "AxC",  IsFirstReconcile::kNotFirst, "",     "A",    "AxC"     },
    {  "AB",   "BxCA", IsFirstReconcile::kNotFirst, "",     "AB",   "BxCA"    },
    {  "B",    "xABxC",IsFirstReconcile::kNotFirst, "",     "B",    "xABxC"   },
    {  "B",    "xAxCB",IsFirstReconcile::kNotFirst, "",     "B",    "xAxCB"   },
    {  "*ABC", "ACB",  IsFirstReconcile::kNotFirst, "",     "*ABC", "ACB"     },
    {  "*ABC", "ABC",  IsFirstReconcile::kNotFirst, "",     "*ABC", "ABC"     },
    {  "BC",   "BC",   IsFirstReconcile::kNotFirst, "",     "BC",   "BC"      },
    {  "BC",   "CB",   IsFirstReconcile::kNotFirst, "",     "BC",   "CB"      },
};
// clang-format on

// Parameterized version of AccountReconcilorTest that tests Dice
// implementation with Multilogin endpoint.
class AccountReconcilorTestDiceMultilogin : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestDiceMultilogin() {
    feature_list_.InitAndDisableFeature(
        switches::kExplicitBrowserSigninUIOnDesktop);

    // TODO(https://crbug.com.1464264): Migrate away from `ConsentLevel::kSync`
    // on desktop platforms.
    consent_level_for_reconcile_ = signin::ConsentLevel::kSync;
  }

  AccountReconcilorTestDiceMultilogin(
      const AccountReconcilorTestDiceMultilogin&) = delete;
  AccountReconcilorTestDiceMultilogin& operator=(
      const AccountReconcilorTestDiceMultilogin&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestDiceMultilogin, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  CheckReconcileIdempotent(kDiceParams, GetParam());
  RunRowTest(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    DiceTableMultilogin,
    AccountReconcilorTestDiceMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kDiceParams)));

// Parameterized version of AccountReconcilorTest that tests Dice
// implementation with Multilogin endpoint.
class AccountReconcilorTestUnoMultilogin : public AccountReconcilorTestTable {
 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kUnoParams = {
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
    // Tokens|Cookies| First Run            |Gaia calls|Tokens aft.|Cookies aft.
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "",      "A",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "*AB",   "AB",  IsFirstReconcile::kBoth,     "",     "*AB",  "AB"      },
    {  "*A",    "A",   IsFirstReconcile::kBoth,     "",     "*A" ,  "A"       },
    {  "*A",    "",    IsFirstReconcile::kBoth,     "PA",   "*A" ,  "A"       },
    {  "*A",    "B",   IsFirstReconcile::kBoth,     "UA",   "*A" ,  "A"       },
    {  "*A",    "AB",  IsFirstReconcile::kBoth,     "UA",   "*A" ,  "A"       },
    {  "*AB",   "BA",  IsFirstReconcile::kFirst,    "UAB",  "*AB",  "AB"      },
    {  "*AB",   "BA",  IsFirstReconcile::kNotFirst, "",     "*AB",  "BA"      },

    {  "*AB",   "A",   IsFirstReconcile::kBoth,     "PAB",  "*AB",  "AB"      },

    {  "*AB",   "B",   IsFirstReconcile::kFirst,    "UAB",  "*AB",  "AB"      },
    {  "*AB",   "B",   IsFirstReconcile::kNotFirst, "PBA",  "*AB",  "BA"      },

    {  "*AB",   "",    IsFirstReconcile::kBoth,     "PAB",  "*AB",  "AB"      },
    // Signed in, token error on primary.
    {  "*xAB",  "AB",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAB",  "BA",  IsFirstReconcile::kBoth,     "UB",   "*xAB", "B"       },
    {  "*xAB",  "A",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAB",  "B",   IsFirstReconcile::kBoth,     "",     "*xAB", "B"       },
    {  "*xAB",  "",    IsFirstReconcile::kBoth,     "PB",   "*xAB", "B"       },
    // Signed in, token error on secondary.
    {  "*AxB",  "AB",  IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    {  "*AxB",  "A",   IsFirstReconcile::kBoth,     "",     "*A",   "A"       },
    {  "*AxB",  "",    IsFirstReconcile::kBoth,     "PA",   "*A",   "A"       },
    // The first account in cookies is swapped even when Chrome is running.
    // The swap would happen at next startup anyway and doing it earlier avoids
    // signing the user out.
    {  "*AxB",  "BA",  IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    {  "*AxB",  "B",   IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    // Signed in, token error on both accounts.
    {  "*xAxB", "AB",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "BA",  IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "A",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "B",   IsFirstReconcile::kBoth,     "X",    "*xA",  ""        },
    {  "*xAxB", "",    IsFirstReconcile::kBoth,     "",     "*xA",  ""        },
    // Signed out.
    {  "AB",    "AB",  IsFirstReconcile::kBoth,     "",     "AB",   "AB"      },
    {  "AB",    "BA",  IsFirstReconcile::kBoth,     "",     "AB",   "BA"      },
    {  "AB",    "A",   IsFirstReconcile::kBoth,     "",     "A",    "A"       },
    {  "AB",    "B",   IsFirstReconcile::kBoth,     "",     "B",    "B"       },
    {  "AB",    "",    IsFirstReconcile::kBoth,     "",     "",     ""        },
    // Signed out, token error on first account.
    {  "xAB",   "AB",  IsFirstReconcile::kFirst,    "UB",   "B",    "B"       },
    {  "xAB",   "AB",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "xAB",   "BA",  IsFirstReconcile::kBoth,     "UB",   "B",    "B"       },

    {  "xAB",   "A",   IsFirstReconcile::kBoth,     "X",    "",     ""        },

    {  "xAB",   "B",   IsFirstReconcile::kBoth,     "",     "B",    "B"       },

    {  "xAB",   "",    IsFirstReconcile::kBoth,     "",     "",     ""        },
    // Signed out, token error on second account
    {  "AxB",   "AB",  IsFirstReconcile::kBoth,     "UA",   "A",    "A"       },

    {  "AxB",   "BA",  IsFirstReconcile::kFirst,    "UA",   "A",    "A"       },
    {  "AxB",   "BA",  IsFirstReconcile::kNotFirst, "X",    "",     ""        },

    {  "AxB",   "A",   IsFirstReconcile::kBoth,     "",     "A",    "A"       },

    {  "AxB",   "B",   IsFirstReconcile::kBoth,     "X",    "",     ""        },

    {  "AxB",   "",    IsFirstReconcile::kBoth,     "",     "",     ""        },
    // Signed out, token error on both accounts.
    {  "xAxB",  "AB",  IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "BA",  IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "A",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "B",   IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAxB",  "",    IsFirstReconcile::kBoth,     "",     "",     ""        },
    // Account marked as invalid in cookies.
    // No difference between cookies and tokens, do not do do anything.
    // Do not logout. Regression tests for http://crbug.com/854799
    {  "",     "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "",     "xAxB", IsFirstReconcile::kBoth,     "",     "",     "xAxB"    },
    {  "xA",   "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "xAB",  "xAB",  IsFirstReconcile::kBoth,     "",     "B",    "xAB"     },
    {  "AxB",  "AxC",  IsFirstReconcile::kBoth,     "",     "A",    "AxC"     },
    {  "B",    "xAB",  IsFirstReconcile::kBoth,     "",     "B",    "xAB"     },
    {  "*xA",  "xA",   IsFirstReconcile::kBoth,     "",     "*xA",  "xA"      },
    {  "*xA",  "xB",   IsFirstReconcile::kBoth,     "",     "*xA",  "xB"      },
    {  "*xAB", "xAB",  IsFirstReconcile::kBoth,     "",     "*xAB", "xAB"     },
    {  "*AxB", "xBA",  IsFirstReconcile::kNotFirst, "",     "*A",   "xBA"     },

    // No-op.
    {  "B",    "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "xAB",  "xA",   IsFirstReconcile::kBoth,     "",     "",     "xA"      },
    {  "AB",   "xAB",  IsFirstReconcile::kBoth,     "",     "B",    "xAB"     },

    // Refresh existing cookies.
    {  "*AB",  "xBxA", IsFirstReconcile::kNotFirst, "PBA",  "*AB",  "BA"      },

    {  "xAB",  "xAC",  IsFirstReconcile::kBoth,     "X",    "",     ""        },
    {  "xAB",  "AxC",  IsFirstReconcile::kBoth,     "X",    "",     ""        },

    // Appending and invalidating cookies at the same time.
    {  "*xAB", "xABC", IsFirstReconcile::kFirst,    "UB",   "*xAB", "B"       },
    {  "*xAB", "xABC", IsFirstReconcile::kNotFirst, "X",    "*xA",  ""        },

    // Miscellaneous cases.
    {  "xAB",  "xABC", IsFirstReconcile::kBoth,    "UB",    "B",    "B"       },
    // Check that unknown Gaia accounts are signed o.
    {  "*A",   "AB",   IsFirstReconcile::kBoth,     "UA",   "*A",   "A"       },
    // Check that Gaia default account is kept in first position.
    {  "AB",   "BC",   IsFirstReconcile::kBoth,     "UB",   "B",    "B"       },
    // Check that Gaia cookie order is preserved for B.
    {  "*ABC", "CB",   IsFirstReconcile::kFirst,    "UABC", "*ABC", "ABC"     },
    // TODO(crbug.com/40149592): Merge session should do XCB instead.
    {  "xABC", "ABC",  IsFirstReconcile::kFirst,    "UCB",  "BC",   "CB"      },
    // Check that order in the chrome_accounts is not important.
    {  "A*B",  "",     IsFirstReconcile::kBoth,     "PBA",  "A*B",  "BA"      },
    {  "*xBA", "BA",   IsFirstReconcile::kFirst,    "X",    "*xB",  ""        },
    // Required for idempotency check.
    {  "",     "",     IsFirstReconcile::kNotFirst, "",     "",     ""        },
    {  "",     "xA",   IsFirstReconcile::kNotFirst, "",     "",     "xA"      },
    {  "",     "xB",   IsFirstReconcile::kNotFirst, "",     "",     "xB"      },
    {  "",     "xAxB", IsFirstReconcile::kNotFirst, "",     "",     "xAxB"    },
    {  "",     "xBxA", IsFirstReconcile::kNotFirst, "",     "",     "xBxA"    },
    {  "*A",   "A",    IsFirstReconcile::kNotFirst, "",     "*A",   "A"       },
    {  "*A",   "xBA",  IsFirstReconcile::kNotFirst, "",     "*A",   "xBA"     },
    {  "*A",   "AxB",  IsFirstReconcile::kNotFirst, "",     "*A",   "AxB"     },
    {  "A",    "A",    IsFirstReconcile::kNotFirst, "",     "A",    "A"       },
    {  "A",    "xBA",  IsFirstReconcile::kNotFirst, "",     "A",    "xBA"     },
    {  "A",    "AxB",  IsFirstReconcile::kNotFirst, "",     "A",    "AxB"     },
    {  "B",    "B",    IsFirstReconcile::kNotFirst, "",     "B",    "B"       },
    {  "B",    "xAB",  IsFirstReconcile::kNotFirst, "",     "B",    "xAB"     },
    {  "B",    "BxA",  IsFirstReconcile::kNotFirst, "",     "B",    "BxA"     },
    {  "*xA",  "",     IsFirstReconcile::kNotFirst, "",     "*xA",  ""        },
    {  "*xA",  "xAxB", IsFirstReconcile::kNotFirst, "",     "*xA",  "xAxB"    },
    {  "*xA",  "xBxA", IsFirstReconcile::kNotFirst, "",     "*xA",  "xBxA"    },
    {  "*xA",  "xA",   IsFirstReconcile::kNotFirst, "",     "*xA",  "xA"      },
    {  "*xA",  "xB",   IsFirstReconcile::kNotFirst, "",     "*xA",  "xB"      },
    {  "*xAB", "B",    IsFirstReconcile::kNotFirst, "",     "*xAB", "B"       },
    {  "*xAB", "BxA",  IsFirstReconcile::kNotFirst, "",     "*xAB", "BxA"     },
    {  "*xAB", "xAB",  IsFirstReconcile::kNotFirst, "",     "*xAB", "xAB"     },
    {  "*xAB", "xABxC",IsFirstReconcile::kNotFirst, "",     "*xAB", "xABxC"   },
    {  "*xB",  "",     IsFirstReconcile::kNotFirst, "",     "*xB",  ""        },
    {  "A*B",  "BA",   IsFirstReconcile::kNotFirst, "",     "A*B",  "BA"      },
    {  "A*B",  "AB",   IsFirstReconcile::kNotFirst, "",     "A*B",  "AB"      },
    {  "A",    "AxC",  IsFirstReconcile::kNotFirst, "",     "A",    "AxC"     },
    {  "AB",   "BxCA", IsFirstReconcile::kNotFirst, "",     "AB",   "BxCA"    },
    {  "B",    "xABxC",IsFirstReconcile::kNotFirst, "",     "B",    "xABxC"   },
    {  "B",    "xAxCB",IsFirstReconcile::kNotFirst, "",     "B",    "xAxCB"   },
    {  "*ABC", "ACB",  IsFirstReconcile::kNotFirst, "",     "*ABC", "ACB"     },
    {  "*ABC", "ABC",  IsFirstReconcile::kNotFirst, "",     "*ABC", "ABC"     },
    {  "BC",   "BC",   IsFirstReconcile::kNotFirst, "",     "BC",   "BC"      },
    {  "BC",   "CB",   IsFirstReconcile::kNotFirst, "",     "BC",   "CB"      },
};
// clang-format on

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestUnoMultilogin, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  CheckReconcileIdempotent(kDiceParams, GetParam());
  RunRowTest(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AccountReconcilorTestUnoMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kUnoParams)));

class AccountReconcilorDiceTest : public AccountReconcilorTest {
 public:
  AccountReconcilorDiceTest() {
    // TODO(https://crbug.com.1464264): Migrate away from `ConsentLevel::kSync`
    // on desktop platforms.
    consent_level_for_reconcile_ = signin::ConsentLevel::kSync;
    SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  }

  AccountReconcilorDiceTest(const AccountReconcilorDiceTest&) = delete;
  AccountReconcilorDiceTest& operator=(const AccountReconcilorDiceTest&) =
      delete;
};

TEST_F(AccountReconcilorDiceTest, ClearPrimaryAccountNotAllowed) {
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(1);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  test_signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}

// Tests that the AccountReconcilor is always registered.
TEST_F(AccountReconcilorDiceTest, DiceTokenServiceRegistration) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, consent_level_for_reconcile_);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());

  // Reconcilor should not logout all accounts from the cookies when
  // the primary account is cleared in IdentityManager.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(::testing::_))
      .Times(0);

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

TEST_F(AccountReconcilorDiceTest, DiceReconcileWithoutSignin) {
  // Add a token in Chrome but do not sign in. Making account available (setting
  // a refresh token) triggers listing cookies so we need to setup cookies
  // before that.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  const CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kFakeEmail).account_id;

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // The reconcilor does not rebuild cookies while signed out.
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
        .Times(0);
  } else {
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  base::RunLoop().RunUntilIdle();
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    SimulateSetAccountsInCookieCompleted(
        reconcilor, accounts_to_send,
        signin::SetAccountsInCookieResult::kSuccess);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Checks that nothing happens when there is no Chrome account and no Gaia
// cookie.
TEST_F(AccountReconcilorDiceTest, DiceReconcileNoop) {
  // No Chrome account and no cookie.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Tests that the first Gaia account is re-used when possible.
TEST_F(AccountReconcilorDiceTest, DiceReconcileReuseGaiaFirstAccount) {
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Add an invalid primary account so that the reconcilor is in a mode where
    // it rebuilds cookies.
    AccountInfo primary_account_info =
        identity_test_env()->MakePrimaryAccountAvailable(
            "primary@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  }

  // Add account "other" to the Gaia cookie.
  signin::SetListAccountsResponseTwoAccounts(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2), "foo@gmail.com",
      "9999", &test_url_loader_factory_);

  // Add accounts "user" and "other" to the token service.
  const AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail);
  const CoreAccountId account_id_1 = account_info_1.account_id;
  const AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id_2 = account_info_2.account_id;

  auto* identity_manager = identity_test_env()->identity_manager();
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_LE(2u, accounts.size());
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_1));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_2));

    std::vector<CoreAccountId> accounts_to_send = {account_id_2, account_id_1};
    // Send accounts to Gaia in order of chrome accounts. Account 2 is added
    // first.
    const signin::MultiloginParameters params(
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Tests that the first account is kept in cache and reused when cookies are
// lost.
TEST_F(AccountReconcilorDiceTest, DiceLastKnownFirstAccount) {
  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Add an invalid primary account so that the reconcilor is in a mode where
    // it rebuilds cookies.
    AccountInfo primary_account_info =
        identity_test_env()->MakePrimaryAccountAvailable(
            "primary@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  }

  // Add accounts to the token service and the Gaia cookie in a different order.
  // Making account available (setting a refresh token) triggers listing cookies
  // so we need to setup cookies before that.
  signin::SetListAccountsResponseTwoAccounts(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2), kFakeEmail,
      signin::GetTestGaiaIdForEmail(kFakeEmail), &test_url_loader_factory_);

  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail);
  const CoreAccountId account_id_1 = account_info_1.account_id;
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id_2 = account_info_2.account_id;

  auto* identity_manager = identity_test_env()->identity_manager();
  std::vector<CoreAccountInfo> accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  ASSERT_LE(2u, accounts.size());

  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_1));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_2));

  // Do one reconcile. It should do nothing but to populating the last known
  // account.
  {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
        .Times(0);

    AccountReconcilor* reconcilor = GetMockReconcilor();
    reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
  }

  // Delete the cookies.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);

  // Since Gaia can't know about cached account, make sure that we reorder
  // chrome accounts accordingly even in PRESERVE mode.
  std::vector<CoreAccountId> accounts_to_send = {account_id_2, account_id_1};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Checks that the reconcilor does not log out unverified accounts.
TEST_F(AccountReconcilorDiceTest, UnverifiedAccountNoop) {
  // Add a unverified account to the Gaia cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {kFakeEmail, kFakeGaiaId, true /* valid */, false /* signed_out */,
       false /* verified */},
      &test_url_loader_factory_);

  // Check that nothing happens.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Checks that the reconcilor does not log out unverified accounts when adding
// a new account to the Gaia cookie.
TEST_F(AccountReconcilorDiceTest, UnverifiedAccountMerge) {
  // Add a unverified account to the Gaia cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {.email = kFakeEmail,
       .gaia_id = kFakeGaiaId,
       .valid = true,
       .signed_out = false,
       .verified = false},
      &test_url_loader_factory_);

  // Add a token to Chrome.
  const CoreAccountId chrome_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kFakeEmail2,
                                        signin::ConsentLevel::kSignin)
          .account_id;

  // In PRESERVE mode it is up to Gaia to not delete existing accounts in
  // cookies and not sign out unveridied accounts.
  std::vector<CoreAccountId> accounts_to_send = {chrome_account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

TEST_F(AccountReconcilorDiceTest, DeleteCookie) {
  const CoreAccountId primary_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kFakeEmail,
                                        consent_level_for_reconcile_)
          .account_id;
  const CoreAccountId secondary_account_id =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).account_id;

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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
class AccountReconcilorDiceTestForSupervisedUsers
    : public AccountReconcilorDiceTest {
 public:
  AccountReconcilorDiceTestForSupervisedUsers() {
    feature_list_.InitAndDisableFeature(
        switches::kExplicitBrowserSigninUIOnDesktop);
  }

  ~AccountReconcilorDiceTestForSupervisedUsers() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AccountReconcilorDiceTestForSupervisedUsers,
       DeleteCookieForNonSyncingSupervisedUsers) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
}

TEST_F(AccountReconcilorDiceTestForSupervisedUsers,
       DeleteCookieForSyncingSupervisedUsers) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, consent_level_for_reconcile_);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();

  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

class AccountReconcilorDiceTestWithUnoDesktop
    : public AccountReconcilorDiceTest,
      public base::test::WithFeatureOverride {
 public:
  AccountReconcilorDiceTestWithUnoDesktop()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop) {}

  ~AccountReconcilorDiceTestWithUnoDesktop() override = default;

  bool is_uno_desktop_enabled() const { return IsParamFeatureEnabled(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    AccountReconcilorDiceTestWithUnoDesktop);

TEST_P(AccountReconcilorDiceTestWithUnoDesktop, DeleteCookieForSignedInUser) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_EQ(is_uno_desktop_enabled(),
            identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_EQ(
      is_uno_desktop_enabled(),
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop, DeleteCookieForSyncingUser) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSync);

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       CookieSettingMigrationExplicitSignin) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  ASSERT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Explicit signin is auto-migrated.
  GetMockReconcilor();
  EXPECT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(
                prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       CookieSettingMigrationExplicitSigninWithClearOnExit) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  ASSERT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));
  test_signin_client()->set_are_signin_cookies_deleted_on_exit(true);

  // Explicit signin is not auto-migrated when the setting exists.
  content_settings::Observer* reconcilor = GetMockReconcilor();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // Changing cookie settings should trigger the migration.
  test_signin_client()->set_are_signin_cookies_deleted_on_exit(false);
  reconcilor->OnContentSettingChanged(
      /*primary_pattern=*/ContentSettingsPattern(),
      /*secondary_pattern=*/ContentSettingsPattern(),
      ContentSettingsTypeSet(ContentSettingsType::COOKIES));
  EXPECT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(
                prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       CookieSettingMigrationImplicitSignin) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  pref_service()->ClearPref(prefs::kExplicitBrowserSignin);
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Implicit signin is not auto-migrated.
  content_settings::Observer* reconcilor = GetMockReconcilor();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // Changing cookie settings should not trigger the migration.
  reconcilor->OnContentSettingChanged(
      /*primary_pattern=*/ContentSettingsPattern(),
      /*secondary_pattern=*/ContentSettingsPattern(),
      ContentSettingsTypeSet(ContentSettingsType::COOKIES));
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       CookieSettingMigrationSignedOut) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // Signed out state is auto-migrated.
  GetMockReconcilor();
  EXPECT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(
                prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop, CookieSettingMigrationSync) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSync);
  ASSERT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Sync is not auto-migrated.
  GetMockReconcilor();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // But is migrated on signout. Regression test for b/350888149.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(
                prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       CookieSettingMigrationExplicitPref) {
  ASSERT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  pref_service()->ClearPref(prefs::kExplicitBrowserSignin);
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Implicit signin is not auto-migrated.
  GetMockReconcilor();
  EXPECT_FALSE(pref_service()->GetBoolean(
      prefs::kCookieClearOnExitMigrationNoticeComplete));

  // Make the signin explicit, this triggers the migration.
  pref_service()->SetBoolean(prefs::kExplicitBrowserSignin, true);
  EXPECT_EQ(is_uno_desktop_enabled(),
            pref_service()->GetBoolean(
                prefs::kCookieClearOnExitMigrationNoticeComplete));
}

TEST_P(AccountReconcilorDiceTestWithUnoDesktop,
       PendingStateThenClearPrimaryAccount) {
  if (!switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    GTEST_SKIP();
  }

  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();
  ASSERT_TRUE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_info.account_id));
  ASSERT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 1u);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop run_loop;
  signin::TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenRemovedCallback(
      run_loop.QuitClosure());

  identity_manager->GetPrimaryAccountMutator()
      ->RemovePrimaryAccountButKeepTokens(
          signin_metrics::ProfileSignout::kTest);
  ASSERT_FALSE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));

  run_loop.Run();
  ASSERT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 0u);
}

const std::vector<AccountReconcilorTestTableParam>
    kDiceParamsUnoPreChromeSignIn = {
        // clang-format off
        // See `kDiceParams` above for detailed params format.
        // First account in cookie doesn't have a token.
        {  "",     "A",     IsFirstReconcile::kBoth,      "X",   "",   ""     },
        {  "xA",   "A",     IsFirstReconcile::kBoth,      "X",   "",   ""     },
        {  "B",    "AB",    IsFirstReconcile::kFirst,     "UB",  "B",  "B"    },
        {  "B",    "AB",    IsFirstReconcile::kNotFirst,  "X",   "",   ""     },
        {  "xAB",  "A",     IsFirstReconcile::kBoth,      "X",   "" ,  ""     },

        // Invalid first account in cookie doesn't have a token.
        {  "xA",   "xA",    IsFirstReconcile::kBoth,      "",    "",   "xA"   },
        {  "",     "xAB",   IsFirstReconcile::kBoth,      "X",   "",   ""     },
        {  "B",    "xAB",   IsFirstReconcile::kBoth,      "",    "B",  "xAB"  },
        {  "B",    "xABC",  IsFirstReconcile::kBoth,      "UB",  "B",  "B"    },

        // Invalid first account in cookie.
        {  "A",    "xA",    IsFirstReconcile::kBoth,      "",    "",   "xA"   },
        {  "A",    "xAB",   IsFirstReconcile::kBoth,      "X",   "",   ""     },
        {  "AB",   "xABC",  IsFirstReconcile::kBoth,      "UB",  "B",  "B"    },

        // Tokens not in the cookie.
        {  "CB",   "B",     IsFirstReconcile::kBoth,      "",    "B",  "B"    },
        {  "AB",   "",      IsFirstReconcile::kBoth,      "",    "" ,  ""     },
        {  "AB",   "AxB",   IsFirstReconcile::kBoth,      "",    "A",  "AxB"  },

        // Tokens and cookies need update.
        {  "A",    "B",     IsFirstReconcile::kBoth,      "X",   "" ,  ""     },

        // Secondary account without token.
        {  "B",    "BC",    IsFirstReconcile::kBoth,      "UB",  "B",  "B"    },

        // Consistent.
        // Added to check Reconcile is Idempotent.
        {  "B",    "B",    IsFirstReconcile::kBoth,       "",    "B",  "B"    },
        {  "",     "",     IsFirstReconcile::kBoth,       "",    "",   ""     },
        {  "",     "xA",   IsFirstReconcile::kBoth,       "",    "",   "xA"   },
        {  "A",    "AxB",  IsFirstReconcile::kBoth,       "",    "A",  "AxB"  },

        // clang-format on
};
class AccountReconcilorTestDiceExplicitBrowserSignin
    : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestDiceExplicitBrowserSignin() {
    consent_level_for_reconcile_ = signin::ConsentLevel::kSignin;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

using AccountReconcilorTestDicePreChromeSignIn =
    AccountReconcilorTestDiceExplicitBrowserSignin;

// Checks one row of the `kDiceParamsUnoPreChromeSignIn` table above.
TEST_P(AccountReconcilorTestDicePreChromeSignIn, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  CheckReconcileIdempotent(kDiceParamsUnoPreChromeSignIn, GetParam());
  RunRowTest(GetParam());
}

INSTANTIATE_TEST_SUITE_P(,
                         AccountReconcilorTestDicePreChromeSignIn,
                         ::testing::ValuesIn(GenerateTestCasesFromParams(
                             kDiceParamsUnoPreChromeSignIn)));

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kMirrorParams = {
// This table encodes the initial state and expectations of a reconcile.
// See kDiceParams for documentation of the syntax.
// -----------------------------------------------------------------------------
// Tokens | Cookies | First Run          |Gaia calls|Tokens after| Cookies after
// -----------------------------------------------------------------------------

// First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
// tokens. Make the Sync account the default account in the Gaia cookie.
// Sync enabled.
{  "*AB",   "AB",   IsFirstReconcile::kBoth, "",          "*AB",         "AB"},
{  "*AB",   "BA",   IsFirstReconcile::kBoth, "UAB",       "*AB",         "AB"},
{  "*AB",   "A",    IsFirstReconcile::kBoth, "UAB",       "*AB",         "AB"},
{  "*AB",   "B",    IsFirstReconcile::kBoth, "UAB",       "*AB",         "AB"},
{  "*AB",   "",     IsFirstReconcile::kBoth, "UAB",       "*AB",         "AB"},
// Sync enabled, token error on primary.
// Sync enabled, token error on secondary.
{  "*AxB",  "AB",   IsFirstReconcile::kBoth, "UA",        "*AxB",        "A"},
{  "*AxB",  "BA",   IsFirstReconcile::kBoth, "UA",        "*AxB",        "A"},
{  "*AxB",  "A",    IsFirstReconcile::kBoth, "",          "*AxB",        "A"},
{  "*AxB",  "B",    IsFirstReconcile::kBoth, "UA",        "*AxB",        "A"},
{  "*AxB",  "",     IsFirstReconcile::kBoth, "UA",        "*AxB",        "A"},

// Cookies can be refreshed in pace, without logout.
{  "*AB",   "xBxA", IsFirstReconcile::kBoth, "UAB",       "*AB",         "AB"},

// Check that unknown Gaia accounts are signed out.
{  "*A",    "AB",   IsFirstReconcile::kBoth, "UA",        "*A",          "A"},
// Check that the previous case is idempotent.
{  "*A",    "A",    IsFirstReconcile::kBoth, "",          "*A",          "A"},

// On Lacros, the reconcilor is enabled even if there is no account, or if the
// primary account is in error.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
{  "",      "",     IsFirstReconcile::kBoth, "",          "",            ""},
{  "*xA",   "",     IsFirstReconcile::kBoth, "",          "*xA",         ""},
{  "*xAB",  "",     IsFirstReconcile::kBoth, "",          "*xAB",        ""},
{  "",      "A",    IsFirstReconcile::kBoth, "X",         "",            ""},
{  "*xA",   "A",    IsFirstReconcile::kBoth, "X",         "*xA",         ""},
{  "*xAB",  "AB",   IsFirstReconcile::kBoth, "X",         "*xAB",        ""},
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};
// clang-format on

// Parameterized version of AccountReconcilorTest that tests Mirror
// implementation with Multilogin endpoint.
class AccountReconcilorTestMirrorMultilogin
    : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestMirrorMultilogin() = default;

  AccountReconcilorTestMirrorMultilogin(
      const AccountReconcilorTestMirrorMultilogin&) = delete;
  AccountReconcilorTestMirrorMultilogin& operator=(
      const AccountReconcilorTestMirrorMultilogin&) = delete;
};

// Checks one row of the kMirrorParams table above.
TEST_P(AccountReconcilorTestMirrorMultilogin, TableRowTest) {
  // Enable Mirror.
  SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
  CheckReconcileIdempotent(kMirrorParams, GetParam());
  RunRowTest(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    MirrorTableMultilogin,
    AccountReconcilorTestMirrorMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kMirrorParams)));

// Tests that reconcile cannot start before the tokens are loaded, and is
// automatically started when tokens are loaded.
TEST_F(AccountReconcilorMirrorTest, TokensNotLoaded) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).account_id;
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  identity_test_env()->ResetToAccountsNotYetLoadedFromDiskState();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

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
  EXPECT_EQ(AccountReconcilor::Trigger::kTokensLoaded, reconcilor->trigger_);
  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

TEST_F(AccountReconcilorMirrorTest, GetAccountsFromCookieSuccess) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
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

  ASSERT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_EQ(1u,
            accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
                .size());
  ASSERT_EQ(account_id, accounts_in_cookie_jar_info
                            .GetPotentiallyInvalidSignedInAccounts()[0]
                            .id);
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.GetSignedOutAccounts().size());
}

// Checks that calling EnableReconcile() while the reconcilor is already running
// doesn't have any effect. Regression test for https://crbug.com/1043651
TEST_F(AccountReconcilorMirrorTest, EnableReconcileWhileAlreadyRunning) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
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

  ASSERT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  reconcilor->EnableReconcile();
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_EQ(1u,
            accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
                .size());
  ASSERT_EQ(account_id, accounts_in_cookie_jar_info
                            .GetPotentiallyInvalidSignedInAccounts()[0]
                            .id);
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.GetSignedOutAccounts().size());
}

TEST_F(AccountReconcilorMirrorTest, GetAccountsFromCookieFailure) {
  ConnectProfileToAccount(kFakeEmail);
  signin::SetListAccountsResponseWithUnexpectedServiceResponse(
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_EQ(0u,
            accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
                .size());
  ASSERT_EQ(0u, accounts_in_cookie_jar_info.GetSignedOutAccounts().size());
  // List accounts retries once on |UNEXPECTED_SERVICE_RESPONSE| errors with
  // backoff protection.
  task_environment()->FastForwardBy(base::Seconds(2));
  ASSERT_EQ(AccountReconcilorState::kError, reconcilor->GetState());
}

// Regression test for https://crbug.com/923716
TEST_F(AccountReconcilorMirrorTest, ExtraCookieChangeNotification) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
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

  ASSERT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());

  // Add extra cookie change notification. Reconcilor should ignore it.
  gaia::ListedAccount listed_account =
      ListedAccountFromCookieParams(cookie_params, account_id);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      /*accounts_are_fresh=*/true, /*accounts=*/{listed_account});
  reconcilor->OnAccountsInCookieUpdated(
      accounts_in_cookie_jar_info, GoogleServiceAuthError::AuthErrorNone());

  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileNoop) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kNoop, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerNoopHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 1);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileCookiesDisabled) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  std::vector<gaia::ListedAccount> accounts;
  // This will be the first call to ListAccounts.
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_test_env()->identity_manager()->GetAccountsInCookieJar();
  ASSERT_FALSE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileContentSettings) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).account_id;
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
  EXPECT_EQ(AccountReconcilor::Trigger::kCookieSettingChange,
            reconcilor->trigger_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileContentSettingsGaiaUrl) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).account_id;
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
      ConnectProfileToAccount(kFakeEmail).account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       StartReconcileContentSettingsWildcardPattern) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This test is needed until chrome changes to use gaia obfuscated id.
// The primary account manager and token service use the gaia "email" property,
// which preserves dots in usernames and preserves case.
// gaia::ParseListAccountsData() however uses gaia "displayEmail" which does not
// preserve case, and then passes the string through gaia::CanonicalizeEmail()
// which removes dots.  This tests makes sure that an email like
// "Dot.S@hmail.com", as seen by the token service, will be considered the same
// as "dots@gmail.com" as returned by gaia::ParseListAccountsData().
TEST_F(AccountReconcilorMirrorTest, StartReconcileNoopWithDots) {
  AccountInfo account_info = ConnectProfileToAccount("Dot.S@gmail.com");
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}
#endif

TEST_F(AccountReconcilorMirrorTest, StartReconcileNoopMultiple) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, account_info_2.email,
      account_info_2.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookie) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).account_id;

  std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.UpTo3mins.Success"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
                  "Signin.Reconciler.Duration.UpTo3mins.Success"),
              testing::ContainerEq(expected_counts));

  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kMultilogin, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerMultiloginHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 1);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerLogoutHistogramName, 0);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerNoopHistogramName, 0);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerThrottledHistogramName, 0);
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
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  TestGaiaCookieObserver observer;
  identity_test_env()->identity_manager()->AddObserver(&observer);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  bool expect_logout =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      true;
#else
      account_consistency == signin::AccountConsistencyMethod::kDice;
#endif
  if (expect_logout) {
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This test does not run on ChromeOS because it clears the primary account,
// which is not a flow that exists on ChromeOS.

TEST_F(AccountReconcilorMirrorTest, SignoutAfterErrorDoesNotRecordUma) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).account_id;

  std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kPersistentError);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  identity_test_env()->ClearPrimaryAccount();

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.UpTo3mins.Failure"] = 1;
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(AccountReconcilorMirrorTest, StartReconcileRemoveFromCookie) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, kFakeEmail2, kFakeGaiaId,
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

// Check that token error on primary account results in a logout to all accounts
// on Lacros. For other mirror platforms, reconcile is aborted.
TEST_F(AccountReconcilorMirrorTest, TokenErrorOnPrimary) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
#endif
  AccountReconcilor* reconcilor = GetMockReconcilor();
  signin::SetListAccountsResponseTwoAccounts(
      account_info.email, account_info.gaia, kFakeEmail2, "67890",
      &test_url_loader_factory_);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateLogOutFromCookieCompleted(reconcilor,
                                    GoogleServiceAuthError::AuthErrorNone());
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
  base::RunLoop().RunUntilIdle();
#endif
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookieTwice) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
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
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send_1,
      signin::SetAccountsInCookieResult::kSuccess);
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
  EXPECT_EQ(AccountReconcilor::Trigger::kTokenChange, reconcilor->trigger_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send_2,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileBadPrimary) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;

  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
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
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileOnlyOnce) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, Lock) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);

  TestAccountReconcilorObserver observer;
  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(reconcilor);

  // Lock prevents reconcile from starting, as long as one instance is alive.
  std::unique_ptr<AccountReconcilor::Lock> lock_1 =
      std::make_unique<AccountReconcilor::Lock>(reconcilor);
  EXPECT_EQ(1, reconcilor->account_reconcilor_lock_count_);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
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
  EXPECT_EQ(AccountReconcilor::Trigger::kUnblockReconcile,
            reconcilor->trigger_);

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

#if BUILDFLAG(ENABLE_MIRROR)
TEST_F(AccountReconcilorTest, ForceReconcileEarlyExitsForInactiveReconcilor) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_EQ(AccountReconcilorState::kInactive, reconcilor->GetState());

  reconcilor->ForceReconcile();
  EXPECT_EQ(AccountReconcilorState::kInactive, reconcilor->GetState());
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       ForceReconcileImmediatelyStartsForIdleReconcilor) {
  // Get the reconcilor to an OK (AccountReconcilorState::kOk) state.
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  reconcilor->SetState(AccountReconcilorState::kOk);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Now try to force a reconcile.
  reconcilor->ForceReconcile();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       ForceReconcileImmediatelyStartsForErroredOutReconcilor) {
  // Get the reconcilor to an error state.
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  reconcilor->SetState(AccountReconcilorState::kError);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Now try to force a reconcile.
  reconcilor->ForceReconcile();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       ForceReconcileSchedulesReconciliationIfReconcilorIsAlreadyRunning) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  identity_test_env()->WaitForRefreshTokensLoaded();
  const CoreAccountId account_id = account_info.account_id;

  // Do NOT set a ListAccounts response. We do not want reconciliation to finish
  // immediately.
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  // Schedule a regular reconciliation cycle. This will eventually end up in a
  // noop because the accounts in cookie match the Primary Account in Chrome.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kInitialized);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  // Immediately force a reconciliation. This should cause a forced
  // reconciliation to be tried later in
  // `kForcedReconciliationWaitTimeInSeconds` seconds.
  reconcilor->ForceReconcile();

  // Now set the account in cookie as the Primary Account in Chrome. This will
  // unblock the regular (`AccountReconcilor::Trigger::kInitialized`)
  // reconciliation cycle.
  signin::SetListAccountsResponseOneAccount(
      /*email=*/account_info.email, /*gaia_id=*/account_info.gaia,
      /*test_url_loader_factory=*/&test_url_loader_factory_);
  // This forced reconciliation attempt should also be blocked since
  // test_url_loader_factory_ will itself post a task to wake up pending
  // requests.
  task_environment()->FastForwardBy(
      base::Seconds(kForcedReconciliationWaitTimeInSeconds));
  base::RunLoop().RunUntilIdle();

  // Give the queued forced reconciliation cycle a chance to actually run.
  task_environment()->FastForwardBy(
      base::Seconds(kForcedReconciliationWaitTimeInSeconds));
  base::RunLoop().RunUntilIdle();

  // Indirectly test through histograms that the forced reconciliation cycle was
  // actually run.
  histogram_tester()->ExpectBucketCount(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kMultilogin, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerMultiloginHistogramName,
      AccountReconcilor::Trigger::kForcedReconcile, 1);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerMultiloginHistogramName, 1);
}

#if BUILDFLAG(IS_CHROMEOS)
// This feature is only available on ChromeOS for now. Extend this to other
// Mirror platforms after implementing `AccountManagerFacade` for them.
TEST_F(AccountReconcilorMirrorTest,
       OnSigninDialogClosedNotificationTriggersForcedReconciliation) {
  // Get the reconcilor to an OK (AccountReconcilorState::kOk) state.
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  reconcilor->SetState(AccountReconcilorState::kOk);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Now try to force a reconcile.
  reconcilor->OnSigninDialogClosed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_MIRROR)

// Checks that an "invalid" Gaia account can be refreshed in place, without
// performing a full logout.
TEST_P(AccountReconcilorMethodParamTest,
       StartReconcileWithSessionInfoExpiredDefault) {
  signin::AccountConsistencyMethod account_consistency = GetParam();
  SetAccountConsistency(account_consistency);
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id2 = account_info2.account_id;
  signin::SetListAccountsResponseWithParams(
      {{account_info.email, account_info.gaia, false /* valid */,
        false /* signed_out */, true /* verified */},
       {account_info2.email, account_info2.gaia, true /* valid */,
        false /* signed_out */, true /* verified */}},
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  const std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  switch (account_consistency) {
    case signin::AccountConsistencyMethod::kMirror: {
      signin::MultiloginParameters params(
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
      break;
    }
    case signin::AccountConsistencyMethod::kDice: {
      signin::MultiloginParameters params(
          gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
      break;
    }
    case signin::AccountConsistencyMethod::kDisabled:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       AddAccountToCookieCompletedWithBogusAccount) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
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
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, NoLoopWithBadPrimary) {
  // Connect profile to a primary account and then add a secondary account.
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id1 = account_info.account_id;
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
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

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // The primary cannot be added to cookie, so it fails.
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kPersistentError);
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
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
}

// Also see the related test -
// `WontMergeAccountsWithErrorDiscoveredByAccountReconcilorItself`.
TEST_F(AccountReconcilorMirrorTest, WontMergeAccountsWithError) {
  // Connect profile to a primary account and then add a secondary account.
  const CoreAccountId account_id1 =
      ConnectProfileToAccount(kFakeEmail).account_id;
  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).account_id;

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

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
}

// `AccountReconcilor` is supposed to filter out accounts with known errors at
// the beginning of the reconciliation cycle. This behaviour should be honored
// even if `AccountReconcilor` is the very entity that discovers this error. In
// this case, it should trigger another reconciliation cycle from scratch - and
// this time, the account with an error state will actually be skipped.
// Also see the related test - `WontMergeAccountsWithError`.
TEST_F(AccountReconcilorMirrorTest,
       WontMergeAccountsWithErrorDiscoveredByAccountReconcilorItself) {
  InSequence seq;
  // Connect profile to a primary account and then add a secondary account.
  const CoreAccountId account_id1 =
      ConnectProfileToAccount(kFakeEmail).account_id;
  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).account_id;

  // The cookie starts empty.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
  const signin::MultiloginParameters params_with_primary_account(
      /*mode=*/gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      /*accounts_to_send=*/{account_id1});
  const signin::MultiloginParameters params_with_both_accounts(
      /*mode=*/gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      /*accounts_to_send=*/{account_id1, account_id2});
  // Expect 2 calls in sequence. The first call should try to set both accounts.
  // The second call should be when the reconcilor discovers the error in the
  // second account and then retries reconciliation with just the first account.
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params_with_both_accounts));
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params_with_primary_account));

  AccountReconcilor* const reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  // Set up observer.
  TestAccountReconcilorObserver observer;
  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      scoped_observation(&observer);
  scoped_observation.Observe(reconcilor);

  // Everything set. Actually start the test.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  // Reconciliation has started and waiting for cookies to be set. At this
  // point, we find that the second account was actually in an error state. Mark
  // the secondary account in auth error state.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  SimulateSetAccountsInCookieCompleted(
      reconcilor, params_with_both_accounts.accounts_to_send,
      signin::SetAccountsInCookieResult::kPersistentError);

  // At this point, reconciliation should restart and ultimately end in an OK
  // state.
  base::RunLoop().RunUntilIdle();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, params_with_primary_account.accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
  ASSERT_EQ(0, observer.error_count_);
}

// Test that delegate timeout is called when the delegate offers a valid
// timeout.
TEST_F(AccountReconcilorTest, DelegateTimeoutIsCalled) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  auto spy_delegate0 = std::make_unique<SpyReconcilorDelegate>();
  SpyReconcilorDelegate* spy_delegate = spy_delegate0.get();
  AccountReconcilor* reconcilor =
      CreateMockReconcilor(std::move(spy_delegate0));
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
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
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_FALSE(timer->IsRunning());
}

TEST_F(AccountReconcilorTest, DelegateTimeoutIsNotCalledIfTimeoutIsNotReached) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::SetListAccountsResponseOneAccount(
      account_info.email, account_info.gaia, &test_url_loader_factory_);
  auto spy_delegate0 = std::make_unique<SpyReconcilorDelegate>();
  SpyReconcilorDelegate* spy_delegate = spy_delegate0.get();
  AccountReconcilor* reconcilor =
      CreateMockReconcilor(std::move(spy_delegate0));
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  ASSERT_TRUE(timer->IsRunning());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(0, spy_delegate->num_reconcile_timeout_calls_);
  EXPECT_EQ(1, spy_delegate->num_reconcile_finished_calls_);
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, ForcedReconcileTriggerShouldNotCallListAccounts) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kDice;
  SetAccountConsistency(account_consistency);
  gaia::MultiloginMode multilogin_mode =
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
#else
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kMirror;
  SetAccountConsistency(account_consistency);
  gaia::MultiloginMode multilogin_mode =
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
#endif
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  identity_test_env()->WaitForRefreshTokensLoaded();
  const CoreAccountId account_id = account_info.account_id;

  // Do not set a ListAccounts response, but still expect multilogin to be
  // called.
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(multilogin_mode, accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kForcedReconcile);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
}

// Forced account reconciliation
// (`AccountReconcilor::Trigger::kForcedReconcile`) should not result in a noop
// - even if ListAccounts claims to have the same set of accounts as Chrome.
TEST_F(AccountReconcilorTest, ForcedReconcileTriggerShouldNotResultInNoop) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kDice;
  SetAccountConsistency(account_consistency);
  gaia::MultiloginMode multilogin_mode =
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
#else
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kMirror;
  SetAccountConsistency(account_consistency);
  gaia::MultiloginMode multilogin_mode =
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER;
#endif
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  identity_test_env()->WaitForRefreshTokensLoaded();
  const CoreAccountId account_id = account_info.account_id;

  // Set a ListAccounts response to match the Primary Account in Chrome.
  signin::SetListAccountsResponseOneAccount(
      /*email=*/account_info.email, /*gaia_id=*/account_info.gaia,
      /*test_url_loader_factory=*/&test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(multilogin_mode, accounts_to_send);
  // `PerformSetCookiesAction()` should be called, despite the cookie jar having
  // the same account(s) as Chrome.
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kForcedReconcile);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  // Check the reported histograms. Noop bucket should not have a sample.
  // Multilogin bucket should have a sample.
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kMultilogin, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerMultiloginHistogramName,
      AccountReconcilor::Trigger::kForcedReconcile, 1);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerNoopHistogramName, 0);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerMultiloginHistogramName, 1);
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
  // Reconcile can't start until accounts are loaded.
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Delegate implementation always returning UPDATE mode with no accounts.
  class MultiloginLogoutDelegate : public signin::AccountReconcilorDelegate {
    bool IsReconcileEnabled() const override { return true; }
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
      CreateMockReconcilor(std::make_unique<MultiloginLogoutDelegate>());
  signin::SetListAccountsResponseOneAccount(kFakeEmail, "123456",
                                            &test_url_loader_factory_);

  // Logout call to Gaia.
  EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction());
  // No multilogin call.
  EXPECT_CALL(*reconcilor, PerformSetCookiesAction(testing::_)).Times(0);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  SimulateLogOutFromCookieCompleted(reconcilor,
                                    GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kLogout, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerLogoutHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 1);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerThrottledHistogramName, 0);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerMultiloginHistogramName, 0);
}

// Reconcilor does not start after being shutdown. Regression test for
// https://crbug.com/923094
TEST_F(AccountReconcilorTest, ReconcileAfterShutdown) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  EXPECT_FALSE(reconcilor->WasShutDown());
  reconcilor->Shutdown();
  EXPECT_TRUE(reconcilor->WasShutDown());
  // This should not crash.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

// Reconcilor does not unlock after being shutdown. Regression test for
// https://crbug.com/923094
TEST_F(AccountReconcilorTest, UnlockAfterShutdown) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  std::unique_ptr<AccountReconcilor::Lock> lock =
      std::make_unique<AccountReconcilor::Lock>(reconcilor);

  // Reconcile does not start now because of the Lock, but is scheduled to start
  // when the lock is released.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  EXPECT_FALSE(reconcilor->is_reconcile_started_);

  reconcilor->Shutdown();
  lock.reset();  // This should not crash.
  EXPECT_FALSE(reconcilor->is_reconcile_started_);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(AccountReconcilorTest, OnAccountsInCookieUpdatedLogoutInProgress) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kDice;
  SetAccountConsistency(account_consistency);
#else
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kMirror;
  SetAccountConsistency(account_consistency);
#endif
  signin::CookieParams cookie_params = {
      kFakeEmail, signin::GetTestGaiaIdForEmail(kFakeEmail), true /* valid */,
      false /* signed_out */, true /* verified */};

  signin::SetListAccountsResponseOneAccountWithParams(
      cookie_params, &test_url_loader_factory_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());

  // Add extra cookie change notification. Reconcilor should ignore it.
  reconcilor->OnAccountsInCookieUpdated(
      identity_test_env()->identity_manager()->GetAccountsInCookieJar(),
      GoogleServiceAuthError::AuthErrorNone());

  base::RunLoop().RunUntilIdle();

  SimulateLogOutFromCookieCompleted(reconcilor,
                                    GoogleServiceAuthError::AuthErrorNone());

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

class AccountReconcilorThrottlerTest : public AccountReconcilorTest {
 public:
  AccountReconcilorThrottlerTest() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // TODO(https://crbug.com.1464264): Migrate away from `ConsentLevel::kSync`
    // on desktop platforms.
    consent_level_for_reconcile_ = signin::ConsentLevel::kSync;
    signin::AccountConsistencyMethod account_consistency =
        signin::AccountConsistencyMethod::kDice;
    SetAccountConsistency(account_consistency);
#else
    signin::AccountConsistencyMethod account_consistency =
        signin::AccountConsistencyMethod::kMirror;
    SetAccountConsistency(account_consistency);
#endif
    minutes_to_refill_per_request_ =
        1 / AccountReconcilorThrottler::kRefillRequestsBucketRatePerMinute;
  }

  AccountReconcilorThrottlerTest(const AccountReconcilorThrottlerTest&) =
      delete;
  AccountReconcilorThrottlerTest& operator=(
      const AccountReconcilorThrottlerTest&) = delete;

  void ConsumeRequests(size_t number_of_requests,
                       const signin::MultiloginParameters& expected_params) {
    AccountReconcilor* reconcilor = GetMockReconcilor();
    for (size_t i = 0; i < number_of_requests; ++i) {
      EXPECT_CALL(*GetMockReconcilor(),
                  PerformSetCookiesAction(expected_params));
      ASSERT_FALSE(reconcilor->is_reconcile_started_);
      reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
      base::RunLoop().RunUntilIdle();
      // Reconciliation not blocked.
      ASSERT_TRUE(reconcilor->is_reconcile_started_);

      SimulateSetAccountsInCookieCompleted(
          reconcilor, expected_params.accounts_to_send,
          signin::SetAccountsInCookieResult::kSuccess);
      ASSERT_FALSE(reconcilor->is_reconcile_started_);
      ASSERT_EQ(GoogleServiceAuthError::State::NONE,
                reconcilor->error_during_last_reconcile_.state());
      testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
    }
  }

  void VerifyRequestsBlockedByThrottler() {
    AccountReconcilor* reconcilor = GetMockReconcilor();
    reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
    base::RunLoop().RunUntilIdle();
    // Reconciliation should fail.
    ASSERT_FALSE(reconcilor->is_reconcile_started_);
    ASSERT_EQ(GoogleServiceAuthError::State::REQUEST_CANCELED,
              reconcilor->error_during_last_reconcile_.state());
  }

  void FastForwadTimeToRefillRequests(size_t number_of_requests) {
    task_environment()->FastForwardBy(
        base::Minutes(minutes_to_refill_per_request_ * number_of_requests));
  }

 private:
  size_t minutes_to_refill_per_request_;
};

TEST_F(AccountReconcilorThrottlerTest, RefillOneRequest) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccount(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2),
      &test_url_loader_factory_);

  signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id});

  // Consume all available requests.
  ConsumeRequests(AccountReconcilorThrottler::kMaxAllowedRequestsPerBucket,
                  params);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerMultiloginHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 30);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerThrottledHistogramName, 0);

  // At this point all the requests in the available request buckets should
  // have been consumed.
  VerifyRequestsBlockedByThrottler();

  // Allow enough time to refill 1 request.
  FastForwadTimeToRefillRequests(1);
  ConsumeRequests(1, params);

  // The blocked request recorded upon allowing a new request.
  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1, 1);
  histogram_tester()->ExpectBucketCount(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kThrottled, 1);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerThrottledHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 1);
  histogram_tester()->ExpectBucketCount(
      AccountReconcilor::kOperationHistogramName,
      AccountReconcilor::Operation::kMultilogin, 31);
  histogram_tester()->ExpectUniqueSample(
      AccountReconcilor::kTriggerMultiloginHistogramName,
      AccountReconcilor::Trigger::kCookieChange, 31);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerLogoutHistogramName, 0);
  histogram_tester()->ExpectTotalCount(
      AccountReconcilor::kTriggerNoopHistogramName, 0);

  // No Available requests.
  VerifyRequestsBlockedByThrottler();

  DeleteReconcilor();
  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1, 2);
}

TEST_F(AccountReconcilorThrottlerTest, RefillFiveRequests) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccount(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2),
      &test_url_loader_factory_);

  signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id});

  // Consume all available requests.
  ConsumeRequests(AccountReconcilorThrottler::kMaxAllowedRequestsPerBucket,
                  params);

  // At this point all the requests in the available request buckets should
  // have been consumed.
  VerifyRequestsBlockedByThrottler();

  // Allow enough time to refill 1 request.
  FastForwadTimeToRefillRequests(5);
  ConsumeRequests(5, params);

  // The blocked request recorded upon allowing a new request.
  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1, 1);

  // No Available requests.
  VerifyRequestsBlockedByThrottler();

  DeleteReconcilor();
  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1, 2);
}

TEST_F(AccountReconcilorThrottlerTest, NewRequestParamsPasses) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccount(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2),
      &test_url_loader_factory_);

  signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id});

  // Consume all available requests.
  ConsumeRequests(AccountReconcilorThrottler::kMaxAllowedRequestsPerBucket,
                  params);

  // Next request should fail.
  VerifyRequestsBlockedByThrottler();

  // Trigger different params.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_));
  identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateSetAccountsInCookieCompleted(
      reconcilor, params.accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1, 1);
}

TEST_F(AccountReconcilorThrottlerTest, BlockFiveRequests) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.account_id;
  signin::SetListAccountsResponseOneAccount(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2),
      &test_url_loader_factory_);

  signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id});

  // Consume all available requests.
  ConsumeRequests(AccountReconcilorThrottler::kMaxAllowedRequestsPerBucket,
                  params);

  // At this point all the requests in the available request buckets should
  // have been consumed.
  size_t rejected_requests = 5;
  for (size_t i = 0; i < rejected_requests; ++i) {
    VerifyRequestsBlockedByThrottler();
  }

  // Allow enough time to refill 1 request.
  FastForwadTimeToRefillRequests(1);
  ConsumeRequests(1, params);

  // The blocked request recorded upon allowing a new request.
  histogram_tester()->ExpectBucketCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update",
      rejected_requests, 1);

  // Allow a new request with no blocked requests in between.
  FastForwadTimeToRefillRequests(1);
  ConsumeRequests(1, params);
  // The number of samples should remain 1.
  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.RejectedRequestsDueToThrottler.Update", 1);
}
