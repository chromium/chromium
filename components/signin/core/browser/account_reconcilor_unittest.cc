// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_reconcilor.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/core/browser/test_account_reconcilor_observer.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/signin/public/identity_manager/token_binding_info.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/features.h"
#include "services/network/test/mock_device_bound_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

using ::base::test::RunOnceClosure;
using ::signin_metrics::AccountReconcilorState;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArgByMove;

namespace {

#if BUILDFLAG(ENABLE_MIRROR)
// This should match the variable in the .cc file.
constexpr int kForcedReconciliationWaitTimeInSeconds = 15;
#endif  // BUILDFLAG(ENABLE_MIRROR)

constexpr char kFakeEmail[] = "user@gmail.com";
constexpr char kFakeEmail2[] = "other@gmail.com";
constexpr GaiaId::Literal kFakeGaiaId("12345");

// An AccountReconcilorDelegate that records all calls (Spy pattern).
class SpyReconcilorDelegate : public signin::AccountReconcilorDelegate {
 public:
  int num_reconcile_finished_calls_{0};
  int num_reconcile_timeout_calls_{0};

  bool IsReconcileEnabled() const override { return true; }

  gaia::GaiaSource GetGaiaApiSource(bool is_cookie_upgrade) const override {
    return is_cookie_upgrade
               ? gaia::GaiaSource::kAccountReconcilorDiceCookieUpgrade
               : gaia::GaiaSource::kAccountReconcilorDice;
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
      signin::AccountConsistencyMethod account_consistency)
      : AccountReconcilor(identity_manager,
                          client,
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
      signin::AccountReconcilorDelegate* delegate)
      : AccountReconcilor(
            identity_manager,
            client,
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
        return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
            identity_manager);
      case signin::AccountConsistencyMethod::kDisabled:
        return std::make_unique<signin::AccountReconcilorDelegate>();
      case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        return std::make_unique<signin::DiceAccountReconcilorDelegate>(
            identity_manager);
#else
        NOTREACHED();
#endif
    }
    NOTREACHED();
  }
};

class MockAccountReconcilor
    : public testing::StrictMock<DummyAccountReconcilorWithDelegate> {
 public:
  MockAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      signin::AccountConsistencyMethod account_consistency);

  MockAccountReconcilor(
      signin::IdentityManager* identity_manager,
      SigninClient* client,
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
  MOCK_METHOD2(PerformSetCookiesAction,
               void(const signin::MultiloginParameters& parameters,
                    bool is_cookie_upgrade));
};

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
    signin::AccountConsistencyMethod account_consistency)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
          account_consistency) {
}

MockAccountReconcilor::MockAccountReconcilor(
    signin::IdentityManager* identity_manager,
    SigninClient* client,
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          identity_manager,
          client,
          delegate.release()) {
}

struct Cookie {
  GaiaId gaia_id;
  bool is_valid;

  bool operator==(const Cookie& other) const = default;
};

// Converts CookieParams to ListedAccounts.
gaia::ListedAccount ListedAccountFromCookieParams(
    const gaia::CookieParams& params,
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

  CoreAccountId PickAccountIdForAccount(const GaiaId& gaia_id,
                                        const std::string& username);

  void SimulateSetAccountsInCookieCompleted(
      AccountReconcilor* reconcilor,
      const std::vector<CoreAccountId>& accounts_to_send,
      signin::SetAccountsInCookieResult result,
      std::optional<base::TimeTicks> cookie_upgrade_start_time = std::nullopt);

  void SimulateLogOutFromCookieCompleted(AccountReconcilor* reconcilor,
                                         const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  void SetAccountConsistency(signin::AccountConsistencyMethod method);

  PrefService* pref_service() { return &pref_service_; }

  void DeleteReconcilor() {
    if (mock_reconcilor_) {
      mock_reconcilor_->Shutdown();
    }
    mock_reconcilor_.reset();
  }

  void EnsureAccountsInCookieJarAreFresh() {
    signin::IdentityManager* identity_manager =
        identity_test_env()->identity_manager();
    identity_manager->GetAccountsCookieMutator()->TriggerCookieJarUpdate();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(identity_manager->GetAccountsInCookieJar().AreAccountsFresh());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

  signin::ConsentLevel consent_level_for_reconcile_ =
#if BUILDFLAG(IS_CHROMEOS)
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
  signin::SetListAccountsResponseHttpNotFound(&test_url_loader_factory_);

  // The reconcilor should not be built before the test can set the account
  // consistency method.
  EXPECT_FALSE(mock_reconcilor_);
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
        identity_test_env_.identity_manager(), &test_signin_client_,
        account_consistency_);
  }

  return mock_reconcilor_.get();
}

MockAccountReconcilor* AccountReconcilorTest::CreateMockReconcilor(
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate) {
  DCHECK(!mock_reconcilor_);
  mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
      identity_test_env_.identity_manager(), &test_signin_client_,
      std::move(delegate));
  return mock_reconcilor_.get();
}

AccountReconcilorTest::~AccountReconcilorTest() {
  if (mock_reconcilor_) {
    mock_reconcilor_->Shutdown();
  }
  test_signin_client_.Shutdown();
}

AccountInfo AccountReconcilorTest::ConnectProfileToAccount(
    const std::string& email) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      email, consent_level_for_reconcile_);
  return account_info;
}

CoreAccountId AccountReconcilorTest::PickAccountIdForAccount(
    const GaiaId& gaia_id,
    const std::string& username) {
  return identity_test_env()->identity_manager()->PickAccountIdForAccount(
      gaia_id, username);
}

