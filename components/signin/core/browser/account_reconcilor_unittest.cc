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
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_account_reconcilor_delegate.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
using FakeSigninManagerForTesting = FakeSigninManagerBase;
#else
using FakeSigninManagerForTesting = FakeSigninManager;
#endif

// TestSigninClient keeping track of the dice migration.
class DiceTestSigninClient : public TestSigninClient {
 public:
  explicit DiceTestSigninClient(PrefService* prefs) : TestSigninClient(prefs) {}

  void SetReadyForDiceMigration(bool ready) override {
    is_ready_for_dice_migration_ = ready;
  }

  bool is_ready_for_dice_migration() { return is_ready_for_dice_migration_; }

 private:
  bool is_ready_for_dice_migration_ = false;
  DISALLOW_COPY_AND_ASSIGN(DiceTestSigninClient);
};

// An AccountReconcilorDelegate that records all calls (Spy pattern).
class SpyReconcilorDelegate : public signin::AccountReconcilorDelegate {
 public:
  int num_reconcile_finished_calls_{0};
  int num_reconcile_timeout_calls_{0};

  bool IsReconcileEnabled() const override { return true; }

  bool IsAccountConsistencyEnforced() const override { return true; }

  std::string GetGaiaApiSource() const override { return "TestSource"; }

  bool ShouldAbortReconcileIfPrimaryHasError() const override { return true; }

  std::string GetFirstGaiaAccountForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string& primary_account,
      bool first_execution,
      bool will_logout) const override {
    return primary_account;
  }

  void OnReconcileFinished(const std::string& first_account,
                           bool reconcile_is_noop) override {
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
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      SigninClient* client,
      GaiaCookieManagerService* cookie_manager_service,
      signin::AccountConsistencyMethod account_consistency)
      : AccountReconcilor(
            token_service,
            signin_manager,
            client,
            cookie_manager_service,
            CreateAccountReconcilorDelegate(client,
                                            signin_manager,
                                            account_consistency)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  // Takes ownership of |delegate|.
  // gmock can't work with move only parameters.
  DummyAccountReconcilorWithDelegate(
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      SigninClient* client,
      GaiaCookieManagerService* cookie_manager_service,
      signin::AccountReconcilorDelegate* delegate)
      : AccountReconcilor(
            token_service,
            signin_manager,
            client,
            cookie_manager_service,
            std::unique_ptr<signin::AccountReconcilorDelegate>(delegate)) {
    Initialize(false /* start_reconcile_if_tokens_available */);
  }

  static std::unique_ptr<signin::AccountReconcilorDelegate>
  CreateAccountReconcilorDelegate(
      SigninClient* signin_client,
      SigninManagerBase* signin_manager,
      signin::AccountConsistencyMethod account_consistency) {
    switch (account_consistency) {
      case signin::AccountConsistencyMethod::kMirror:
        return std::make_unique<signin::MirrorAccountReconcilorDelegate>(
            signin_manager);
      case signin::AccountConsistencyMethod::kDisabled:
      case signin::AccountConsistencyMethod::kDiceFixAuthErrors:
        return std::make_unique<signin::AccountReconcilorDelegate>();
      case signin::AccountConsistencyMethod::kDiceMigration:
      case signin::AccountConsistencyMethod::kDice:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        return std::make_unique<signin::DiceAccountReconcilorDelegate>(
            signin_client, account_consistency);
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
  explicit MockAccountReconcilor(
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      SigninClient* client,
      GaiaCookieManagerService* cookie_manager_service,
      signin::AccountConsistencyMethod account_consistency);

  explicit MockAccountReconcilor(
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      SigninClient* client,
      GaiaCookieManagerService* cookie_manager_service,
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  MOCK_METHOD1(PerformMergeAction, void(const std::string& account_id));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
  MOCK_METHOD1(PerformSetCookiesAction,
               void(const signin::MultiloginParameters& parameters));
};

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service,
    signin::AccountConsistencyMethod account_consistency)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          token_service,
          signin_manager,
          client,
          cookie_manager_service,
          account_consistency) {}

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service,
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate)
    : testing::StrictMock<DummyAccountReconcilorWithDelegate>(
          token_service,
          signin_manager,
          client,
          cookie_manager_service,
          delegate.release()) {}

struct Cookie {
  std::string gaia_id;
  bool is_valid;

  bool operator==(const Cookie& other) const {
    return gaia_id == other.gaia_id && is_valid == other.is_valid;
  }
};

}  // namespace

class AccountReconcilorTest : public ::testing::Test {
 public:
  AccountReconcilorTest();
  ~AccountReconcilorTest() override;

