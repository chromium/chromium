// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace network {
class TestURLLoaderFactory;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace account_manager {
class AccountManagerFacade;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

class GoogleServiceAuthError;

// Test-related utilities that don't fit in either IdentityTestEnvironment or
// IdentityManager itself. NOTE: Using these utilities directly is discouraged,
// but sometimes necessary during conversion. Use IdentityTestEnvironment if
// possible. These utilities should be used directly only if the production code
// is using IdentityManager, but it is not yet feasible to convert the test code
// to use IdentityTestEnvironment. Any such usage should only be temporary,
// i.e., should be followed as quickly as possible by conversion of the test
// code to use IdentityTestEnvironment.
namespace signin {

struct CookieParamsForTest {
  std::string email;
  std::string gaia_id;
  bool signed_out = false;
};

class IdentityManager;

// Blocks until `LoadCredentials` is complete and `OnRefreshTokensLoaded` is
// invoked.
void WaitForRefreshTokensLoaded(IdentityManager* identity_manager);

// Returns the current exact consent level for the primary account, or
// `std::nullopt` if there is no primary account set.
std::optional<signin::ConsentLevel> GetPrimaryAccountConsentLevel(
    IdentityManager* identity_manager);

// Sets the primary account (which must not already be set) to the given email
// address with corresponding consent level, generating a GAIA ID that
// corresponds uniquely to that email address. On non-ChromeOS, results in the
// firing of the IdentityManager and PrimaryAccountManager callbacks for signin
// success. Blocks until the primary account is set. Returns the CoreAccountInfo
// of the newly-set account.
//
// See `MakePrimaryAccountAvailable()` for a method that will return the
// extended `AccountInfo` instead (and consequently always sets the refresh
// token). If a more specific configuration is needed, see
// `MakeAccountAvailable(IdentityManager*, AccountAvailabilityOptions)`.
//
// NOTE: See disclaimer at top of file re: direct usage.
CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email,
                                  ConsentLevel consent_level);

// When this is set for the given `identity_manager`, access token requests
// will be automatically granted with an access token value of "access_token".
void SetAutomaticIssueOfAccessTokens(IdentityManager* identity_manager,
                                     bool grant);

// Sets a refresh token for the primary account (which must already be set).
// Blocks until the refresh token is set. If |token_value| is empty a default
// value will be used instead.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager,
    const std::string& token_value = std::string());

// Sets a special invalid refresh token for the primary account (which must
// already be set). Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetInvalidRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager,
    signin_metrics::SourceForRefreshTokenOperation source =
        signin_metrics::SourceForRefreshTokenOperation::kUnknown);

// Removes any refresh token for the primary account, if present. Blocks until
// the refresh token is removed.
// NOTE: See disclaimer at top of file re: direct usage.
void RemoveRefreshTokenForPrimaryAccount(IdentityManager* identity_manager);

// Makes the primary account (which must not already be set) available for the
// given email address, generating a GAIA ID and refresh token that correspond
// uniquely to that email address. On non-ChromeOS, results in the firing of the
// IdentityManager and PrimaryAccountManager callbacks for signin success.
// Blocks until the primary account is available. Returns the AccountInfo of the
// newly-available account.
//
// If a more specific configuration is needed, see
// `MakeAccountAvailable(IdentityManager*, AccountAvailabilityOptions)`.
//
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email,
                                        ConsentLevel consent_level);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Revokes sync consent from the primary account: the primary account is left
// at ConsentLevel::kSignin.
// NOTE: See disclaimer at top of file re: direct usage.
// NOTE:`ConsentLevel::kSync` is deprecated, see the `ConsentLevel`
// documentation.
// TODO(crbug.com/40067058): remove this function once `ConsentLevel::kSync` is
// removed.
void RevokeSyncConsent(IdentityManager* identity_manager);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Clears the primary account, removes all accounts and revokes the sync
// consent (if applicable). Blocks until the primary account is cleared.
// NOTE: See disclaimer at top of file re: direct usage.
void ClearPrimaryAccount(IdentityManager* identity_manager);

// Waits until the primary account id at consent_level to be equal to
// |account_id|.
//
// Note: Passing an empty |account_id| will make this function wait until
// the primary account id is cleared at the |consent_level| (calling
// identity_manager->HasPrimaryAccount(consent_level) will return false)
void WaitForPrimaryAccount(IdentityManager* identity_manager,
                           ConsentLevel consent_level,
                           const CoreAccountId& account_id);

// Configuration for the `signin::MakeAccountAvailable()` function. See
// `AccountAvailabilityOptionsBuilder` for a helper to set the fields correctly.
struct AccountAvailabilityOptions {
  // Email for the account to be created. MUST be non-empty.
  const std::string email;