void AccountReconcilorTest::SimulateSetAccountsInCookieCompleted(
    AccountReconcilor* reconcilor,
    const std::vector<CoreAccountId>& accounts_to_send,
    signin::SetAccountsInCookieResult result,
    std::optional<base::TimeTicks> cookie_upgrade_start_time) {
  reconcilor->OnSetAccountsInCookieCompleted(accounts_to_send,
                                             cookie_upgrade_start_time, result);
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
  const std::string tokens;
  const std::string cookies;
  IsFirstReconcile is_first_reconcile;
  const std::string gaia_api_calls;
  const std::string tokens_after_reconcile;
  const std::string cookies_after_reconcile;
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
      << base::ToString(param.is_first_reconcile == IsFirstReconcile::kFirst);
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
    GaiaId gaia_id;
  };

  struct Token {
    GaiaId gaia_id;
    std::string email;
    bool is_authenticated;
    bool has_error;
  };

  virtual void CreateReconclior() { GetMockReconcilor(); }

  // Build Tokens from string.
  std::vector<Token> ParseTokenString(std::string_view token_string) {
    std::vector<Token> parsed_tokens;
    bool is_authenticated = false;
    bool has_error = false;
    for (char token_code : token_string) {
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
  std::vector<Cookie> ParseCookieString(std::string_view cookie_string) {
    std::vector<Cookie> parsed_cookies;
    bool valid = true;
    for (char cookie_code : cookie_string) {
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
    if (!authenticated_account_found) {
      EXPECT_EQ(CoreAccountId(), primary_account_id);
    }
  }

  void SetupTokens(std::string_view tokens_string) {
    std::vector<Token> tokens = ParseTokenString(tokens_string);
    Token primary_account;
    for (const Token& token : tokens) {
      CoreAccountId account_id;
      if (token.is_authenticated) {
        account_id = ConnectProfileToAccount(token.email).GetAccountId();
      } else {
        account_id = identity_test_env()
                         ->MakeAccountAvailable(token.email)
                         .GetAccountId();
      }
      if (token.has_error) {
        signin::UpdatePersistentErrorOfRefreshTokenForAccount(
            identity_test_env()->identity_manager(), account_id,
            GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
      }
    }
    VerifyCurrentTokens(tokens);
  }

  void ConfigureCookieManagerService(const std::vector<Cookie>& cookies) {
    std::vector<gaia::CookieParams> cookie_params;
    for (const auto& cookie : cookies) {
      GaiaId gaia_id = cookie.gaia_id;

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
      if (PickAccountIdForAccount(account.gaia_id, account.email) ==
          account_id) {
        return account;
      }
    }
    NOTREACHED();
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
      std::vector<GaiaId> gaia_ids;
      for (const auto& account_id : parameters.accounts_to_send) {
        gaia_ids.push_back(GetAccount(account_id).gaia_id);
      }
      cookies_after_reconcile = cookies_before_reconcile;
      for (Cookie& cookie : cookies_after_reconcile) {
        if (std::ranges::contains(gaia_ids, cookie.gaia_id)) {
          cookie.is_valid = true;
          gaia_ids.erase(std::ranges::find(gaia_ids, cookie.gaia_id));
        } else {
          DCHECK(!cookie.is_valid);
        }
      }
      for (const GaiaId& gaia_id : gaia_ids) {
        cookies_after_reconcile.emplace_back(gaia_id, true);
      }
    }
    return cookies_after_reconcile;
  }

  // Runs the test corresponding to one row of the table.
  void RunRowTest(const AccountReconcilorTestTableParam& param) {
    // Setup cookies.
    std::vector<Cookie> cookies = ParseCookieString(param.cookies);
    ConfigureCookieManagerService(cookies);
    std::vector<Cookie> cookies_after_reconcile = cookies;

    // Ensure that accounts in cookie jar are fresh so the next call to
    // GetAccountsInCookieJar() completes synchronously.
    EnsureAccountsInCookieJarAreFresh();

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
        EXPECT_CALL(
            *GetMockReconcilor(),
            PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false))
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
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
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
      if (row.is_first_reconcile == IsFirstReconcile::kFirst) {
        continue;
      }

      if (!(row.tokens == param.tokens_after_reconcile &&
            row.cookies == param.cookies_after_reconcile)) {
        continue;
      }
      EXPECT_EQ(row.tokens, row.tokens_after_reconcile);
      EXPECT_EQ(row.cookies, row.cookies_after_reconcile);
      return;
    }

    ADD_FAILURE() << "Could not check that reconcile is idempotent.";
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
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
  account_mutator->SetPrimaryAccount(account_info.GetAccountId(),
                                     consent_level_for_reconcile_,
                                     signin_metrics::AccessPoint::kStartPage);

  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AccountReconcilorMirrorTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount(kFakeEmail);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class AccountReconcilorTestUnoMultilogin : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestUnoMultilogin() = default;
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

// Checks one row of the kUnoParams table above.
TEST_P(AccountReconcilorTestUnoMultilogin, TableRowTest) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  CheckReconcileIdempotent(kUnoParams, GetParam());
  RunRowTest(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AccountReconcilorTestUnoMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kUnoParams)));

class AccountReconcilorDiceTest : public AccountReconcilorTest {
 public:
  AccountReconcilorDiceTest() {
    consent_level_for_reconcile_ = signin::ConsentLevel::kSignin;
    SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  }

  AccountReconcilorDiceTest(const AccountReconcilorDiceTest&) = delete;
  AccountReconcilorDiceTest& operator=(const AccountReconcilorDiceTest&) =
      delete;
};

TEST_F(AccountReconcilorDiceTest, ClearPrimaryAccountNotAllowed) {
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

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
  run_loop.Run();
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
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

  identity_test_env()->ClearPrimaryAccount();
  ASSERT_TRUE(reconcilor->IsRegisteredWithIdentityManager());
}

TEST_F(AccountReconcilorDiceTest, DiceReconcileWithoutSignin) {
  // Add a token in Chrome but do not sign in. Making account available (setting
  // a refresh token) triggers listing cookies so we need to setup cookies
  // before that.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // The reconcilor does not rebuild cookies while signed out.
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  TestAccountReconcilorObserver observer(
      reconcilor, /*wait_state=*/AccountReconcilorState::kOk);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  observer.WaitForStateChange();

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Checks that nothing happens when there is no Chrome account and no Gaia
// cookie.
TEST_F(AccountReconcilorDiceTest, DiceReconcileNoop) {
  // No Chrome account and no cookie.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  TestAccountReconcilorObserver observer(
      reconcilor, /*wait_state=*/AccountReconcilorState::kOk);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  observer.WaitForStateChange();

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Tests that the first Gaia account is re-used when possible.
TEST_F(AccountReconcilorDiceTest, DiceReconcileReuseGaiaFirstAccount) {
  // Add an invalid primary account so that the reconcilor is in a mode where
  // it rebuilds cookies.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Add account "other" to the Gaia cookie.
  signin::SetListAccountsResponseTwoAccounts(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2), "foo@gmail.com",
      GaiaId("9999"), &test_url_loader_factory_);

  // Add accounts "user" and "other" to the token service.
  const AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail);
  const CoreAccountId account_id_1 = account_info_1.GetAccountId();
  const AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id_2 = account_info_2.GetAccountId();

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
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  run_loop.Run();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