  FakeSigninManagerForTesting* signin_manager() { return &signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }
  DiceTestSigninClient* test_signin_client() { return &test_signin_client_; }
  AccountTrackerService* account_tracker() { return &account_tracker_; }
  FakeGaiaCookieManagerService* cookie_manager_service() {
    return &cookie_manager_service_;
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  MockAccountReconcilor* GetMockReconcilor();
  MockAccountReconcilor* GetMockReconcilor(
      std::unique_ptr<signin::AccountReconcilorDelegate> delegate);

  std::string ConnectProfileToAccount(const std::string& gaia_id,
                                      const std::string& username);

  std::string PickAccountIdForAccount(const std::string& gaia_id,
                                      const std::string& username);

  void SimulateAddAccountToCookieCompleted(
      GaiaCookieManagerService::Observer* observer,
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  void SimulateSetAccountsInCookieCompleted(
      GaiaCookieManagerService::Observer* observer,
      const GoogleServiceAuthError& error);

  GURL get_check_connection_info_url() {
    return get_check_connection_info_url_;
  }

  void SetAccountConsistency(signin::AccountConsistencyMethod method);

  PrefService* pref_service() { return &pref_service_; }

  void DeleteReconcilor() { mock_reconcilor_.reset(); }

 private:
  base::MessageLoop loop;
  signin::AccountConsistencyMethod account_consistency_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  DiceTestSigninClient test_signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_;
  FakeGaiaCookieManagerService cookie_manager_service_;
  FakeSigninManagerForTesting signin_manager_;
  std::unique_ptr<MockAccountReconcilor> mock_reconcilor_;
  base::HistogramTester histogram_tester_;
  GURL get_check_connection_info_url_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTest);
};

class AccountReconcilorMirrorEndpointParamTest
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<bool> {
 public:
  AccountReconcilorMirrorEndpointParamTest() {
    SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
    if (IsMultiloginEnabled())
      scoped_feature_list_.InitAndEnableFeature(kUseMultiloginEndpoint);
  }
  bool IsMultiloginEnabled() { return GetParam(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorMirrorEndpointParamTest);
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

INSTANTIATE_TEST_CASE_P(Dice_Mirror,
                        AccountReconcilorMethodParamTest,
                        ::testing::Values(
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                            signin::AccountConsistencyMethod::kDice,
#endif
                            signin::AccountConsistencyMethod::kMirror));

AccountReconcilorTest::AccountReconcilorTest()
    : account_consistency_(signin::AccountConsistencyMethod::kDisabled),
      test_signin_client_(&pref_service_),
      token_service_(&pref_service_),
      cookie_manager_service_(&token_service_,
                              GaiaConstants::kChromeSource,
                              &test_signin_client_),
#if defined(OS_CHROMEOS)
      signin_manager_(&test_signin_client_, &account_tracker_) {
#else
      signin_manager_(&test_signin_client_,
                      &token_service_,
                      &account_tracker_,
                      &cookie_manager_service_) {
#endif
  AccountTrackerService::RegisterPrefs(pref_service_.registry());
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
  SigninManagerBase::RegisterPrefs(pref_service_.registry());
  pref_service_.registry()->RegisterBooleanPref(
      prefs::kTokenServiceDiceCompatible, false);
  get_check_connection_info_url_ =
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          GaiaConstants::kChromeSource);

  account_tracker_.Initialize(&pref_service_, base::FilePath());
  cookie_manager_service_.SetListAccountsResponseHttpNotFound();
  signin_manager_.Initialize(nullptr);

  // The reconcilor should not be built before the test can set the account
  // consistency method.
  EXPECT_FALSE(mock_reconcilor_);
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
        &token_service_, &signin_manager_, &test_signin_client_,
        &cookie_manager_service_, account_consistency_);
  }

  return mock_reconcilor_.get();
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor(
    std::unique_ptr<signin::AccountReconcilorDelegate> delegate) {
  mock_reconcilor_ = std::make_unique<MockAccountReconcilor>(
      &token_service_, &signin_manager_, &test_signin_client_,
      &cookie_manager_service_, std::move(delegate));

  return mock_reconcilor_.get();
}

AccountReconcilorTest::~AccountReconcilorTest() {
  if (mock_reconcilor_)
    mock_reconcilor_->Shutdown();
  signin_manager_.Shutdown();
  cookie_manager_service_.Shutdown();
  account_tracker_.Shutdown();
  test_signin_client_.Shutdown();
  token_service_.Shutdown();
}

std::string AccountReconcilorTest::ConnectProfileToAccount(
    const std::string& gaia_id,
    const std::string& username) {
  const std::string account_id = PickAccountIdForAccount(gaia_id, username);
#if !defined(OS_CHROMEOS)
  signin_manager()->set_password("password");
#endif
  signin_manager()->SetAuthenticatedAccountInfo(gaia_id, username);
  token_service()->UpdateCredentials(account_id, "refresh_token");
  return account_id;
}

std::string AccountReconcilorTest::PickAccountIdForAccount(
    const std::string& gaia_id,
    const std::string& username) {
  return account_tracker()->PickAccountIdForAccount(gaia_id, username);
}

void AccountReconcilorTest::SimulateAddAccountToCookieCompleted(
    GaiaCookieManagerService::Observer* observer,
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer->OnAddAccountToCookieCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateSetAccountsInCookieCompleted(
    GaiaCookieManagerService::Observer* observer,
    const GoogleServiceAuthError& error) {
  observer->OnSetAccountsInCookieCompleted(error);
}

void AccountReconcilorTest::SimulateCookieContentSettingsChanged(
    content_settings::Observer* observer,
    const ContentSettingsPattern& primary_pattern) {
  observer->OnContentSettingChanged(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string());
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
  const char* gaia_api_calls_multilogin;
  const char* tokens_after_reconcile_multilogin;
  const char* cookies_after_reconcile_multilogin;
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

// Parameterized version of AccountReconcilorTest.
class AccountReconcilorTestTable
    : public AccountReconcilorTest,
      public ::testing::WithParamInterface<AccountReconcilorTestTableParam> {
 protected:
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

  AccountReconcilorTestTable() {
    accounts_['A'] = {"a@gmail.com", "A"};
    accounts_['B'] = {"b@gmail.com", "B"};
    accounts_['C'] = {"c@gmail.com", "C"};
  }

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
    EXPECT_EQ(token_service()->GetAccounts().size(), tokens.size());
    bool authenticated_account_found = false;
    for (const Token& token : tokens) {
      std::string account_id =
          PickAccountIdForAccount(token.gaia_id, token.email);
      EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id));
      EXPECT_EQ(token.has_error,
                token_service()->RefreshTokenHasError(account_id));
      if (token.is_authenticated) {
        EXPECT_EQ(account_id, signin_manager()->GetAuthenticatedAccountId());
        authenticated_account_found = true;
      }
    }
    if (!authenticated_account_found)
      EXPECT_EQ("", signin_manager()->GetAuthenticatedAccountId());
  }

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

  void ConfigureCookieManagerService(const std::vector<Cookie>& cookies) {
    std::vector<FakeGaiaCookieManagerService::CookieParams> cookie_params;
    for (const auto& cookie : cookies) {
      std::string gaia_id = cookie.gaia_id;
      cookie_params.push_back({accounts_[gaia_id[0]].email, gaia_id,
                               cookie.is_valid, false /* signed_out */,
                               true /* verified */});
    }
    cookie_manager_service()->SetListAccountsResponseWithParams(cookie_params);
    cookie_manager_service()->set_list_accounts_stale_for_testing(true);
  }

  std::map<char, Account> accounts_;
};

#if !defined(OS_CHROMEOS)

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_P(AccountReconcilorMirrorEndpointParamTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());

