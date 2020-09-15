// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_utils.h"

#include <vector>

#include "base/guid.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"

#if defined(OS_CHROMEOS)
#include "chromeos/components/account_manager/account_manager.h"
#endif

#if defined(OS_ANDROID)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#endif

namespace signin {

namespace {

void WaitForLoadCredentialsToComplete(IdentityManager* identity_manager) {
  base::RunLoop run_loop;
  TestIdentityManagerObserver load_credentials_observer(identity_manager);
  load_credentials_observer.SetOnRefreshTokensLoadedCallback(
      run_loop.QuitClosure());

  if (identity_manager->AreRefreshTokensLoaded())
    return;

  // Do NOT explicitly load credentials here:
  // 1. It is not re-entrant and will DCHECK fail.
  // 2. It should have been called by IdentityManager during its initialization.

  run_loop.Run();
}

// Helper function that updates the refresh token for |account_id| to
// |new_token|. Before updating the refresh token, blocks until refresh tokens
// are loaded. After updating the token, blocks until the update is processed by
// |identity_manager|.
void UpdateRefreshTokenForAccount(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
#if defined(OS_CHROMEOS)
    chromeos::AccountManager* account_manager,
#endif
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const std::string& new_token) {
  DCHECK_EQ(account_tracker_service->GetAccountInfo(account_id).account_id,
            account_id)
      << "To set the refresh token for an unknown account, use "
         "MakeAccountAvailable()";

  // Ensure that refresh tokens are loaded; some platforms enforce the invariant
  // that refresh token mutation cannot occur until refresh tokens are loaded,
  // and it is desired to eventually enforce that invariant across all
  // platforms.
  WaitForLoadCredentialsToComplete(identity_manager);

  base::RunLoop run_loop;
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenUpdatedCallback(
      run_loop.QuitClosure());

#if defined(OS_CHROMEOS)
  const AccountInfo& account_info =
      account_tracker_service->GetAccountInfo(account_id);

  DCHECK(account_manager);
  account_manager->UpsertAccount(
      chromeos::AccountManager::AccountKey{
          account_info.gaia,
          chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA},
      account_info.email, new_token);
#else
  token_service->UpdateCredentials(account_id, new_token);
#endif

  run_loop.Run();
}

// Ensures that an account for |email| exists in the AccountTrackerService,
// seeding it if necessary. Returns AccountInfo for the account.
AccountInfo EnsureAccountExists(AccountTrackerService* account_tracker_service,
                                const std::string& email) {
  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  if (account_info.account_id.empty()) {
    std::string gaia_id = GetTestGaiaIdForEmail(email);
    account_tracker_service->SeedAccountInfo(gaia_id, email);
    account_info = account_tracker_service->FindAccountInfoByEmail(email);
    DCHECK(!account_info.account_id.empty());
  }
  return account_info;
}

}  // namespace

CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email) {
  DCHECK(!identity_manager->HasPrimaryAccount());
  PrimaryAccountManager* primary_account_manager =
      identity_manager->GetPrimaryAccountManager();
  DCHECK(!primary_account_manager->IsAuthenticated());

  AccountInfo account_info =
      EnsureAccountExists(identity_manager->GetAccountTrackerService(), email);
  DCHECK(!account_info.gaia.empty());

  primary_account_manager->SignIn(email);

  DCHECK(primary_account_manager->IsAuthenticated());
  DCHECK(identity_manager->HasPrimaryAccount());
  return identity_manager->GetPrimaryAccountInfo();
}

CoreAccountInfo SetUnconsentedPrimaryAccount(IdentityManager* identity_manager,
                                             const std::string& email) {
  DCHECK(!identity_manager->HasPrimaryAccount(ConsentLevel::kNotRequired));

  AccountInfo account_info =
      EnsureAccountExists(identity_manager->GetAccountTrackerService(), email);
  DCHECK(!account_info.gaia.empty());

  PrimaryAccountManager* primary_account_manager =
      identity_manager->GetPrimaryAccountManager();
  primary_account_manager->SetUnconsentedPrimaryAccountInfo(account_info);

  DCHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kNotRequired));
  DCHECK_EQ(account_info.gaia,
            identity_manager
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
                .gaia);
  return identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);
}

void SetRefreshTokenForPrimaryAccount(IdentityManager* identity_manager,
                                      const std::string& token_value) {
  DCHECK(identity_manager->HasPrimaryAccount());
  CoreAccountId account_id = identity_manager->GetPrimaryAccountId();
  SetRefreshTokenForAccount(identity_manager, account_id, token_value);
}

void SetInvalidRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager) {
  DCHECK(identity_manager->HasPrimaryAccount());
  CoreAccountId account_id = identity_manager->GetPrimaryAccountId();

  SetInvalidRefreshTokenForAccount(identity_manager, account_id);
}

void RemoveRefreshTokenForPrimaryAccount(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount())
    return;

  CoreAccountId account_id = identity_manager->GetPrimaryAccountId();

  RemoveRefreshTokenForAccount(identity_manager, account_id);
}

AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email) {
  CoreAccountInfo account_info = SetPrimaryAccount(identity_manager, email);
  SetRefreshTokenForPrimaryAccount(identity_manager);
  base::Optional<AccountInfo> primary_account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_info.account_id);
  // Ensure that extended information for the account is available after setting
  // the refresh token.
  DCHECK(primary_account_info.has_value());
  return primary_account_info.value();
}

void ClearPrimaryAccount(IdentityManager* identity_manager,
                         ClearPrimaryAccountPolicy policy) {
#if defined(OS_CHROMEOS)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED();
#else
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kNotRequired))
    return;

  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSync)) {
    PrimaryAccountManager* primary_account_manager =
        identity_manager->GetPrimaryAccountManager();
    primary_account_manager->SetUnconsentedPrimaryAccountInfo(
        CoreAccountInfo());
    RemoveRefreshTokenForAccount(
        identity_manager,
        identity_manager->GetPrimaryAccountId(ConsentLevel::kNotRequired));
    return;
  }

  base::RunLoop run_loop;
  TestIdentityManagerObserver signout_observer(identity_manager);
  signout_observer.SetOnPrimaryAccountClearedCallback(run_loop.QuitClosure());

  PrimaryAccountManager* primary_account_manager =
      identity_manager->GetPrimaryAccountManager();
  signin_metrics::ProfileSignout signout_source_metric =
      signin_metrics::SIGNOUT_TEST;
  signin_metrics::SignoutDelete signout_delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  switch (policy) {
    case ClearPrimaryAccountPolicy::DEFAULT:
      primary_account_manager->SignOut(signout_source_metric,
                                       signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::KEEP_ALL_ACCOUNTS:
      primary_account_manager->SignOutAndKeepAllAccounts(signout_source_metric,
                                                         signout_delete_metric);
      break;
    case ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS:
      primary_account_manager->SignOutAndRemoveAllAccounts(
          signout_source_metric, signout_delete_metric);
      break;
  }

  run_loop.Run();
#endif
}

AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(account_tracker_service->FindAccountInfoByEmail(email).IsEmpty());

  // Wait until tokens are loaded, otherwise the account will be removed as soon
  // as tokens finish loading.
  WaitForLoadCredentialsToComplete(identity_manager);

  std::string gaia_id = GetTestGaiaIdForEmail(email);
  account_tracker_service->SeedAccountInfo(gaia_id, email);

  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  DCHECK(!account_info.account_id.empty());

  SetRefreshTokenForAccount(identity_manager, account_info.account_id);

  return account_info;
}

AccountInfo MakeAccountAvailableWithCookies(
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email,
    const std::string& gaia_id) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(account_tracker_service->FindAccountInfoByEmail(email).IsEmpty());

  // Wait until tokens are loaded, otherwise the account will be removed as soon
  // as tokens finish loading.
  WaitForLoadCredentialsToComplete(identity_manager);

  SetCookieAccounts(identity_manager, test_url_loader_factory,
                    {{email, gaia_id}});

  account_tracker_service->SeedAccountInfo(gaia_id, email);

  AccountInfo account_info =
      account_tracker_service->FindAccountInfoByEmail(email);
  DCHECK(!account_info.account_id.empty());

  SetRefreshTokenForAccount(identity_manager, account_info.account_id);

  return account_info;
}

void SetRefreshTokenForAccount(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& token_value) {
  UpdateRefreshTokenForAccount(
      identity_manager->GetTokenService(),
      identity_manager->GetAccountTrackerService(),
#if defined(OS_CHROMEOS)
      identity_manager->GetChromeOSAccountManager(),
#endif
      identity_manager, account_id,
      token_value.empty() ? "refresh_token_for_" + account_id.ToString()
                                + "_" + base::GenerateGUID()
                          : token_value);
}

void SetInvalidRefreshTokenForAccount(IdentityManager* identity_manager,
                                      const CoreAccountId& account_id) {
  UpdateRefreshTokenForAccount(identity_manager->GetTokenService(),

                               identity_manager->GetAccountTrackerService(),
#if defined(OS_CHROMEOS)
                               identity_manager->GetChromeOSAccountManager(),
#endif
                               identity_manager, account_id,
                               GaiaConstants::kInvalidRefreshToken);
}