// Tests that the first account is kept in cache and reused when cookies are
// lost.
TEST_F(AccountReconcilorDiceTest, DiceLastKnownFirstAccount) {
  // Add an invalid primary account so that the reconcilor is in a mode where
  // it rebuilds cookies.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Add accounts to the token service and the Gaia cookie in a different order.
  // Making account available (setting a refresh token) triggers listing cookies
  // so we need to setup cookies before that.
  signin::SetListAccountsResponseTwoAccounts(
      kFakeEmail2, signin::GetTestGaiaIdForEmail(kFakeEmail2), kFakeEmail,
      signin::GetTestGaiaIdForEmail(kFakeEmail), &test_url_loader_factory_);

  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail);
  const CoreAccountId account_id_1 = account_info_1.GetAccountId();
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id_2 = account_info_2.GetAccountId();

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
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

    AccountReconcilor* reconcilor = GetMockReconcilor();
    TestAccountReconcilorObserver observer(
        reconcilor, /*wait_state=*/AccountReconcilorState::kOk);
    reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
    observer.WaitForStateChange();
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
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  run_loop.Run();
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
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction).Times(0);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  TestAccountReconcilorObserver observer(
      reconcilor, /*wait_state=*/AccountReconcilorState::kOk);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  observer.WaitForStateChange();
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
          .GetAccountId();

  // In PRESERVE mode it is up to Gaia to not delete existing accounts in
  // cookies and not sign out unveridied accounts.
  std::vector<CoreAccountId> accounts_to_send = {chrome_account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  run_loop.Run();
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(AccountReconcilorDiceTest, DeleteCookieForNonSyncingSupervisedUsers) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_parental_controls(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));
}

TEST_F(AccountReconcilorDiceTest, DeleteCookieForSyncingSupervisedUsers) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, consent_level_for_reconcile_);

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_parental_controls(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);

  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));

  AccountReconcilor* reconcilor = GetMockReconcilor();

  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

TEST_F(AccountReconcilorDiceTest, DeleteCookie) {
  const CoreAccountId primary_account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable(kFakeEmail,
                                        consent_level_for_reconcile_)
          .GetAccountId();
  const CoreAccountId secondary_account_id =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).GetAccountId();

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

  identity_test_env()->SetRefreshTokenForAccount(secondary_account_id);
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(primary_account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(secondary_account_id));
}

TEST_F(AccountReconcilorDiceTest, DeleteCookieForSignedInUser) {
  auto* identity_manager = identity_test_env()->identity_manager();
  signin::SetListAccountsResponseOneAccount(kFakeEmail, kFakeGaiaId,
                                            &test_url_loader_factory_);
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);

  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  ASSERT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnAccountsCookieDeletedByUserAction();

  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info.GetAccountId()));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.GetAccountId()));
}

TEST_F(AccountReconcilorDiceTest, PendingStateThenClearPrimaryAccount) {
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
          primary_account_info.GetAccountId()));
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

TEST_F(AccountReconcilorDiceTest, SetAccountsInCookiePersistentError) {
  // Make Chrome to try to rebuild the cookies (Chrome accounts and Gaia
  // accounts mismatch).
  signin::SetListAccountsResponseOneAccountWithParams(
      {.email = kFakeEmail,
       .gaia_id = kFakeGaiaId,
       .valid = true,
       .signed_out = false,
       .verified = true},
      &test_url_loader_factory_);

  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();

  const AccountInfo account_info_1 = signin::MakeAccountAvailable(
      identity_manager, signin::AccountAvailabilityOptionsBuilder()
                            .WithGaiaId(kFakeGaiaId)
                            .WithRefreshToken("refresh_token_1")
                            .AsPrimary(signin::ConsentLevel::kSignin)
                            .Build(kFakeEmail));
  ASSERT_TRUE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));

  const AccountInfo account_info_2 = signin::MakeAccountAvailable(
      identity_manager, signin::AccountAvailabilityOptionsBuilder()
                            .WithRefreshToken("refresh_token_2")
                            .Build(kFakeEmail2));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info_2.GetAccountId()));

  MockAccountReconcilor* reconcilor = GetMockReconcilor();

  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      /*accounts_to_send=*/{account_info_1.GetAccountId(),
                            account_info_2.GetAccountId()});
  base::RunLoop run_loop;
  EXPECT_CALL(*reconcilor,
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  run_loop.Run();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, /*accounts_to_send=*/{},
      signin::SetAccountsInCookieResult::kPersistentError);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Given the persistent error received, the reoncilor is in the error state.
  EXPECT_EQ(AccountReconcilorState::kError, reconcilor->GetState());

  // Nothing changes to the accounts state.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info_1.GetAccountId()));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info_2.GetAccountId()));
}