  account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());
}

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_P(AccountReconcilorMirrorEndpointParamTest, Reauth) {
  const std::string email = "user@gmail.com";
  const std::string account_id = ConnectProfileToAccount("12345", email);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Simulate reauth.  The state of the reconcilor should not change.
  signin_manager()->OnExternalSigninCompleted(email);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#endif  // !defined(OS_CHROMEOS)

TEST_P(AccountReconcilorMirrorEndpointParamTest, ProfileAlreadyConnected) {
  ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {
std::vector<Cookie> FakeSetAccountsInCookie(
    const signin::MultiloginParameters& parameters,
    const std::vector<Cookie>& cookies_before_reconcile) {
  std::vector<Cookie> cookies_after_reconcile;
  if (parameters.mode ==
      signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER) {
    for (const std::string& account : parameters.accounts_to_send) {
      cookies_after_reconcile.push_back({account, true});
    }
  } else {
    std::set<std::string> accounts_set;
    for (const std::string& account : parameters.accounts_to_send) {
      accounts_set.insert(account);
    }
    cookies_after_reconcile = cookies_before_reconcile;
    for (Cookie& param : cookies_after_reconcile) {
      if (accounts_set.find(param.gaia_id) != accounts_set.end()) {
        param.is_valid = true;
        accounts_set.erase(param.gaia_id);
      } else {
        param.is_valid = false;
      }
    }
    for (const std::string& account : accounts_set) {
      cookies_after_reconcile.push_back({account, true});
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
    //   *: The next account is the main Chrome account (i.e. in SigninManager).
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
    // Tokens|Cookies|First Run|Gaia calls|Tokens aft.|Cookies aft.|M.calls| M.Tokens aft.| M.Cookies aft.|
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "*AB",   "AB",  IsFirstReconcile::kBoth,       "",    "*AB",   "AB",  "",    "*AB",  "AB"},

    {  "*AB",   "BA",  IsFirstReconcile::kFirst,      "XAB", "*AB",   "AB",  "UAB", "*AB",  "AB"},
    {  "*AB",   "BA",  IsFirstReconcile::kNotFirst,   "",    "*AB",   "BA",  "",    "*AB",  "BA"}, 

    {  "*AB",   "A",   IsFirstReconcile::kBoth,       "B",   "*AB",   "AB",  "PAB", "*AB",  "AB"},

    {  "*AB",   "B",   IsFirstReconcile::kFirst,      "XAB", "*AB",   "AB",  "UAB", "*AB",  "AB"},
    {  "*AB",   "B",   IsFirstReconcile::kNotFirst,   "A",   "*AB",   "BA",  "PAB", "*AB",  "BA"},

    {  "*AB",   "",    IsFirstReconcile::kFirst,      "AB",  "*AB",   "AB",  "UAB", "*AB",  "AB"},
    {  "*AB",   "",    IsFirstReconcile::kNotFirst,   "AB",  "*AB",   "AB",  "PAB", "*AB",  "AB"},
    // Sync enabled, token error on primary.
    {  "*xAB",  "AB",  IsFirstReconcile::kBoth,       "X",   "*xA",   "" ,   "PB",  "*xAB", "xAB"},
    {  "*xAB",  "BA",  IsFirstReconcile::kBoth,       "XB",  "*xAB",  "B",   "PB",  "*xAB", "BxA"},
    {  "*xAB",  "A",   IsFirstReconcile::kBoth,       "X",   "*xA",   "" ,   "PB",  "*xAB", "xAB"},
    {  "*xAB",  "B",   IsFirstReconcile::kBoth,       "",    "*xAB",  "B",   "",   "*xAB", "B"  },
    {  "*xAB",  "",    IsFirstReconcile::kBoth,       "B",   "*xAB",  "B",   "PB",  "*xAB", "B"  },
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",  IsFirstReconcile::kBoth,       "XA",  "*A",    "A",   "PA",  "*A",   "AxB"},

    {  "*AxB",  "BA",  IsFirstReconcile::kFirst,      "XA",  "*A",    "A",   "UA",  "*A",   "A"  },
    {  "*AxB",  "BA",  IsFirstReconcile::kNotFirst,   "XA",  "*A",    "A",   "PA",  "*A",   "xBA"},

    {  "*AxB",  "A",   IsFirstReconcile::kBoth,       "",    "*A",    "A",   "",    "*A",   "A"  },

    {  "*AxB",  "B",   IsFirstReconcile::kFirst,      "XA",  "*A",    "A",   "UA",  "*A",   "A"  },
    {  "*AxB",  "B",   IsFirstReconcile::kNotFirst,   "XA",  "*A",    "A",   "PA",  "*A",   "xBA"},

    {  "*AxB",  "",    IsFirstReconcile::kFirst,      "A",   "*A",    "A",   "UA",  "*A",   "A"  },
    {  "*AxB",  "",    IsFirstReconcile::kNotFirst,   "A",   "*A",    "A",   "PA",  "*A",   "A"},
    // Sync enabled, token error on both accounts.
    {  "*xAxB", "AB",  IsFirstReconcile::kBoth,       "X",   "*xA",   "",    "P",   "*xA",  "xAxB"},
    {  "*xAxB", "BA",  IsFirstReconcile::kBoth,       "X",   "*xA",   "",    "P",   "*xA",  "xBxA"},
    {  "*xAxB", "A",   IsFirstReconcile::kBoth,       "X",   "*xA",   "",    "P",   "*xA",  "xA"  },
    {  "*xAxB", "B",   IsFirstReconcile::kBoth,       "X",   "*xA",   "",    "P",   "*xA",  "xB"  },
    {  "*xAxB", "",    IsFirstReconcile::kBoth,       "",    "*xA",   "",    "",    "*xA",  ""    },
    // Sync disabled.
    {  "AB",    "AB",  IsFirstReconcile::kBoth,       "",    "AB",    "AB",  "",    "AB",   "AB"},
    {  "AB",    "BA",  IsFirstReconcile::kBoth,       "",    "AB",    "BA",  "",    "AB",   "BA"},
    {  "AB",    "A",   IsFirstReconcile::kBoth,       "B",   "AB",    "AB",  "PAB", "AB",   "AB"},
    {  "AB",    "B",   IsFirstReconcile::kBoth,       "A",   "AB",    "BA",  "PAB", "AB",   "BA"},
    {  "AB",    "",    IsFirstReconcile::kBoth,       "AB",  "AB",    "AB",  "PAB", "AB",   "AB"},
    // Sync disabled, token error on first account.
    {  "xAB",   "AB",  IsFirstReconcile::kFirst,      "XB",  "B",     "B",   "PB",  "B",    "xAB"},
    {  "xAB",   "AB",  IsFirstReconcile::kNotFirst,   "X",   "",      "" ,   "PB",  "B",    "xAB"},

    {  "xAB",   "BA",  IsFirstReconcile::kBoth,       "XB",  "B",     "B",   "PB",  "B",    "BxA"},

    {  "xAB",   "A",   IsFirstReconcile::kFirst,      "XB",  "B",     "B",   "PB",  "B",    "xAB"},
    {  "xAB",   "A",   IsFirstReconcile::kNotFirst,   "X",   "",      "" ,   "PB",  "B",    "xAB"},

    {  "xAB",   "B",   IsFirstReconcile::kBoth,       "",    "B",     "B",   "",    "B",    "B"},

    {  "xAB",   "",    IsFirstReconcile::kBoth,       "B",   "B",     "B",   "PB",  "B",    "B"},
    // Sync disabled, token error on second account   .
    {  "AxB",   "AB",  IsFirstReconcile::kBoth,       "XA",  "A",     "A",   "PA",  "A",    "AxB"},

    {  "AxB",   "BA",  IsFirstReconcile::kFirst,      "XA",  "A",     "A",   "PA",  "A",    "xBA"},
    {  "AxB",   "BA",  IsFirstReconcile::kNotFirst,   "X",  "",       "" ,   "PA",  "A",    "xBA"},

    {  "AxB",   "A",   IsFirstReconcile::kBoth,       "",    "A",     "A",   "",    "A",    "A"},

    {  "AxB",   "B",   IsFirstReconcile::kFirst,      "XA",  "A",     "A",   "PA",  "A",    "xBA"},
    {  "AxB",   "B",   IsFirstReconcile::kNotFirst,   "X",   "",      "" ,   "PA",  "A",    "xBA"},

    {  "AxB",   "",    IsFirstReconcile::kBoth,       "A",   "A",     "A",   "PA",  "A",    "A"},
    // Sync disabled, token error on both accounts.
    {  "xAxB",  "AB",  IsFirstReconcile::kBoth,       "X",   "",      "",    "P",   "",     "xAxB"},
    {  "xAxB",  "BA",  IsFirstReconcile::kBoth,       "X",   "",      "",    "P",   "",     "xBxA"},
    {  "xAxB",  "A",   IsFirstReconcile::kBoth,       "X",   "",      "",    "P",   "",     "xA"},
    {  "xAxB",  "B",   IsFirstReconcile::kBoth,       "X",   "",      "",    "P",   "",     "xB"},
    {  "xAxB",  "",    IsFirstReconcile::kBoth,       "",    "",      "",    "",    "",     ""}, 
    // Account marked as invalid in cookies.
    // No difference between cookies and tokens, do not do do anything.
    // Do not logout. Regression tests for http://crbug.com/854799
    {  "",     "xA",   IsFirstReconcile::kBoth,       "",    "",      "xA",   "",   "",     "xA"},
    {  "",     "xAxB", IsFirstReconcile::kBoth,       "",    "",      "xAxB", "",   "",     "xAxB"},
    {  "xA",   "xA",   IsFirstReconcile::kBoth,       "",    "",      "xA",   "",   "",     "xA"},
    {  "xAB",  "xAB",  IsFirstReconcile::kBoth,       "",    "B",     "xAB",  "",   "B",    "xAB"},
    {  "AxB",  "AxC",  IsFirstReconcile::kBoth,       "",    "A",     "AxC",  "",   "A",    "AxC"},
    {  "B",    "xAB",  IsFirstReconcile::kBoth,       "",    "B",     "xAB",  "",   "B",    "xAB"},
    {  "*xA",  "xA",   IsFirstReconcile::kBoth,       "",    "*xA",   "xA",   "",   "*xA",  "xA"},
    {  "*xA",  "xB",   IsFirstReconcile::kBoth,       "",    "*xA",   "xB",   "",   "*xA",  "xB"},
    {  "*xAB", "xAB",  IsFirstReconcile::kBoth,       "",    "*xAB",  "xAB",  "",   "*xAB", "xAB"},
    // Appending a new cookie after the invalid one.    
    {  "B",    "xA",   IsFirstReconcile::kBoth,       "B",   "B",     "xAB",  "PB", "B",    "xAB"},
    {  "xAB",  "xA",   IsFirstReconcile::kBoth,       "B",   "B",     "xAB",  "PB", "B",    "xAB"},
    // Refresh existing cookies.   
    {  "AB",   "xAB",  IsFirstReconcile::kBoth,       "A",   "AB",    "AB",   "PAB","AB",   "AB"},
    {  "*AB",  "xBxA", IsFirstReconcile::kNotFirst,   "BA",  "*AB",   "BA",   "PAB","*AB",  "BA"},
    // Appending and invalidating cookies at the same time.    
    // Difference should disappear after migrating to Multilogin.
    {  "xAB",  "xAC",  IsFirstReconcile::kFirst,      "XB",  "B",     "B",    "PB", "B",   "xAxCB"},
    {  "xAB",  "xAC",  IsFirstReconcile::kNotFirst,   "X",   "",      "",     "PB", "B",   "xAxCB"},

    {  "xAB",  "AxC",  IsFirstReconcile::kFirst,      "XB", "B",      "B",    "PB", "B",   "xAxCB"},
    {  "xAB",  "AxC",  IsFirstReconcile::kNotFirst,   "X",  "",       "",     "PB", "B",   "xAxCB"},

    {  "*xAB", "xABC", IsFirstReconcile::kFirst,      "XB", "*xAB",   "B",    "PB", "*xAB","xABxC"},
    {  "*xAB", "xABC", IsFirstReconcile::kNotFirst,   "X",  "*xA",    "",     "PB", "*xAB","xABxC"},

    {  "xAB",  "xABC", IsFirstReconcile::kFirst,      "XB", "B",      "B",    "PB", "B",   "xABxC"},
    {  "xAB",  "xABC", IsFirstReconcile::kNotFirst,   "X",  "",       "",     "PB", "B",   "xABxC"},

    // Miscellaneous cases.
    // Check that unknown Gaia accounts are signed out.
    {  "",     "A",    IsFirstReconcile::kBoth,       "X",  "",       "",     "P",  "",    "xA"},
    {  "*A",   "AB",   IsFirstReconcile::kBoth,       "XA", "*A",     "A",    "PA", "*A",  "AxB"},
    // Check that Gaia default account is kept in first position.   
    {  "AB",   "BC",   IsFirstReconcile::kBoth,      "XBA", "AB",     "BA",   "PAB","AB",  "BxCA"},
    // Check that Gaia cookie order preserved except for the first one.
    {  "*ABC", "CB",   IsFirstReconcile::kFirst,     "XABC", "*ABC",  "ABC",  "UACB","*ABC","ACB"},
    // Required for idempotency check.
    {  "",     "",     IsFirstReconcile::kNotFirst,    "",  "",       "",     "",   "",    ""},
    {  "",     "xA",   IsFirstReconcile::kNotFirst,    "",  "",       "xA",   "",   "",    "xA"},
    {  "",     "xB",   IsFirstReconcile::kNotFirst,    "",  "",       "xB",   "",   "",    "xB"},
    {  "",     "xAxB", IsFirstReconcile::kNotFirst,    "",  "",       "xAxB", "",   "",    "xAxB"},
    {  "",     "xBxA", IsFirstReconcile::kNotFirst,    "",  "",       "xBxA", "",   "",    "xBxA"},
    {  "*A",   "A",    IsFirstReconcile::kNotFirst,    "",  "*A",     "A",    "",   "*A",  "A"},
    {  "*A",   "xBA",  IsFirstReconcile::kNotFirst,    "XA","*A",     "A",    "",   "*A",  "xBA"},
    {  "*A",   "AxB",  IsFirstReconcile::kNotFirst,    "",  "*A",     "AxB",  "",   "*A",  "AxB"},
    {  "A",    "A",    IsFirstReconcile::kNotFirst,    "",  "A",      "A",    "",   "A",   "A"},
    {  "A",    "xBA",  IsFirstReconcile::kNotFirst,    "",  "A",      "xBA",  "",   "A",   "xBA"},
    {  "A",    "AxB",  IsFirstReconcile::kNotFirst,    "",  "A",      "AxB",  "",   "A",   "AxB"},
    {  "B",    "B",    IsFirstReconcile::kNotFirst,    "",  "B",      "B",    "",   "B",   "B"},
    {  "B",    "xAB",  IsFirstReconcile::kNotFirst,    "",  "B",      "xAB",  "",   "B",   "xAB"},
    {  "B",    "BxA",  IsFirstReconcile::kNotFirst,    "",  "B",      "BxA",  "",   "B",   "BxA"},
    {  "*xA",  "",     IsFirstReconcile::kNotFirst,    "",  "*xA",    "",     "",   "*xA", ""},
    {  "*xA",  "xAxB", IsFirstReconcile::kNotFirst,    "",  "*xA",    "xAxB", "",   "*xA", "xAxB"},
    {  "*xA",  "xBxA", IsFirstReconcile::kNotFirst,    "",  "*xA",    "xBxA", "",   "*xA", "xBxA"},
    {  "*xA",  "xA",   IsFirstReconcile::kNotFirst,    "",  "*xA",    "xA",   "",   "*xA", "xA"},  
    {  "*xA",  "xB",   IsFirstReconcile::kNotFirst,    "",  "*xA",    "xB",   "",   "*xA", "xB"},
    {  "*xAB", "B",    IsFirstReconcile::kNotFirst,    "",  "*xAB",   "B",    "",   "*xAB","B"},
    {  "*xAB", "BxA",  IsFirstReconcile::kNotFirst,    "",  "*xAB",   "BxA",  "",   "*xAB","BxA"},
    {  "*xAB", "xAB",  IsFirstReconcile::kNotFirst,    "",  "*xAB",   "xAB",  "",   "*xAB","xAB"},
    {  "*xAB", "xABxC",IsFirstReconcile::kNotFirst,    "",  "*xAB",   "xABxC","",   "*xAB","xABxC"},
    {  "A",    "AxC",  IsFirstReconcile::kNotFirst,    "",  "A",      "AxC",  "",   "A",   "AxC"},
    {  "AB",   "BxCA", IsFirstReconcile::kNotFirst,    "",  "AB",     "BxCA", "",   "AB",  "BxCA"},
    {  "B",    "xABxC",IsFirstReconcile::kNotFirst,    "",  "B",      "xABxC","",   "B",   "xABxC"},
    {  "B",    "xAxCB",IsFirstReconcile::kNotFirst,    "",  "B",      "xAxCB","",   "B",   "xAxCB"},
    {  "*ABC", "ACB",  IsFirstReconcile::kNotFirst,    "",  "*ABC",   "ACB",  "",   "*ABC","ACB"},
    {  "*ABC", "ABC",  IsFirstReconcile::kNotFirst,    "",  "*ABC",   "ABC",  "",   "*ABC","ABC"}
};
// clang-format on

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestTable, TableRowTest) {
  // Enable Dice.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  // Check that reconcile is idempotent: when called twice in a row it should do
  // nothing on the second call.
  CheckReconcileIdempotent(kDiceParams, GetParam(), /*multilogin=*/false);

  // Setup tokens.
  std::vector<Token> tokens_before_reconcile =
      ParseTokenString(GetParam().tokens);
  for (const Token& token : tokens_before_reconcile) {
    std::string account_id =
        PickAccountIdForAccount(token.gaia_id, token.email);
    if (token.is_authenticated)
      ConnectProfileToAccount(token.gaia_id, token.email);
    else
      token_service()->UpdateCredentials(account_id, "refresh_token");
    if (token.has_error) {
      token_service()->UpdateAuthErrorForTesting(
          account_id, GoogleServiceAuthError(
                          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    }
  }
  VerifyCurrentTokens(tokens_before_reconcile);

  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);

  // Call list accounts now so that the next call completes synchronously.
  cookie_manager_service()->ListAccounts(nullptr, nullptr, "foo");
  base::RunLoop().RunUntilIdle();

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
    std::string cookie(1, GetParam().gaia_api_calls[i]);
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(cookie)).Times(1);
    // MergeSession fixes an existing cookie or appends it at the end.
    auto it = std::find(cookies.begin(), cookies.end(),
                        Cookie{cookie, false /* is_valid */});
    if (it == cookies.end())
      cookies.push_back({cookie, true});
    else
      it->is_valid = true;
  }
  if (!logout_action) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(0);
  }

  // Check the expected cookies after reconcile.
  std::vector<Cookie> expected_cookies =
      ParseCookieString(GetParam().cookies_after_reconcile);
  ASSERT_EQ(expected_cookies, cookies);

  // Reconcile.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ =
      GetParam().is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
  reconcilor->StartReconcile();
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'X')
      continue;
    std::string account_id =
        PickAccountIdForAccount(accounts_[GetParam().gaia_api_calls[i]].gaia_id,
                                accounts_[GetParam().gaia_api_calls[i]].email);
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  VerifyCurrentTokens(ParseTokenString(GetParam().tokens_after_reconcile));

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

INSTANTIATE_TEST_CASE_P(
    DiceTable,
    AccountReconcilorTestTable,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kDiceParams)));

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
  // Enable Mirror.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);
  scoped_feature_list_.InitAndEnableFeature(kUseMultiloginEndpoint);

  CheckReconcileIdempotent(kDiceParams, GetParam(), /*multilogin=*/true);

  // Setup tokens.
  std::vector<Token> tokens_before_reconcile =
      ParseTokenString(GetParam().tokens);
  Token primary_account;
  for (const Token& token : tokens_before_reconcile) {
    std::string account_id =
        PickAccountIdForAccount(token.gaia_id, token.email);
    if (token.is_authenticated) {
      primary_account = token;
      ConnectProfileToAccount(token.gaia_id, token.email);
    } else {
      token_service()->UpdateCredentials(account_id, "refresh_token");
    }
    if (token.has_error) {
      token_service()->UpdateAuthErrorForTesting(
          account_id, GoogleServiceAuthError(
                          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    }
  }
  VerifyCurrentTokens(tokens_before_reconcile);

  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);
  std::vector<Cookie> cookies_after_reconcile = cookies;

  // Call list accounts now so that the next call completes synchronously.
  cookie_manager_service()->ListAccounts(nullptr, nullptr, "foo");
  base::RunLoop().RunUntilIdle();

  // Setup expectations.
  testing::InSequence mock_sequence;
  if (GetParam().gaia_api_calls_multilogin[0] != '\0') {
    signin::MultiloginMode mode =
        GetParam().gaia_api_calls_multilogin[0] == 'U'
            ? signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER
            : signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER;
    // Generate expected array of accounts in cookies and set fake gaia
    // response.
    std::vector<std::string> accounts_to_send;
    for (int i = 1; GetParam().gaia_api_calls_multilogin[i] != '\0'; ++i) {
      accounts_to_send.push_back(
          accounts_[GetParam().gaia_api_calls_multilogin[i]].gaia_id);
    }
    const signin::MultiloginParameters params(mode, accounts_to_send);
    cookies_after_reconcile = FakeSetAccountsInCookie(params, cookies);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params)).Times(1);
  }
  // Reconcile.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->first_execution_);
  reconcilor->first_execution_ =
      GetParam().is_first_reconcile == IsFirstReconcile::kFirst ? true : false;
  reconcilor->StartReconcile();

  SimulateSetAccountsInCookieCompleted(reconcilor,
                                       GoogleServiceAuthError::AuthErrorNone());

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
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