void RemoveRefreshTokenForAccount(IdentityManager* identity_manager,
                                  const CoreAccountId& account_id) {
  if (!identity_manager->HasAccountWithRefreshToken(account_id))
    return;

  base::RunLoop run_loop;
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenRemovedCallback(
      run_loop.QuitClosure());

#if defined(OS_CHROMEOS)
  const AccountInfo& account_info =
      identity_manager->GetAccountTrackerService()->GetAccountInfo(account_id);

  identity_manager->GetChromeOSAccountManager()->RemoveAccount(
      chromeos::AccountManager::AccountKey{
          account_info.gaia,
          chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA});
#else
  identity_manager->GetTokenService()->RevokeCredentials(account_id);
#endif

  run_loop.Run();
}

void SetCookieAccounts(
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::vector<CookieParamsForTest>& cookie_accounts) {
  // Convert |cookie_accounts| to the format list_accounts_test_utils wants.
  std::vector<CookieParams> gaia_cookie_accounts;
  for (const CookieParamsForTest& params : cookie_accounts) {
    gaia_cookie_accounts.push_back({params.email, params.gaia_id,
                                    /*valid=*/true, /*signed_out=*/false,
                                    /*verified=*/true});
  }

  base::RunLoop run_loop;
  TestIdentityManagerObserver cookie_observer(identity_manager);
  cookie_observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());

  SetListAccountsResponseWithParams(gaia_cookie_accounts,
                                    test_url_loader_factory);

  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(true);
  cookie_manager->ListAccounts(nullptr, nullptr);

  run_loop.Run();
}

void UpdateAccountInfoForAccount(IdentityManager* identity_manager,
                                 AccountInfo account_info) {
  // Make sure the account being updated is a known account.

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(!account_tracker_service->GetAccountInfo(account_info.account_id)
              .account_id.empty());

  account_tracker_service->SeedAccountInfo(account_info);
}

void SimulateAccountImageFetch(IdentityManager* identity_manager,
                               const CoreAccountId& account_id,
                               const std::string& image_url_with_size,
                               const gfx::Image& image) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  account_tracker_service->SetAccountImage(account_id, image_url_with_size,
                                           image);
}

void SetFreshnessOfAccountsInGaiaCookie(IdentityManager* identity_manager,
                                        bool accounts_are_fresh) {
  GaiaCookieManagerService* cookie_manager =
      identity_manager->GetGaiaCookieManagerService();
  cookie_manager->set_list_accounts_stale_for_testing(!accounts_are_fresh);
}

std::string GetTestGaiaIdForEmail(const std::string& email) {
  std::string gaia_id =
      std::string("gaia_id_for_") + gaia::CanonicalizeEmail(email);
  // Avoid character '@' in the gaia ID string as there is code in the codebase
  // that asserts that a gaia ID does not contain a "@" character.
  std::replace(gaia_id.begin(), gaia_id.end(), '@', '_');
  return gaia_id;
}

void UpdatePersistentErrorOfRefreshTokenForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));
  identity_manager->GetTokenService()->GetDelegate()->UpdateAuthError(
      account_id, auth_error);
}

void DisableAccessTokenFetchRetries(IdentityManager* identity_manager) {
  identity_manager->GetTokenService()
      ->set_max_authorization_token_fetch_retries_for_testing(0);
}

#if defined(OS_ANDROID)
void DisableInteractionWithSystemAccounts() {
  ProfileOAuth2TokenServiceDelegateAndroid::
      set_disable_interaction_with_system_accounts();
}
#endif

void CancelAllOngoingGaiaCookieOperations(IdentityManager* identity_manager) {
  identity_manager->GetGaiaCookieManagerService()->CancelAll();
}

void SimulateSuccessfulFetchOfAccountInfo(IdentityManager* identity_manager,
                                          const CoreAccountId& account_id,
                                          const std::string& email,
                                          const std::string& gaia,
                                          const std::string& hosted_domain,
                                          const std::string& full_name,
                                          const std::string& given_name,
                                          const std::string& locale,
                                          const std::string& picture_url) {
  base::DictionaryValue user_info;
  user_info.SetString("id", gaia);
  user_info.SetString("email", email);
  user_info.SetString("hd", hosted_domain);
  user_info.SetString("name", full_name);
  user_info.SetString("given_name", given_name);
  user_info.SetString("locale", locale);
  user_info.SetString("picture", picture_url);

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  account_tracker_service->SetAccountInfoFromUserInfo(account_id, &user_info);
}

}  // namespace signin