TEST_F(AccountReconcilorDiceTest,
       SetAccountsInCookiePersistentErrorRefreshTokensBoundToDifferentKeys) {
  // Make Chrome to try to rebuild the cookies (Chrome accounts and Gaia
  // accounts mismatch).
  signin::SetListAccountsResponseOneAccountWithParams(
      {.email = kFakeEmail,
       .gaia_id = kFakeGaiaId,
       .valid = true,
       .signed_out = false,
       .verified = true},
      &test_url_loader_factory_);

  // Setup two accounts with refresh tokens bound to different keys.
  const std::vector<uint8_t> fake_binding_key = {1, 2, 3};
  const std::vector<uint8_t> fake_binding_key_other = {4, 5, 6};

  signin::IdentityManager* identity_manager =
      identity_test_env()->identity_manager();

  const AccountInfo account_info_1 = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithGaiaId(kFakeGaiaId)
          .WithRefreshToken("refresh_token_1")
          .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
              fake_binding_key, /*mtls_token_binding=*/false))
          .AsPrimary(signin::ConsentLevel::kSignin)
          .Build(kFakeEmail));
  ASSERT_TRUE(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));

  const AccountInfo account_info_2 = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithRefreshToken("refresh_token_2")
          .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
              fake_binding_key_other, /*mtls_token_binding=*/false))
          .Build(kFakeEmail2));
  ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(
      account_info_2.GetAccountId()));

  MockAccountReconcilor* reconcilor = GetMockReconcilor();

  const signin::MultiloginParameters expected_params_1(
      gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
      /*accounts_to_send=*/{account_info_1.GetAccountId(),
                            account_info_2.GetAccountId()});
  base::RunLoop perform_set_cookies_run_loop;
  EXPECT_CALL(*reconcilor, PerformSetCookiesAction(expected_params_1,
                                                   /*is_cookie_upgrade=*/false))
      .WillOnce(RunOnceClosure(perform_set_cookies_run_loop.QuitClosure()));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  perform_set_cookies_run_loop.Run();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, /*accounts_to_send=*/{},
      signin::SetAccountsInCookieResult::kPersistentError);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Change in accounts is detected due to the invalidated refresh tokens,
  // putting the reconcilor in the scheduled state.
  EXPECT_EQ(AccountReconcilorState::kScheduled, reconcilor->GetState());

  // Refresh tokens for secondary accounts are revoked and the refresh token for
  // the primary account is invalidated.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      account_info_2.GetAccountId()));
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info_1.GetAccountId()));

  base::RunLoop perform_logout_all_accounts_run_loop;
  // In the next reconcile cycle, there is no valid Chrome account (i.e. no
  // accounts to send), the reconcilor recovers and preserves the primary
  // account in the error state.
  EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction())
      .WillOnce(
          RunOnceClosure(perform_logout_all_accounts_run_loop.QuitClosure()));

  perform_logout_all_accounts_run_loop.Run();

  SimulateLogOutFromCookieCompleted(reconcilor,
                                    GoogleServiceAuthError::AuthErrorNone());

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(AccountReconcilorState::kOk, reconcilor->GetState());
  // The primary account is not cleared.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
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
using AccountReconcilorTestDicePreChromeSignIn = AccountReconcilorTestTable;

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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
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
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id = account_info.GetAccountId();
  signin::SetListAccountsResponseOneAccountWithParams(
      {std::string(account_info.GetEmail()), account_info.GetGaiaId(),
       false /* valid */, false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id = account_info.GetAccountId();
  signin::SetListAccountsResponseOneAccountWithParams(
      {std::string(account_info.GetEmail()), account_info.GetGaiaId(),
       false /* valid */, false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id = account_info.GetAccountId();
  gaia::CookieParams cookie_params = {
      std::string(account_info.GetEmail()), account_info.GetGaiaId(),
      false /* valid */, false /* signed_out */, true /* verified */};

  signin::SetListAccountsResponseOneAccountWithParams(
      cookie_params, &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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

  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

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

TEST_F(AccountReconcilorMirrorTest, StartReconcileCookieJarFresh) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);

  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
  EnsureAccountsInCookieJarAreFresh();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  base::HistogramTester tester;
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Cookie jar is fresh when StartReconcile is called, so it should record
  // true.
  tester.ExpectUniqueSample(AccountReconcilor::kCookieJarIsFreshHistogramName,
                            true, 1);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileCookieJarStale) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);

  // By default, the accounts in the cookie jar are stale/not fresh.
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->GetAccountsInCookieJar()
                   .AreAccountsFresh());

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  base::HistogramTester tester;
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Cookie jar is stale when StartReconcile is called, so it should record
  // false.
  tester.ExpectUniqueSample(AccountReconcilor::kCookieJarIsFreshHistogramName,
                            false, 1);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileCookiesDisabled) {
  const CoreAccountId account_id =
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
  identity_test_env()->SetRefreshTokenForAccount(account_id);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

#if BUILDFLAG(IS_CHROMEOS)
// This test is needed until chrome changes to use gaia obfuscated id.
// The primary account manager and token service use the gaia "email" property,
// which preserves dots in usernames and preserves case.
// gaia::ParseBinaryListAccountsData() however uses gaia "displayEmail" which
// does not preserve case, and then passes the string through
// gaia::CanonicalizeEmail() which removes dots.  This tests makes sure that an
// email like "Dot.S@hmail.com", as seen by the token service, will be
// considered the same as "dots@gmail.com" as returned by
// gaia::ParseBinaryListAccountsData().
TEST_F(AccountReconcilorMirrorTest, StartReconcileNoopWithDots) {
  AccountInfo account_info = ConnectProfileToAccount("Dot.S@gmail.com");
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
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
      account_info.GetEmail(), account_info.GetGaiaId(),
      account_info_2.GetEmail(), account_info_2.GetGaiaId(),
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookie) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.GetAccountId();
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).GetAccountId();

  std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id = account_info.GetAccountId();
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  TestGaiaCookieObserver observer;
  identity_test_env()->identity_manager()->AddObserver(&observer);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

  bool expect_logout =
      account_consistency == signin::AccountConsistencyMethod::kDice;
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

#if !BUILDFLAG(IS_CHROMEOS)
// This test does not run on ChromeOS because it clears the primary account,
// which is not a flow that exists on ChromeOS.

TEST_F(AccountReconcilorMirrorTest, SignoutAfterErrorDoesNotRecordUma) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.GetAccountId();
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).GetAccountId();

  std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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