INSTANTIATE_TEST_CASE_P(
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
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Reconcilor should not logout all accounts from the cookies when
  // SigninManager signs out.
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(::testing::_))
      .Times(0);

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

// Tests that reconcile starts even when Sync is not enabled.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceReconcileWithoutSignin) {
  // Add a token in Chrome but do not sign in.
  const std::string account_id =
      PickAccountIdForAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<std::string> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
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
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that nothing happens when there is no Chrome account and no Gaia
// cookie.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceReconcileNoop) {
  // No Chrome account and no cookie.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
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
  // Add accounts 1 and 2 to the token service.
  const std::string account_id_1 =
      PickAccountIdForAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id_1, "refresh_token");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");

  ASSERT_EQ(2u, token_service()->GetAccounts().size());
  ASSERT_EQ(account_id_1, token_service()->GetAccounts()[0]);
  ASSERT_EQ(account_id_2, token_service()->GetAccounts()[1]);

  // Add accounts 2 and 3 to the Gaia cookie.
  const std::string account_id_3 =
      PickAccountIdForAccount("9999", "foo@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "foo@gmail.com", "9999");

  if (!IsMultiloginEnabled()) {
    testing::InSequence mock_sequence;
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
    // Account 2 is added first.
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_2));
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1));
  } else {
    std::vector<std::string> accounts_to_send = {account_id_1, account_id_2};
    // Send accounts to Gaia in any order, it will determine the order itself in
    // PRESERVE order.
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_1, GoogleServiceAuthError::AuthErrorNone());
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_2, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the first account is kept in cache and reused when cookies are
// lost.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceLastKnownFirstAccount) {
  // Add accounts to the token service and the Gaia cookie in a different order.
  const std::string account_id_1 =
      PickAccountIdForAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "user@gmail.com", "12345");
  token_service()->UpdateCredentials(account_id_1, "refresh_token");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");

  ASSERT_EQ(2u, token_service()->GetAccounts().size());
  ASSERT_EQ(account_id_1, token_service()->GetAccounts()[0]);
  ASSERT_EQ(account_id_2, token_service()->GetAccounts()[1]);

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
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);

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
    std::vector<std::string> accounts_to_send = {account_id_2, account_id_1};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
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
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Checks that the reconcilor does not log out unverified accounts.
TEST_P(AccountReconcilorDiceEndpointParamTest, UnverifiedAccountNoop) {
  // Add a unverified account to the Gaia cookie.
  cookie_manager_service()->SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", true /* valid */, false /* signed_out */,
       false /* verified */});

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
  cookie_manager_service()->SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", true /* valid */, false /* signed_out */,
       false /* verified */});

  // Add a token to Chrome.
  const std::string chrome_account_id =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(chrome_account_id, "refresh_token");

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
    std::vector<std::string> accounts_to_send = {chrome_account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER,
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
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

// Tests that the Dice migration happens after a no-op reconcile.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceMigrationAfterNoop) {
  // Enable Dice migration.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDiceMigration);
  pref_service()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);

  // Chrome account is consistent with the cookie.
  const std::string account_id =
      PickAccountIdForAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor = GetMockReconcilor();
  // Dice is not enabled by default.
  EXPECT_FALSE(reconcilor->delegate_->IsAccountConsistencyEnforced());

  // No-op reconcile.
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());

  // Migration will happen on next startup.
  EXPECT_TRUE(test_signin_client()->is_ready_for_dice_migration());
}

