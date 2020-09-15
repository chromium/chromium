// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_

#include <string>

#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace network {
class TestURLLoaderFactory;
}

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

// Controls whether to keep or remove accounts when clearing the primary
// account.
enum class ClearPrimaryAccountPolicy {
  // Use the default internal policy.
  DEFAULT,
  // Explicitly keep all accounts.
  KEEP_ALL_ACCOUNTS,
  // Explicitly remove all accounts.
  REMOVE_ALL_ACCOUNTS
};

struct CookieParamsForTest {
  std::string email;
  std::string gaia_id;
};

class IdentityManager;

// Sets the primary account (which must not already be set) to the given email
// address, generating a GAIA ID that corresponds uniquely to that email
// address. On non-ChromeOS, results in the firing of the IdentityManager and
// PrimaryAccountManager callbacks for signin success. Blocks until the primary
// account is set. Returns the CoreAccountInfo of the newly-set account.
// NOTE: See disclaimer at top of file re: direct usage.
CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email);

// As above, but adds an "unconsented" primary account. See ./README.md for
// the distinction between primary and unconsented primary accounts.
CoreAccountInfo SetUnconsentedPrimaryAccount(IdentityManager* identity_manager,
                                             const std::string& email);

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
void SetInvalidRefreshTokenForPrimaryAccount(IdentityManager* identity_manager);

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
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email);

// Clears the primary account if present, with |policy| used to determine
// whether to keep or remove all accounts. On non-ChromeOS, results in the
// firing of the IdentityManager and PrimaryAccountManager callbacks for
// signout. Blocks until the primary account is cleared.
// NOTE: See disclaimer at top of file re: direct usage.
void ClearPrimaryAccount(
    IdentityManager* identity_manager,
    ClearPrimaryAccountPolicy policy = ClearPrimaryAccountPolicy::DEFAULT);

// Makes an account available for the given email address, generating a GAIA ID
// and refresh token that correspond uniquely to that email address. Blocks
// until the account is available. Returns the AccountInfo of the
// newly-available account.
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email);

// Combination of MakeAccountAvailable() and SetCookieAccounts() for a single
// account. It makes an account available for the given email address, and GAIA
// ID, setting the cookies and the refresh token that correspond uniquely to
// that email address. Blocks until the account is available. Returns the
// AccountInfo of the newly-available account.
// NOTE: See disclaimer at top of file re: direct usage.
AccountInfo MakeAccountAvailableWithCookies(
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email,
    const std::string& gaia_id);

// Sets a refresh token for the given account (which must already be available).
// Blocks until the refresh token is set. If |token_value| is empty a default
// value will be used instead.
// NOTE: See disclaimer at top of file re: direct usage.
void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& token_value = std::string());

// Sets a special invalid refresh token for the given account (which must
// already be available). Blocks until the refresh token is set.
// NOTE: See disclaimer at top of file re: direct usage.
void SetInvalidRefreshTokenForAccount(IdentityManager* identity_manager,
                                      const CoreAccountId& account_id);

// Removes any refresh token that is present for the given account. Blocks until
// the refresh token is removed. Is a no-op if no refresh token is present for
// the given account.
// NOTE: See disclaimer at top of file re: direct usage.
void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                  const CoreAccountId& account_id);

// Puts the given accounts into the Gaia cookie, replacing any previous
// accounts. Blocks until the accounts have been set.
// |test_url_loader_factory| is used to set a fake ListAccounts response
// containing the provided |cookie_accounts|, which are then put into
// the Gaia cookie.
// NOTE: See disclaimer at top of file re: direct usage.
void SetCookieAccounts(IdentityManager* identity_manager,
                       network::TestURLLoaderFactory* test_url_loader_factory,
                       const std::vector<CookieParamsForTest>& cookie_accounts);

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

// Updates the persistent auth error set on |account_id| which must be a known
// account, i.e., an account with a refresh token.
void UpdatePersistentErrorOfRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error);

// Disables internal retries of failed access token fetches.
void DisableAccessTokenFetchRetries(IdentityManager* identity_manager);

#if defined(OS_ANDROID)
// Disables interaction with system accounts, which requires special
// initialization of the java subsystems (AccountManagerFacade).
void DisableInteractionWithSystemAccounts();
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
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_UTILS_H_