#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AccountReconcilorMirrorTest, StartReconcileRemoveFromCookie) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.GetAccountId();
  identity_test_env()->SetRefreshTokenForAccount(account_id);
  signin::SetListAccountsResponseTwoAccounts(
      account_info.GetEmail(), account_info.GetGaiaId(), kFakeEmail2,
      kFakeGaiaId, &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

// Check that token error on primary account results in aborted reconcile
TEST_F(AccountReconcilorMirrorTest, TokenErrorOnPrimary) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  signin::SetListAccountsResponseTwoAccounts(
      account_info.GetEmail(), account_info.GetGaiaId(), kFakeEmail2,
      GaiaId("67890"), &test_url_loader_factory_);
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest, StartReconcileAddToCookieTwice) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.GetAccountId();
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id2 = account_info2.GetAccountId();

  const std::string email3 = "third@gmail.com";
  const GaiaId gaia_id3 = signin::GetTestGaiaIdForEmail(email3);
  const CoreAccountId account_id3 = PickAccountIdForAccount(gaia_id3, email3);

  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send_1 = {account_id, account_id2};
  const signin::MultiloginParameters ml_params_1(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send_1);
  EXPECT_CALL(
      *GetMockReconcilor(),
      PerformSetCookiesAction(ml_params_1, /*is_cookie_upgrade=*/false));

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
      account_info.GetEmail(), account_info.GetGaiaId(),
      account_info2.GetEmail(), account_info2.GetGaiaId(),
      &test_url_loader_factory_);
  identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(false);

  // This will cause the reconcilor to fire.
  identity_test_env()->MakeAccountAvailable(email3);
  std::vector<CoreAccountId> accounts_to_send_2 = {account_id, account_id2,
                                                   account_id3};
  const signin::MultiloginParameters ml_params_2(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send_2);
  EXPECT_CALL(
      *GetMockReconcilor(),
      PerformSetCookiesAction(ml_params_2, /*is_cookie_upgrade=*/false));
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
  const CoreAccountId account_id = account_info.GetAccountId();

  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id2 = account_info2.GetAccountId();
  signin::SetListAccountsResponseTwoAccounts(
      account_info2.GetEmail(), account_info2.GetGaiaId(),
      account_info.GetEmail(), account_info.GetGaiaId(),
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);

  TestAccountReconcilorObserver observer(reconcilor);

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
    EXPECT_EQ(0, observer.started_count());
    EXPECT_EQ(0, observer.unblocked_count());
    EXPECT_EQ(1, observer.blocked_count());
  }

  // All locks are deleted, reconcile starts.
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(1, observer.started_count());
  EXPECT_EQ(1, observer.unblocked_count());
  EXPECT_EQ(1, observer.blocked_count());
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
  EXPECT_EQ(2, observer.started_count());
  EXPECT_EQ(2, observer.unblocked_count());
  EXPECT_EQ(2, observer.blocked_count());

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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));
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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));
  reconcilor->SetState(AccountReconcilorState::kError);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  // Now try to force a reconcile.
  reconcilor->ForceReconcile();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       CreateForceReconcileCallbackTriggersForcedReconciliation) {
  // Get the reconcilor to an OK (AccountReconcilorState::kOk) state.
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));
  reconcilor->SetState(AccountReconcilorState::kOk);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  TestAccountReconcilorObserver observer(
      reconcilor, /*wait_state=*/AccountReconcilorState::kRunning);
  reconcilor->CreateForceReconcileCallback().Run();
  observer.WaitForStateChange();

  EXPECT_EQ(AccountReconcilorState::kRunning, reconcilor->GetState());
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorMirrorTest,
       ForceReconcileSchedulesReconciliationIfReconcilorIsAlreadyRunning) {
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  identity_test_env()->WaitForRefreshTokensLoaded();
  const CoreAccountId account_id = account_info.GetAccountId();

  // Do NOT set a ListAccounts response. We do not want reconciliation to finish
  // immediately.
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
      /*email=*/account_info.GetEmail(),
      /*gaia_id=*/account_info.GetGaiaId(),
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

#endif  // BUILDFLAG(ENABLE_MIRROR)

// Checks that an "invalid" Gaia account can be refreshed in place, without
// performing a full logout.
TEST_P(AccountReconcilorMethodParamTest,
       StartReconcileWithSessionInfoExpiredDefault) {
  signin::AccountConsistencyMethod account_consistency = GetParam();
  SetAccountConsistency(account_consistency);
  AccountInfo account_info = ConnectProfileToAccount(kFakeEmail);
  const CoreAccountId account_id = account_info.GetAccountId();
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id2 = account_info2.GetAccountId();
  signin::SetListAccountsResponseWithParams(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId(),
        false /* valid */, false /* signed_out */, true /* verified */},
       {std::string(account_info2.GetEmail()), account_info2.GetGaiaId(),
        true /* valid */, false /* signed_out */, true /* verified */}},
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  const std::vector<CoreAccountId> accounts_to_send = {account_id, account_id2};
  switch (account_consistency) {
    case signin::AccountConsistencyMethod::kMirror: {
      signin::MultiloginParameters params(
          gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(),
                  PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));
      break;
    }
    case signin::AccountConsistencyMethod::kDice: {
      signin::MultiloginParameters params(
          gaia::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(),
                  PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));
      break;
    }
    case signin::AccountConsistencyMethod::kDisabled:
      NOTREACHED();
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
  const CoreAccountId account_id = account_info.GetAccountId();
  signin::SetListAccountsResponseOneAccountWithParams(
      {std::string(account_info.GetEmail()), account_info.GetGaiaId(),
       false /* valid */, false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id1 = account_info.GetAccountId();
  AccountInfo account_info2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2);
  const CoreAccountId account_id2 = account_info2.GetAccountId();

  std::vector<CoreAccountId> accounts_to_send = {account_id1, account_id2};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

  // The primary account is in auth error, so it is not in the cookie.
  signin::SetListAccountsResponseOneAccountWithParams(
      {std::string(account_info2.GetEmail()), account_info2.GetGaiaId(),
       false /* valid */, false /* signed_out */, true /* verified */},
      &test_url_loader_factory_);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN);

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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).GetAccountId();

  // Mark the secondary account in auth error state.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id2,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  // The cookie starts empty.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
  std::vector<CoreAccountId> accounts_to_send = {account_id1};
  const signin::MultiloginParameters params(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
      ConnectProfileToAccount(kFakeEmail).GetAccountId();
  const CoreAccountId account_id2 =
      identity_test_env()->MakeAccountAvailable(kFakeEmail2).GetAccountId();

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
              PerformSetCookiesAction(params_with_both_accounts,
                                      /*is_cookie_upgrade=*/false));
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params_with_primary_account,
                                      /*is_cookie_upgrade=*/false));

  AccountReconcilor* const reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  // Set up observer.
  TestAccountReconcilorObserver observer(reconcilor);
  // Everything set. Actually start the test.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  // Reconciliation has started and waiting for cookies to be set. At this
  // point, we find that the second account was actually in an error state. Mark
  // the secondary account in auth error state.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager(), account_id2,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
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
  ASSERT_EQ(0, observer.error_count());
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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
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
  signin::SetListAccountsResponseOneAccount(account_info.GetEmail(),
                                            account_info.GetGaiaId(),
                                            &test_url_loader_factory_);
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
  const CoreAccountId account_id = account_info.GetAccountId();

  // Do not set a ListAccounts response, but still expect multilogin to be
  // called.
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(multilogin_mode, accounts_to_send);
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  const CoreAccountId account_id = account_info.GetAccountId();

  // Set a ListAccounts response to match the Primary Account in Chrome.
  signin::SetListAccountsResponseOneAccount(
      /*email=*/account_info.GetEmail(),
      /*gaia_id=*/account_info.GetGaiaId(),
      /*test_url_loader_factory=*/&test_url_loader_factory_);
  std::vector<CoreAccountId> accounts_to_send = {account_id};
  const signin::MultiloginParameters params(multilogin_mode, accounts_to_send);
  // `PerformSetCookiesAction()` should be called, despite the cookie jar having
  // the same account(s) as Chrome.
  EXPECT_CALL(*GetMockReconcilor(),
              PerformSetCookiesAction(params, /*is_cookie_upgrade=*/false));

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
  signin::SetListAccountsResponseOneAccount(kFakeEmail, GaiaId("123456"),
                                            &test_url_loader_factory_);

  // Logout call to Gaia.
  EXPECT_CALL(*reconcilor, PerformLogoutAllAccountsAction());
  // No multilogin call.
  EXPECT_CALL(*reconcilor, PerformSetCookiesAction).Times(0);

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(AccountReconcilorTest, OnAccountsInCookieUpdatedLogoutInProgress) {
  signin::AccountConsistencyMethod account_consistency =
      signin::AccountConsistencyMethod::kDice;
  SetAccountConsistency(account_consistency);
  gaia::CookieParams cookie_params = {
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class AccountReconcilorThrottlerTest : public AccountReconcilorTest {
 public:
  AccountReconcilorThrottlerTest() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    consent_level_for_reconcile_ = signin::ConsentLevel::kSignin;
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
                  PerformSetCookiesAction(expected_params,
                                          /*is_cookie_upgrade=*/false));
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
  const CoreAccountId account_id = account_info.GetAccountId();
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
  const CoreAccountId account_id = account_info.GetAccountId();
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
  const CoreAccountId account_id = account_info.GetAccountId();
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
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction);
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
  const CoreAccountId account_id = account_info.GetAccountId();
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(AccountReconcilorTest, DeviceBoundSessionsFetchBlocksReconciliation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  network::MockDeviceBoundSessionManager mock_session_manager;
  test_signin_client()->set_device_bound_session_manager(&mock_session_manager);

  // Set expectation for GetAllSessions. We will capture the callback.
  network::mojom::DeviceBoundSessionManager::GetAllSessionsCallback callback;
  EXPECT_CALL(mock_session_manager, GetAllSessions)
      .WillOnce(SaveArgByMove<0>(&callback));

  // Create reconcilor.
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Try to start reconciliation manually.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Reconcilor should be blocked because sessions are not fetched yet.
  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kScheduled);

  // Set expectation on the mock reconcilor's action (e.g.
  // PerformSetCookiesAction). This will be called once we unblock.
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  // Invoke the callback to finish session prefetch.
  std::move(callback).Run({});

  // Now the reconcilor should start reconciliation and proceed.
  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kRunning);

  run_loop.Run();
}