// Tests that the Dice no migration happens if the token service is not ready.
TEST_P(AccountReconcilorDiceEndpointParamTest,
       DiceNoMigrationWhenTokensNotReady) {
  // Enable Dice migration.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDiceMigration);

  // Chrome account is consistent with the cookie.
  const std::string account_id =
      PickAccountIdForAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor = GetMockReconcilor();
  // Dice is not enabled by default.
  EXPECT_FALSE(reconcilor->delegate_->IsAccountConsistencyEnforced());

  // No-op reconcile.
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(testing::_)).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction()).Times(0);
  EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(testing::_))
      .Times(0);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());

  // Migration did not happen.
  EXPECT_FALSE(test_signin_client()->is_ready_for_dice_migration());
}

// Tests that the Dice migration does not happen after a busy reconcile.
TEST_P(AccountReconcilorDiceEndpointParamTest, DiceNoMigrationAfterReconcile) {
  // Enable Dice migration.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDiceMigration);
  pref_service()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);

  // Add a token in Chrome.
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  AccountReconcilor* reconcilor = GetMockReconcilor();

  // Dice is not enabled by default.
  EXPECT_FALSE(reconcilor->delegate_->IsAccountConsistencyEnforced());

  // Busy reconcile
  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(::testing::_))
        .Times(1);
  }
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());

  // Migration did not happen.
  EXPECT_FALSE(test_signin_client()->is_ready_for_dice_migration());
}