  // Gaia ID for the account to be created. if empty, the existing ID should be
  // kept for known accounts, or a new one generated from the email for totally
  // new accounts.
  const std::string gaia_id;

  // If present, the account to be created should be set as primary at
  // `consent_level`.
  const std::optional<ConsentLevel> consent_level = std::nullopt;

  // If present, a refresh token will be set for the account to be created. Can
  // be an empty string (this is the default), in this case the token's value
  // will be auto-generated. If non-empty, the value of `refresh_token` will be
  // used as the token.
  const std::optional<std::string> refresh_token = std::string();

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // If non-empty, a refresh token will be bound to device with this key.
  const std::vector<uint8_t> wrapped_binding_key;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  // If non-null, the account to be created will be marked as present in the
  // Gaia cookie, by using `url_loader_factory_for_cookies` to mock the
  // LIST_ACCOUNTS response.
  const raw_ptr<network::TestURLLoaderFactory> url_loader_factory_for_cookies =
      nullptr;

  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;

  explicit AccountAvailabilityOptions(std::string_view email);
  ~AccountAvailabilityOptions();

 private:
  friend class AccountAvailabilityOptionsBuilder;

  // For complex options, prefer using `AccountAvailabilityOptionsBuilder`.
  AccountAvailabilityOptions(
      std::string_view email,
      std::string_view gaia_id,
      std::optional<ConsentLevel> consent_level,
      std::optional<std::string> refresh_token,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      const std::vector<uint8_t>& wrapped_binding_key,
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      raw_ptr<network::TestURLLoaderFactory> url_loader_factory_for_cookies,
      signin_metrics::AccessPoint access_point);
};

class AccountAvailabilityOptionsBuilder {
 public:
  // Note: `url_loader_factory` MUST be non-null to be able to use
  // `WithCookies()`.
  explicit AccountAvailabilityOptionsBuilder(
      network::TestURLLoaderFactory* url_loader_factory = nullptr);

  AccountAvailabilityOptionsBuilder(
      const AccountAvailabilityOptionsBuilder& other);
  AccountAvailabilityOptionsBuilder(
      AccountAvailabilityOptionsBuilder&& other) noexcept;
  AccountAvailabilityOptionsBuilder& operator=(
      const AccountAvailabilityOptionsBuilder& other);
  AccountAvailabilityOptionsBuilder& operator=(
      AccountAvailabilityOptionsBuilder&& other) noexcept;

  ~AccountAvailabilityOptionsBuilder();

  // Request the new account to be set as primary account at `consent_level`.
  AccountAvailabilityOptionsBuilder& AsPrimary(ConsentLevel);

  // Provide a custom `gaia_id` to use for the new account.
  AccountAvailabilityOptionsBuilder& WithGaiaId(std::string_view gaia_id);

  // Whether to add the new account to the Gaia cookie. See
  // `signin::AddCookieAccount()` for more context.
  AccountAvailabilityOptionsBuilder& WithCookie(bool with_cookie = true);

  // Request a specific refresh token to be set for the new account. If
  // `refresh_token` is empty, the token will be auto-generated based on the
  // account data.
  // NOTE: by default, the builder will auto-generate a refresh token. Call
  // `WithoutRefreshToken()` to avoid it.
  AccountAvailabilityOptionsBuilder& WithRefreshToken(
      std::string_view refresh_token);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // Request a refresh token that is bound to a `wrapped_binding_key`.
  // `WithoutRefreshToken()` must not be called if this option is set.
  AccountAvailabilityOptionsBuilder& WithRefreshTokenBindingKey(
      const std::vector<uint8_t>& wrapped_binding_key);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  // Request that we should not attempt to set a refresh token for the account.
  AccountAvailabilityOptionsBuilder& WithoutRefreshToken();

  AccountAvailabilityOptionsBuilder& WithAccessPoint(
      signin_metrics::AccessPoint access_point);

  AccountAvailabilityOptions Build(std::string_view email);

 private:
  raw_ptr<network::TestURLLoaderFactory> url_loader_factory_for_cookies_ =
      nullptr;

  std::string gaia_id_;
  std::optional<ConsentLevel> primary_account_consent_level_;
  std::optional<std::string> refresh_token_ = std::string();
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::vector<uint8_t> wrapped_binding_key_;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  bool with_cookie_ = false;
  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
};

// Sets up an account identified by `email` according to options provided. See
// `AccountAvailabilityOptions` documentation for more details on these.
// Blocks until the account is available. Returns the AccountInfo of the
// newly-available account.
//
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const AccountAvailabilityOptions& options);