TEST_F(
    AccountReconcilorTest,
    DeviceBoundSessionsFetchDoesNotBlockReconciliationWhenPreconditionsNotMet) {
  base::test::ScopedFeatureList feature_list;
  // Disable cookie binding upgrade feature.
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions},
                                {switches::kEnableCookieBindingCookieUpgrade});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  network::MockDeviceBoundSessionManager mock_session_manager;
  test_signin_client()->set_device_bound_session_manager(&mock_session_manager);

  EXPECT_CALL(mock_session_manager, GetAllSessions).Times(0);

  // Create reconcilor.
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Set expectation on PerformSetCookiesAction. It should be called
  // immediately despite sessions not being fetched.
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  // Start reconciliation.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Reconcilor should not be blocked and should proceed to running.
  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kRunning);

  run_loop.Run();
}

TEST_F(AccountReconcilorTest,
       CookieUpgradeTriggersMultiloginEvenIfCookiesMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Set standard sessions fetched to empty.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->OnDeviceBoundSessionsFetched(std::nullopt, {});

  // Set cookie jar containing the same account.
  signin::SetListAccountsResponseOneAccount(
      /*email=*/account_info.GetEmail(),
      /*gaia_id=*/account_info.GetGaiaId(),
      /*test_url_loader_factory=*/&test_url_loader_factory_);

  // PerformSetCookiesAction should be called because we need to upgrade the
  // cookie.
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();
}

class MockBoundSessionOAuthMultiLoginDelegate
    : public signin::BoundSessionOAuthMultiLoginDelegate {
 public:
  MockBoundSessionOAuthMultiLoginDelegate() = default;
  ~MockBoundSessionOAuthMultiLoginDelegate() override = default;

  MOCK_METHOD(void,
              BeforeSetCookies,
              (const OAuthMultiloginResult&),
              (override));
  MOCK_METHOD(void, OnCookiesSet, (), (override));
  MOCK_METHOD((std::vector<std::pair<GURL, std::string>>),
              GetAllSessions,
              (),
              (const, override));
};

TEST_F(AccountReconcilorTest, NeedsCookieBindingUpgradeTriggersUpgrade) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Create reconcilor.
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Feed empty sessions (simulating prefetch returned no sessions).
  reconcilor->OnDeviceBoundSessionsFetched(std::nullopt, {});

  // Verify that NeedsCookieBindingUpgrade returns kNeedsUpgrade because no
  // sidts_session exists.
  EXPECT_EQ(reconcilor->NeedsCookieBindingUpgrade(),
            AccountReconcilor::CookieBindingUpgradeStatus::kNeedsUpgrade);
}