// Tests that secondary refresh tokens are cleared when cookie is empty during
// Dice migration.
TEST_P(AccountReconcilorDiceEndpointParamTest, MigrationClearSecondaryTokens) {
  // Enable Dice migration.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDiceMigration);
  pref_service()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);

  // Add a tokens in Chrome, signin to Sync, but no Gaia cookies.
  const std::string account_id_1 =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(account_id_1));
  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(account_id_2));

  // Reconcile should revoke the secondary account.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id_1));
  } else {
    std::vector<std::string> accounts_to_send = {account_id_1};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params)).Times(1);
    ;
  }
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id_1, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());

  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id_1));
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id_2));

  // Profile was not migrated.
  EXPECT_FALSE(test_signin_client()->is_ready_for_dice_migration());
}

// Tests that all refresh tokens are cleared when cookie is empty during
// Dice migration, if Sync is not enabled.
TEST_P(AccountReconcilorDiceEndpointParamTest, MigrationClearAllTokens) {
  // Enable Dice migration.
  SetAccountConsistency(signin::AccountConsistencyMethod::kDiceMigration);
  pref_service()->SetBoolean(prefs::kTokenServiceDiceCompatible, true);

  // Add a tokens in Chrome but no Gaia cookies.
  const std::string account_id_1 =
      PickAccountIdForAccount("12345", "user@gmail.com");
  const std::string account_id_2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id_1, "refresh_token");
  token_service()->UpdateCredentials(account_id_2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(account_id_1));
  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(account_id_2));

  // Reconcile should revoke the secondary account.
  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());

  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id_1));
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id_2));

  // Profile is now ready for migration on next startup.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_signin_client()->is_ready_for_dice_migration());
}

INSTANTIATE_TEST_CASE_P(TestDiceEndpoint,
                        AccountReconcilorDiceEndpointParamTest,
                        ::testing::ValuesIn({false, true}));

TEST_F(AccountReconcilorTest, DiceDeleteCookie) {
  SetAccountConsistency(signin::AccountConsistencyMethod::kDice);

  const std::string primary_account_id =
      account_tracker()->SeedAccountInfo("12345", "user@gmail.com");
  token_service()->UpdateCredentials(primary_account_id, "refresh_token");
  signin_manager()->SignIn("12345", "user@gmail.com", "password");
  const std::string secondary_account_id =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(secondary_account_id, "refresh_token");

  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(primary_account_id));
  ASSERT_FALSE(token_service()->RefreshTokenHasError(primary_account_id));
  ASSERT_TRUE(token_service()->RefreshTokenIsAvailable(secondary_account_id));
  ASSERT_FALSE(token_service()->RefreshTokenHasError(secondary_account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();

  // With scoped deletion, only secondary tokens are revoked.
  {
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
        reconcilor->GetScopedSyncDataDeletion();
    reconcilor->OnGaiaCookieDeletedByUserAction();
    EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(primary_account_id));
    EXPECT_FALSE(token_service()->RefreshTokenHasError(primary_account_id));
    EXPECT_FALSE(
        token_service()->RefreshTokenIsAvailable(secondary_account_id));
  }

  token_service()->UpdateCredentials(secondary_account_id, "refresh_token");
  reconcilor->OnGaiaCookieDeletedByUserAction();

  // Without scoped deletion, the primary token is also invalidated.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(primary_account_id));
  EXPECT_TRUE(token_service()->RefreshTokenHasError(primary_account_id));
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            token_service()
                ->GetAuthError(primary_account_id)
                .GetInvalidGaiaCredentialsReason());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(secondary_account_id));

  // If the primary account has an error, always revoke it.
  token_service()->UpdateCredentials(primary_account_id, "refresh_token");
  ASSERT_FALSE(token_service()->RefreshTokenHasError(primary_account_id));
  token_service()->UpdateAuthErrorForTesting(
      primary_account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  {
    std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
        reconcilor->GetScopedSyncDataDeletion();
    reconcilor->OnGaiaCookieDeletedByUserAction();
    EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                  CREDENTIALS_REJECTED_BY_CLIENT,
              token_service()
                  ->GetAuthError(primary_account_id)
                  .GetInvalidGaiaCredentialsReason());
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// clang-format off
const std::vector<AccountReconcilorTestTableParam> kMirrorParams = {
    // This table encodes the initial state and expectations of a reconcile.
    // The syntax is:
    // - Tokens:
    //   A, B, C: Accounts for which we have a token in Chrome.
    //   *: The next account is the main Chrome account (i.e. in SigninManager).
    //   x: The next account has a token error.
    // - Cookies:
    //   A, B, C: Accounts in the Gaia cookie (returned by ListAccounts).
    //   x: The next cookie is marked "invalid".
    // - First Run: true if this is the first reconcile (i.e. Chrome startup).
    //   M: Multilogin.
    // -----------------
    // -------------------------------------------------------------------------
    // Tokens | Cookies |          First Run        | Gaia calls | Tokens after | Cookies after
    // -------------------------------------------------------------------------

    // First reconcile (Chrome restart): Rebuild the Gaia cookie to match the
    // tokens. Make the Sync account the default account in the Gaia cookie.
    // Sync enabled.
    {  "*AB",   "AB",     IsFirstReconcile::kBoth,       "",          "*AB",         "AB"},
    {  "*AB",   "BA",     IsFirstReconcile::kBoth,       "M",         "*AB",         "AB"},
    {  "*AB",   "A",      IsFirstReconcile::kBoth,       "M",         "*AB",         "AB"},
    {  "*AB",   "B",      IsFirstReconcile::kBoth,       "M",         "*AB",         "AB"},
    {  "*AB",   "",       IsFirstReconcile::kBoth,       "M",         "*AB",         "AB"},
    // Sync enabled, token error on primary.
    // Sync enabled, token error on secondary.
    {  "*AxB",  "AB",     IsFirstReconcile::kBoth,       "M",         "*AxB",         "A"},
    {  "*AxB",  "BA",     IsFirstReconcile::kBoth,       "M",         "*AxB",         "A"},
    {  "*AxB",  "A",      IsFirstReconcile::kBoth,       "",          "*AxB",         ""},
    {  "*AxB",  "B",      IsFirstReconcile::kBoth,       "M",         "*AxB",         "A"},
    {  "*AxB",  "",       IsFirstReconcile::kBoth,       "M",         "*AxB",         "A"},

    // Cookies can be refreshed in pace, without logout.
    {  "*AB",   "xBxA",   IsFirstReconcile::kBoth,       "M",         "*AB",         "AB"},

    // Check that unknown Gaia accounts are signed out.
    {  "*A",    "AB",     IsFirstReconcile::kBoth,       "M",          "*A",          "A"},
    // Check that the previous case is idempotent..
    {  "*A",    "A",      IsFirstReconcile::kBoth,       "",           "*A",          ""},
};
// clang-format on

// Parameterized version of AccountReconcilorTest that tests Mirror
// implementation with Multilogin endpoint.
class AccountReconcilorTestMirrorMultilogin
    : public AccountReconcilorTestTable {
 public:
  AccountReconcilorTestMirrorMultilogin() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTestMirrorMultilogin);
};