// Makes an account available for the given email address, generating a GAIA ID
// and refresh token that correspond uniquely to that email address. Blocks
// until the account is available. Returns the AccountInfo of the
// newly-available account.
//
// If a more specific configuration is needed, see
// `MakeAccountAvailable(IdentityManager*, AccountAvailabilityOptions)`.
//
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email);

// Sets a refresh token for the given account (which must already be available).
// Blocks until the refresh token is set. If |token_value| is empty a default
// value will be used instead.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const std::string& token_value = std::string()
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        ,
    const std::vector<uint8_t>& wrapped_binding_key = std::vector<uint8_t>()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
);

// Sets a special invalid refresh token for the given account (which must
// already be available). Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetInvalidRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    signin_metrics::SourceForRefreshTokenOperation source =
        signin_metrics::SourceForRefreshTokenOperation::kUnknown);

// Removes any refresh token that is present for the given account. Blocks until
// the refresh token is removed. Is a no-op if no refresh token is present for
// the given account.
// NOTE: See disclaimer at top of file re: direct usage.
void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                  const CoreAccountId& account_id);

// Adds the given account into the Gaia cookie, appending it after the existing
// accounts. Blocks until the accounts have been set.
// `test_url_loader_factory` is used to set a fake ListAccounts response
// containing the provided `cookie_accounts`, which are then put into
// the Gaia cookie.
// NOTE: See disclaimer at top of file re: direct usage.
void AddCookieAccount(IdentityManager* identity_manager,
                      network::TestURLLoaderFactory* test_url_loader_factory,
                      const CookieParamsForTest& cookie_account_to_add);

// Puts the given accounts into the Gaia cookie, replacing any previous
// accounts. Blocks until the accounts have been set.
// |test_url_loader_factory| is used to set a fake ListAccounts response
// containing the provided |cookie_accounts|, which are then put into
// the Gaia cookie.
// NOTE: See disclaimer at top of file re: direct usage.
void SetCookieAccounts(IdentityManager* identity_manager,
                       network::TestURLLoaderFactory* test_url_loader_factory,
                       const std::vector<CookieParamsForTest>& cookie_accounts);

// Triggers a fake /ListAccount call with the current accounts in the cookie
// jar. It will notify all observers.
void TriggerListAccount(IdentityManager* identity_manager,
                        network::TestURLLoaderFactory* test_url_loader_factory);

// Updates the info for |account_info.account_id|, which must be a known
// account.
void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                 AccountInfo account_info);

void SimulateAccountImageFetch(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& image_url_with_size,
                               const gfx::Image& image);

// Sets whether the list of accounts in Gaia cookie jar is fresh and does not
// need to be updated.
void SetFreshnessOfAccountsInGaiaCookie(IdentityManager* identity_manager,
                                        bool accounts_are_fresh);

std::string GetTestGaiaIdForEmail(const std::string& email);

// Returns a new `AccountInfo` object that completes the info from
// `base_account_info` with generated extended info (the one normally obtained
// from Gaia's /UserInfo endpoint), based on `given_name`.
//
// The returned account info will be valid per `AccountInfo::IsValid()`.
AccountInfo WithGeneratedUserInfo(const AccountInfo& base_account_info,
                                  std::string_view given_name);

// Updates the persistent auth error set on |account_id| which must be a known
// account, i.e., an account with a refresh token.
void UpdatePersistentErrorOfRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error);

// Waits until `GetErrorStateOfRefreshTokenForAccount` result for `account_id`
// satisfies the passed `predicate`. If calling the predicate on the current
// error state returns true, this method returns immediately.
void WaitForErrorStateOfRefreshTokenUpdatedForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate);

// Disables internal retries of failed access token fetches.
void DisableAccessTokenFetchRetries(IdentityManager* identity_manager);

#if BUILDFLAG(IS_ANDROID)
// Stubs AccountManagerFacade, which requires special initialization of the java
// subsystems. By default sets up a mock with no real method implementation
// using Mockito. If `useFakeImpl` is `true` uses FakeAccountManagerFacade
// instead which has fake method implementations.
void SetUpMockAccountManagerFacade(bool useFakeImpl = false);
#endif

// Cancels all ongoing operations related to the accounts in the Gaia cookie.
void CancelAllOngoingGaiaCookieOperations(IdentityManager* identity_manager);

// Simulate account fetching using AccountTrackerService without sending
// network requests.
void SimulateSuccessfulFetchOfAccountInfo(IdentityManager* identity_manager,
                                          const CoreAccountId& account_id,
                                          const std::string& email,
                                          const std::string& gaia,
                                          const std::string& hosted_domain,
                                          const std::string& full_name,
                                          const std::string& given_name,
                                          const std::string& locale,
                                          const std::string& picture_url);

#if BUILDFLAG(IS_CHROMEOS)
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    IdentityManager* identity_manager);
#endif
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_