TEST_F(AccountReconcilorTest,
       NeedsCookieBindingUpgradeNoUpgradeIfStandardSessionExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Create reconcilor.
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Feed standard session matching secure google.com and sidts_session.
  std::vector<net::device_bound_sessions::SessionKey> sessions;
  sessions.emplace_back(
      net::SchemefulSite(GaiaUrls::GetInstance()->secure_google_url()),
      net::device_bound_sessions::SessionKey::Id("sidts_session"));
  reconcilor->OnDeviceBoundSessionsFetched(std::nullopt, sessions);

  // Verify that NeedsCookieBindingUpgrade returns kHasStandardSession since
  // standard session exists.
  EXPECT_EQ(reconcilor->NeedsCookieBindingUpgrade(),
            AccountReconcilor::CookieBindingUpgradeStatus::kHasStandardSession);
}

TEST_F(AccountReconcilorTest,
       NeedsCookieBindingUpgradeNoUpgradeIfPrototypeSessionExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Make primary account available with a bound key first.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  // Setup prototype delegate to return a prototype session.
  test_signin_client()->set_bound_session_oauth_multilogin_delegate_factory(
      base::BindRepeating(
          []() -> std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate> {
            auto mock_delegate = std::make_unique<
                testing::NiceMock<MockBoundSessionOAuthMultiLoginDelegate>>();
            std::vector<std::pair<GURL, std::string>> sessions;
            sessions.emplace_back(GaiaUrls::GetInstance()->secure_google_url(),
                                  "sidts_session");
            ON_CALL(*mock_delegate, GetAllSessions)
                .WillByDefault(Return(sessions));
            return mock_delegate;
          }));

  // Create reconcilor.
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Feed empty standard sessions.
  reconcilor->OnDeviceBoundSessionsFetched(std::nullopt, {});

  // Verify that NeedsCookieBindingUpgrade returns kHasPrototypeSession since
  // prototype session exists.
  EXPECT_EQ(
      reconcilor->NeedsCookieBindingUpgrade(),
      AccountReconcilor::CookieBindingUpgradeStatus::kHasPrototypeSession);
}

TEST_F(AccountReconcilorTest,
       CookieBindingUpgradeStatusMetricsFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kEnableCookieBindingCookieUpgrade);
  base::HistogramTester tester;

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  // Make primary account available.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  identity_test_env()->WaitForRefreshTokensLoaded();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();

  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  // Update cookies in IdentityManager. This will trigger a no-op reconciliation
  // run.
  identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kOk);

  tester.ExpectUniqueSample("Signin.CookieBinding.NeedsUpgradeStatus",
                            /*kFeatureDisabled=*/1, 1);
}

TEST_F(AccountReconcilorTest, CookieBindingUpgradeStatusMetricsNoWrappedKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});
  base::HistogramTester tester;

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);
  // Make primary account available WITHOUT wrapped key.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  identity_test_env()->WaitForRefreshTokensLoaded();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();

  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  // Update cookies in IdentityManager. This will trigger a no-op reconciliation
  // run.
  identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kOk);

  tester.ExpectUniqueSample("Signin.CookieBinding.NeedsUpgradeStatus",
                            /*kNoWrappedKey=*/2, 1);
}

TEST_F(AccountReconcilorTest, CookieBindingUpgradeStatusMetricsNeedsUpgrade) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});
  base::HistogramTester tester;

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  // Set list accounts response first.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Make primary account available WITH wrapped key.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  network::MockDeviceBoundSessionManager mock_session_manager;
  test_signin_client()->set_device_bound_session_manager(&mock_session_manager);
  network::mojom::DeviceBoundSessionManager::GetAllSessionsCallback callback;
  EXPECT_CALL(mock_session_manager, GetAllSessions)
      .WillOnce(SaveArgByMove<0>(&callback));

  AccountReconcilor* reconcilor = GetMockReconcilor();

  base::RunLoop run_loop;
  // Reconcilor will attempt to upgrade, so we must set expectation and call the
  // real implementation.
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce([reconcilor, &run_loop](
                    const signin::MultiloginParameters& parameters,
                    bool is_cookie_upgrade) {
        reconcilor->AccountReconcilor::PerformSetCookiesAction(
            parameters, is_cookie_upgrade);
        run_loop.Quit();
      });

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Deferral should be logged to true since DBSC sessions are not fetched yet.
  tester.ExpectUniqueSample(
      "Signin.CookieBinding.UpgradeReconciliationDeferredOnStartup", true, 1);

  // Complete session fetch.
  std::move(callback).Run({});
  run_loop.Run();

  // Simulate completion of multilogin request to log the duration.
  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send, signin::SetAccountsInCookieResult::kSuccess,
      base::TimeTicks::Now());

  // Update cookies in IdentityManager. This will trigger a no-op reconciliation
  // run.
  identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kOk);

  tester.ExpectUniqueSample("Signin.CookieBinding.NeedsUpgradeStatus",
                            /*kNeedsUpgrade=*/6, 1);
  tester.ExpectTotalCount("Signin.CookieBinding.UpgradeSessionFetchDuration",
                          1);
  tester.ExpectTotalCount("Signin.CookieBinding.UpgradeOAuthMultiloginDuration",
                          1);
}

TEST_F(AccountReconcilorTest,
       CookieBindingUpgradeStatusMetricsHasStandardSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});
  base::HistogramTester tester;

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  // Set list accounts response first.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));

  network::MockDeviceBoundSessionManager mock_session_manager;
  test_signin_client()->set_device_bound_session_manager(&mock_session_manager);
  network::mojom::DeviceBoundSessionManager::GetAllSessionsCallback callback;
  EXPECT_CALL(mock_session_manager, GetAllSessions)
      .WillOnce(SaveArgByMove<0>(&callback));

  identity_test_env()->WaitForRefreshTokensLoaded();

  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Reconcilor will attempt to reconcile, so we must set expectation.
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);

  // Deferral should be logged to true since DBSC sessions are not fetched yet.
  tester.ExpectUniqueSample(
      "Signin.CookieBinding.UpgradeReconciliationDeferredOnStartup", true, 1);

  // Simulate standard session already fetched before StartReconcile completes.
  std::vector<net::device_bound_sessions::SessionKey> sessions;
  sessions.emplace_back(
      net::SchemefulSite(GaiaUrls::GetInstance()->secure_google_url()),
      net::device_bound_sessions::SessionKey::Id("sidts_session"));

  std::move(callback).Run(sessions);
  run_loop.Run();

  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  // Update cookies in IdentityManager. This will trigger a no-op reconciliation
  // run.
  identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kOk);

  tester.ExpectUniqueSample("Signin.CookieBinding.NeedsUpgradeStatus",
                            /*kHasStandardSession=*/3, 1);
}