// Checks one row of the kDiceParams table above.
TEST_P(AccountReconcilorTestMirrorMultilogin, TableRowTest) {
  // Enable Mirror.
  SetAccountConsistency(signin::AccountConsistencyMethod::kMirror);
  scoped_feature_list_.InitAndEnableFeature(kUseMultiloginEndpoint);

  // Setup tokens.
  std::vector<Token> tokens_before_reconcile =
      ParseTokenString(GetParam().tokens);
  for (const Token& token : tokens_before_reconcile) {
    std::string account_id =
        PickAccountIdForAccount(token.gaia_id, token.email);
    if (token.is_authenticated)
      ConnectProfileToAccount(token.gaia_id, token.email);
    else
      token_service()->UpdateCredentials(account_id, "refresh_token");
    if (token.has_error) {
      token_service()->UpdateAuthErrorForTesting(
          account_id, GoogleServiceAuthError(
                          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    }
  }
  VerifyCurrentTokens(tokens_before_reconcile);

  // Setup cookies.
  std::vector<Cookie> cookies = ParseCookieString(GetParam().cookies);
  ConfigureCookieManagerService(cookies);

  // Call list accounts now so that the next call completes synchronously.
  cookie_manager_service()->ListAccounts(nullptr, nullptr, "foo");
  base::RunLoop().RunUntilIdle();

  // Setup expectations.
  testing::InSequence mock_sequence;
  bool logout_action = false;
  for (int i = 0; GetParam().gaia_api_calls[i] != '\0'; ++i) {
    if (GetParam().gaia_api_calls[i] == 'M') {
      std::vector<std::string> accounts_to_send;
      for (int i = 0; GetParam().cookies_after_reconcile[i] != '\0'; ++i) {
        std::string account_to_send =
            std::string(1, GetParam().cookies_after_reconcile[i]);
        accounts_to_send.push_back(PickAccountIdForAccount(
            account_to_send,
            accounts_[GetParam().cookies_after_reconcile[i]].email));
      }
      const signin::MultiloginParameters params(
          signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
          accounts_to_send);
      EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params))
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

  SimulateSetAccountsInCookieCompleted(reconcilor,
                                       GoogleServiceAuthError::AuthErrorNone());

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

INSTANTIATE_TEST_CASE_P(
    DiceTableMirrorMultilogin,
    AccountReconcilorTestMirrorMultilogin,
    ::testing::ValuesIn(GenerateTestCasesFromParams(kMirrorParams)));

// Tests that reconcile cannot start before the tokens are loaded, and is
// automatically started when tokens are loaded.
TEST_P(AccountReconcilorMirrorEndpointParamTest, TokensNotLoaded) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseNoAccounts();
  token_service()->set_all_credentials_loaded_for_testing(false);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

#if !defined(OS_CHROMEOS)
  // No reconcile when tokens are not loaded, except on ChromeOS where reconcile
  // can start as long as the token service is not empty.
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  // When tokens are loaded, reconcile starts automatically.
  token_service()->LoadCredentials(account_id);
#endif

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<std::string> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  base::RunLoop().RunUntilIdle();
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, GetAccountsFromCookieSuccess) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", false /* valid */, false /* signed_out */,
       true /* verified */});

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<std::string> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());

  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_TRUE(cookie_manager_service()->ListAccounts(
      &accounts, &signed_out_accounts, GaiaConstants::kChromeSource));
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ(account_id, accounts[0].id);
  ASSERT_EQ(0u, signed_out_accounts.size());
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, GetAccountsFromCookieFailure) {
  ;
  ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseWebLoginRequired();

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor->GetState());
  reconcilor->StartReconcile();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_RUNNING, reconcilor->GetState());
  base::RunLoop().RunUntilIdle();

  std::vector<gaia::ListedAccount> accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(
      &accounts, &signed_out_accounts, GaiaConstants::kChromeSource));
  ASSERT_EQ(0u, accounts.size());
  ASSERT_EQ(0u, signed_out_accounts.size());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_ERROR, reconcilor->GetState());
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileNoop) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectTotalCount(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
  }
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileCookiesDisabled) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  std::vector<gaia::ListedAccount> accounts;
  // This will be the first call to ListAccounts.
  ASSERT_FALSE(cookie_manager_service()->ListAccounts(
      &accounts, nullptr, GaiaConstants::kChromeSource));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileContentSettings) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

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

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileContentSettingsGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GaiaUrls::GetInstance()->gaia_url()));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileContentSettingsNonGaiaUrl) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileContentSettingsInvalidPattern) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  builder->Invalid();

  SimulateCookieContentSettingsChanged(reconcilor, builder->Build());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

// This test is needed until chrome changes to use gaia obfuscated id.
// The signin manager and token service use the gaia "email" property, which
// preserves dots in usernames and preserves case. gaia::ParseListAccountsData()
// however uses gaia "displayEmail" which does not preserve case, and then
// passes the string through gaia::CanonicalizeEmail() which removes dots.  This
// tests makes sure that an email like "Dot.S@hmail.com", as seen by the
// token service, will be considered the same as "dots@gmail.com" as returned
// by gaia::ParseListAccountsData().
TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileNoopWithDots) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    return;
  }

  const std::string account_id =
      ConnectProfileToAccount("12345", "Dot.S@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("dot.s@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
  }
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileNoopMultiple) {
  const std::string account_id =
      ConnectProfileToAccount("67890", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("12345", "other@gmail.com");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "67890", "other@gmail.com", "12345");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectTotalCount(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
  }
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileAddToCookie) {
  const std::string account_id =
      ConnectProfileToAccount("67890", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "67890");

  const std::string account_id2 =
      PickAccountIdForAccount("12345", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  } else {
    std::vector<std::string> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  }

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Success"] = 1;
  EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
                  "Signin.Reconciler.Duration.Success"),
              testing::ContainerEq(expected_counts));
}

TEST_F(AccountReconcilorTest, AuthErrorTriggersListAccount) {
  class TestGaiaCookieObserver : public GaiaCookieManagerService::Observer {
   public:
    void OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const std::vector<gaia::ListedAccount>& signed_out_accounts,
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
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  TestGaiaCookieObserver observer;
  cookie_manager_service()->AddObserver(&observer);
  AccountReconcilor* reconcilor = GetMockReconcilor();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  if (account_consistency == signin::AccountConsistencyMethod::kDice) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction())
        .Times(1);
  }

  // Set an authentication error.
  ASSERT_FALSE(observer.cookies_updated_);
  token_service()->UpdateAuthErrorForTesting(
      account_id, GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                          CREDENTIALS_REJECTED_BY_SERVER));
  base::RunLoop().RunUntilIdle();

  // Check that a call to ListAccount was triggered.
  EXPECT_TRUE(observer.cookies_updated_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  cookie_manager_service()->RemoveObserver(&observer);
}

#if !defined(OS_CHROMEOS)
// This test does not run on ChromeOS because it calls
// FakeSigninManagerForTesting::SignOut() which doesn't exist for ChromeOS.

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       SignoutAfterErrorDoesNotRecordUma) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");

  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  } else {
    std::vector<std::string> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(reconcilor, account_id2, error);
  } else {
    SimulateSetAccountsInCookieCompleted(reconcilor, error);
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Reconciler.Duration.Failure"] = 1;
  if (!IsMultiloginEnabled()) {
    EXPECT_THAT(histogram_tester()->GetTotalCountsForPrefix(
                    "Signin.Reconciler.Duration.Failure"),
                testing::ContainerEq(expected_counts));
  }
}

#endif  // !defined(OS_CHROMEOS)

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileRemoveFromCookie) {
  const std::string account_id =
      ConnectProfileToAccount("67890", "user@gmail.com");
  token_service()->UpdateCredentials(account_id, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "67890", "other@gmail.com", "12345");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<std::string> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 1, 1);
  }
}

// Check that reconcile is aborted if there is token error on primary account.
TEST_P(AccountReconcilorMirrorEndpointParamTest, TokenErrorOnPrimary) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  token_service()->UpdateAuthErrorForTesting(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "12345", "other@gmail.com", "67890");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       StartReconcileAddToCookieTwice) {
  const std::string account_id =
      ConnectProfileToAccount("67890", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("12345", "other@gmail.com");
  const std::string account_id3 =
      PickAccountIdForAccount("34567", "third@gmail.com");

  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "67890");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id3));
  } else {
    std::vector<std::string> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  }

  // Do another pass after I've added a third account to the token service
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "user@gmail.com", "67890", "other@gmail.com", "12345");
  cookie_manager_service()->set_list_accounts_stale_for_testing(true);

  // This will cause the reconcilor to fire.
  token_service()->UpdateCredentials(account_id3, "refresh_token");
  if (IsMultiloginEnabled()) {
    std::vector<std::string> accounts_to_send = {account_id, account_id2,
                                                 account_id3};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id3, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::ACCOUNTS_SAME, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
        signin_metrics::ACCOUNTS_SAME, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.SubsequentRun", 1, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.SubsequentRun", 0, 1);
  }
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileBadPrimary) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");

  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseTwoAccounts(
      "other@gmail.com", "67890", "user@gmail.com", "12345");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  } else {
    std::vector<std::string> accounts_to_send = {account_id, account_id2};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
    ASSERT_TRUE(reconcilor->is_reconcile_started_);
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
        signin_metrics::COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
    histogram_tester()->ExpectUniqueSample(
        "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  }
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, StartReconcileOnlyOnce) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, Lock) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  EXPECT_EQ(0, reconcilor->account_reconcilor_lock_count_);

  class TestAccountReconcilorObserver : public AccountReconcilor::Observer {
   public:
    void OnStartReconcile() override { ++started_count_; }
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
  SetAccountConsistency(GetParam());
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");
  cookie_manager_service()->SetListAccountsResponseWithParams(
      {{"user@gmail.com", "12345", false /* valid */, false /* signed_out */,
        true /* verified */},
       {"other@gmail.com", "67890", true /* valid */, false /* signed_out */,
        true /* verified */}});

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, account_id,
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest,
       AddAccountToCookieCompletedWithBogusAccount) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccountWithParams(
      {"user@gmail.com", "12345", false /* valid */, false /* signed_out */,
       true /* verified */});

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id));
  } else {
    std::vector<std::string> accounts_to_send = {account_id};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, NoLoopWithBadPrimary) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id2));
  } else {
    std::vector<std::string> accounts_to_send = {account_id1, account_id2};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }
  // The primary account is in auth error, so it is not in the cookie.
  cookie_manager_service()->SetListAccountsResponseOneAccountWithParams(
      {"other@gmail.com", "67890", false /* valid */, false /* signed_out */,
       true /* verified */});

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // The primary cannot be added to cookie, so it fails.
  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(reconcilor, account_id1, error);
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id2, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(reconcilor, error);
  }
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_NE(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());

  // Now that we've tried once, the token service knows that the primary
  // account has an auth error.
  token_service()->UpdateAuthErrorForTesting(account_id1, error);

  // A second attempt to reconcile should be a noop.
  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  testing::Mock::VerifyAndClearExpectations(GetMockReconcilor());
}

TEST_P(AccountReconcilorMirrorEndpointParamTest, WontMergeAccountsWithError) {
  // Connect profile to a primary account and then add a secondary account.
  const std::string account_id1 =
      ConnectProfileToAccount("12345", "user@gmail.com");
  const std::string account_id2 =
      PickAccountIdForAccount("67890", "other@gmail.com");
  token_service()->UpdateCredentials(account_id2, "refresh_token");

  // Mark the secondary account in auth error state.
  token_service()->UpdateAuthErrorForTesting(
      account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The cookie starts empty.
  cookie_manager_service()->SetListAccountsResponseNoAccounts();

  // Since the cookie jar starts empty, the reconcilor should attempt to merge
  // accounts into it.  However, it should only try accounts not in auth
  // error state.
  if (!IsMultiloginEnabled()) {
    EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(account_id1));
  } else {
    std::vector<std::string> accounts_to_send = {account_id1};
    const signin::MultiloginParameters params(
        signin::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        accounts_to_send);
    EXPECT_CALL(*GetMockReconcilor(), PerformSetCookiesAction(params));
  }

  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  if (!IsMultiloginEnabled()) {
    SimulateAddAccountToCookieCompleted(
        reconcilor, account_id1, GoogleServiceAuthError::AuthErrorNone());
  } else {
    SimulateSetAccountsInCookieCompleted(
        reconcilor, GoogleServiceAuthError::AuthErrorNone());
  }
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_EQ(GoogleServiceAuthError::State::NONE,
            reconcilor->error_during_last_reconcile_.state());
}

// Test that delegate timeout is called when the delegate offers a valid
// timeout.
TEST_F(AccountReconcilorTest, DelegateTimeoutIsCalled) {
  const std::string account_id =
      ConnectProfileToAccount("12345", "user@gmail.com");
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
TEST_P(AccountReconcilorMirrorEndpointParamTest, DelegateTimeoutIsNotCalled) {
  ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
  AccountReconcilor* reconcilor = GetMockReconcilor();
  ASSERT_TRUE(reconcilor);
  auto timer0 = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer = timer0.get();
  reconcilor->set_timer_for_testing(std::move(timer0));

  reconcilor->StartReconcile();
  EXPECT_TRUE(reconcilor->is_reconcile_started_);
  EXPECT_FALSE(timer->IsRunning());
}

INSTANTIATE_TEST_CASE_P(TestMirrorEndpoint,
                        AccountReconcilorMirrorEndpointParamTest,
                        ::testing::ValuesIn({false, true}));

TEST_F(AccountReconcilorTest, DelegateTimeoutIsNotCalledIfTimeoutIsNotReached) {
  ConnectProfileToAccount("12345", "user@gmail.com");
  cookie_manager_service()->SetListAccountsResponseOneAccount("user@gmail.com",
                                                              "12345");
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