TEST_F(AccountReconcilorTest,
       CookieBindingUpgradeStatusMetricsUpgradeNotDeferred) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});
  base::HistogramTester tester;

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  // Set list accounts response first.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));

  network::MockDeviceBoundSessionManager mock_session_manager;
  test_signin_client()->set_device_bound_session_manager(&mock_session_manager);
  network::mojom::DeviceBoundSessionManager::GetAllSessionsCallback callback;
  EXPECT_CALL(mock_session_manager, GetAllSessions)
      .WillOnce(SaveArgByMove<0>(&callback));

  identity_test_env()->WaitForRefreshTokensLoaded();

  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Complete session fetch BEFORE starting reconciliation.
  std::move(callback).Run({});

  // Reconcilor will attempt to reconcile, so we must set expectation.
  base::RunLoop run_loop;
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();

  std::vector<CoreAccountId> accounts_to_send = {account_info.GetAccountId()};
  SimulateSetAccountsInCookieCompleted(
      reconcilor, accounts_to_send,
      signin::SetAccountsInCookieResult::kSuccess);

  // Update cookies in IdentityManager. This will trigger a no-op reconciliation
  // run.
  identity_test_env()->SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  EXPECT_EQ(reconcilor->GetState(), AccountReconcilorState::kOk);

  // Deferral should be logged to false since DBSC sessions were already
  // fetched.
  tester.ExpectUniqueSample(
      "Signin.CookieBinding.UpgradeReconciliationDeferredOnStartup", false, 1);
}

TEST_F(AccountReconcilorTest, GetGaiaApiSourceNormalReconcileParameter) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Mock GetCheckConnectionInfo response to allow OAML flow to proceed.
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()
          ->GetCheckConnectionInfoURLWithSource("ChromiumBrowser")
          .spec(),
      "cc_result");

  // Enable automatic token issuance so multilogin helper can get tokens and
  // proceed.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      kFakeEmail, signin::ConsentLevel::kSignin);
  identity_test_env()->WaitForRefreshTokensLoaded();

  auto spy_delegate = std::make_unique<SpyReconcilorDelegate>();
  auto reconcilor = std::make_unique<AccountReconcilor>(
      identity_test_env()->identity_manager(), test_signin_client(),
      std::move(spy_delegate));
  reconcilor->Initialize(/*start_reconcile_if_tokens_available=*/false);

  // Set up interceptor to quit run loop when multilogin request is sent.
  base::RunLoop run_loop;
  test_url_loader_factory_.SetInterceptor(base::BindRepeating(
      [](base::RepeatingClosure quit_closure,
         const network::ResourceRequest& request) {
        if (request.url.path() == "/oauth/multilogin") {
          quit_closure.Run();
        }
      },
      run_loop.QuitClosure()));

  // Trigger reconciliation.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();

  // Verify that the Multilogin request was sent with the correct normal source.
  GURL multilogin_url;
  for (const auto& pending : *test_url_loader_factory_.pending_requests()) {
    if (pending.request.url.path() == "/oauth/multilogin") {
      multilogin_url = pending.request.url;
      break;
    }
  }
  ASSERT_FALSE(multilogin_url.is_empty());
  EXPECT_TRUE(multilogin_url.query().find(
                  "source=ChromiumAccountReconcilorDice") != std::string::npos);

  reconcilor->Shutdown();
}

TEST_F(AccountReconcilorTest, GetGaiaApiSourceCookieUpgradeParameter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({net::features::kDeviceBoundSessions,
                                 switches::kEnableCookieBindingCookieUpgrade},
                                {});

  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  // Mock GetCheckConnectionInfo response to allow OAML flow to proceed.
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()
          ->GetCheckConnectionInfoURLWithSource("ChromiumBrowser")
          .spec(),
      "cc_result");

  // Enable automatic token issuance so multilogin helper can get tokens and
  // proceed.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // Make primary account available with a bound key.
  auto builder = identity_test_env()->CreateAccountAvailabilityOptionsBuilder();
  builder.AsPrimary(signin::ConsentLevel::kSignin)
      .WithRefreshTokenBindingInfo(signin::TokenBindingInfo(
          /*wrapped_binding_key=*/{1, 2, 3, 4},
          /*mtls_token_binding=*/false));
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(builder.Build(kFakeEmail));
  identity_test_env()->WaitForRefreshTokensLoaded();

  auto spy_delegate = std::make_unique<SpyReconcilorDelegate>();
  auto reconcilor = std::make_unique<AccountReconcilor>(
      identity_test_env()->identity_manager(), test_signin_client(),
      std::move(spy_delegate));
  reconcilor->Initialize(/*start_reconcile_if_tokens_available=*/false);

  // Feed empty DBSC sessions so that upgrade preconditions are met.
  reconcilor->OnDeviceBoundSessionsFetched(std::nullopt, {});

  // Set up interceptor to quit run loop when multilogin request is sent.
  base::RunLoop run_loop;
  test_url_loader_factory_.SetInterceptor(base::BindRepeating(
      [](base::RepeatingClosure quit_closure,
         const network::ResourceRequest& request) {
        if (request.url.path() == "/oauth/multilogin") {
          quit_closure.Run();
        }
      },
      run_loop.QuitClosure()));

  // Trigger reconciliation.
  reconcilor->StartReconcile(AccountReconcilor::Trigger::kCookieChange);
  run_loop.Run();

  // Verify that the Multilogin request was sent with the correct upgrade
  // source.
  GURL multilogin_url;
  for (const auto& pending : *test_url_loader_factory_.pending_requests()) {
    if (pending.request.url.path() == "/oauth/multilogin") {
      multilogin_url = pending.request.url;
      break;
    }
  }
  ASSERT_FALSE(multilogin_url.is_empty());
  EXPECT_TRUE(multilogin_url.query().find(
                  "source=ChromiumAccountReconcilorDiceCookieUpgrade") !=
              std::string::npos);

  reconcilor->Shutdown();
}
#endif
