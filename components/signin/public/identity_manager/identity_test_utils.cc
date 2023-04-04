// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_utils.h"

#include <vector>

#include "base/run_loop.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_android.h"
#include "components/signin/public/android/test_support_jni_headers/AccountManagerFacadeUtil_jni.h"
#endif

namespace signin {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
// Whether identity_test_utils uses `AccountManagerFacade` or
// `ProfileOAuth2TokenService` for managing credentials.
bool ShouldUseAccountManagerFacade(IdentityManager* identity_manager) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If account consistency is `kMirror` - use `AccountManagerFacade` for
  // managing credentials, otherwise use `ProfileOAuth2TokenService`.
  return identity_manager->GetAccountConsistency() ==
         AccountConsistencyMethod::kMirror;
#else
  // In Ash - always use `AccountManagerFacade` for managing credentials.
  return true;
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Helper function that updates the refresh token for |account_id| to
// |new_token|. Before updating the refresh token, blocks until refresh tokens
// are loaded. After updating the token, blocks until the update is processed by
// |identity_manager|.
void UpdateRefreshTokenForAccount(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
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
  WaitForRefreshTokensLoaded(identity_manager);

  base::RunLoop run_loop;
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnRefreshTokenUpdatedCallback(
      run_loop.QuitClosure());

  // TODO(crbug.com/1226041): simplify this when all Lacros Profiles use Mirror.
#if BUILDFLAG(IS_CHROMEOS)
  if (ShouldUseAccountManagerFacade(identity_manager)) {
    const AccountInfo& account_info =
        account_tracker_service->GetAccountInfo(account_id);
    account_manager::Account account{
        account_manager::AccountKey{account_info.gaia,
                                    account_manager::AccountType::kGaia},
        account_info.email};
    GetAccountManagerFacade(identity_manager)
        ->UpsertAccountForTesting(account, new_token);
  } else
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
    token_service->UpdateCredentials(account_id, new_token);
  }

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

// Helper for `WaitForErrorStateOfRefreshTokenUpdatedForAccount` - checks the
// current error status using a predicate and calls quit_closure if the
// predicate returns `true`.
void CompareErrorStatusAndCallClosure(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate,
    const base::RepeatingClosure& quit_closure) {
  GoogleServiceAuthError error =
      identity_manager->GetErrorStateOfRefreshTokenForAccount(account_id);
  if (predicate.Run(error))
    quit_closure.Run();
}

}  // namespace

void WaitForRefreshTokensLoaded(IdentityManager* identity_manager) {
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

  DCHECK(identity_manager->AreRefreshTokensLoaded());
}

absl::optional<signin::ConsentLevel> GetPrimaryAccountConsentLevel(
    IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return absl::nullopt;
  }

  return identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)
             ? signin::ConsentLevel::kSync
             : signin::ConsentLevel::kSignin;
}

CoreAccountInfo SetPrimaryAccount(IdentityManager* identity_manager,
                                  const std::string& email,
                                  ConsentLevel consent_level) {
  DCHECK(
      !identity_manager->HasPrimaryAccount(consent_level) ||
      (identity_manager->GetPrimaryAccountInfo(consent_level).email != email &&
       consent_level == ConsentLevel::kSignin));

  AccountInfo account_info =
      EnsureAccountExists(identity_manager->GetAccountTrackerService(), email);
  DCHECK(!account_info.gaia.empty());

  PrimaryAccountManager* primary_account_manager =
      identity_manager->GetPrimaryAccountManager();
  primary_account_manager->SetPrimaryAccountInfo(
      account_info, consent_level,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  DCHECK(identity_manager->HasPrimaryAccount(consent_level));
  DCHECK_EQ(account_info.gaia,
            identity_manager->GetPrimaryAccountInfo(consent_level).gaia);
  return identity_manager->GetPrimaryAccountInfo(consent_level);
}

void SetRefreshTokenForPrimaryAccount(IdentityManager* identity_manager,
                                      const std::string& token_value) {
  // Primary account for ConsentLevel::kSync (if one exists) is always the
  // same as the one with ConsentLevel::kSignin.
  DCHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);
  SetRefreshTokenForAccount(identity_manager, account_id, token_value);
}

void SetInvalidRefreshTokenForPrimaryAccount(
    IdentityManager* identity_manager) {
  DCHECK(identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);

  SetInvalidRefreshTokenForAccount(identity_manager, account_id);
}

void RemoveRefreshTokenForPrimaryAccount(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSignin))
    return;

  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(ConsentLevel::kSignin);

  RemoveRefreshTokenForAccount(identity_manager, account_id);
}

AccountInfo MakePrimaryAccountAvailable(IdentityManager* identity_manager,
                                        const std::string& email,
                                        ConsentLevel consent_level) {
  CoreAccountInfo account_info =
      SetPrimaryAccount(identity_manager, email, consent_level);
  SetRefreshTokenForPrimaryAccount(identity_manager);
  AccountInfo primary_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          account_info.account_id);
  // Ensure that extended information for the account is available after setting
  // the refresh token.
  DCHECK(!primary_account_info.IsEmpty());
  return primary_account_info;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void RevokeSyncConsent(IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSync))
    return;

  DCHECK(identity_manager->GetPrimaryAccountMutator());
  base::RunLoop run_loop;
  TestIdentityManagerObserver signout_observer(identity_manager);
  signout_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](base::RunLoop* run_loop, PrimaryAccountChangeEvent event) {
        if (event.GetEventTypeFor(ConsentLevel::kSync) ==
            PrimaryAccountChangeEvent::Type::kCleared) {
          run_loop->Quit();
        }
      },
      &run_loop));
  identity_manager->GetPrimaryAccountMutator()->RevokeSyncConsent(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void ClearPrimaryAccount(IdentityManager* identity_manager) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(blundell): If we ever need this functionality on ChromeOS (which seems
  // unlikely), plumb this through to just clear the primary account info
  // synchronously with IdentityManager.
  NOTREACHED();
#else
  if (!identity_manager->HasPrimaryAccount(ConsentLevel::kSignin))
    return;

  DCHECK(identity_manager->GetPrimaryAccountMutator());
  base::RunLoop run_loop;
  TestIdentityManagerObserver signout_observer(identity_manager);
  signout_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](base::RunLoop* run_loop, PrimaryAccountChangeEvent event) {
        if (event.GetEventTypeFor(ConsentLevel::kSignin) ==
            PrimaryAccountChangeEvent::Type::kCleared) {
          run_loop->Quit();
        }
      },
      &run_loop));
  identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);

  run_loop.Run();
#endif
}

void WaitForPrimaryAccount(IdentityManager* identity_manager,
                           ConsentLevel consent_level,
                           const CoreAccountId& account_id) {
  if (identity_manager->GetPrimaryAccountId(consent_level) == account_id)
    return;

  base::RunLoop run_loop;
  TestIdentityManagerObserver primary_account_observer(identity_manager);
  primary_account_observer.SetOnPrimaryAccountChangedCallback(base::BindOnce(
      [](IdentityManager* identity_manager, ConsentLevel consent_level,
         const CoreAccountId& account_id, base::RunLoop* run_loop,
         PrimaryAccountChangeEvent event) {
        if (identity_manager->GetPrimaryAccountId(consent_level) == account_id)
          run_loop->Quit();
      },
      identity_manager, consent_level, account_id, &run_loop));
  run_loop.Run();
}

AccountInfo MakeAccountAvailable(IdentityManager* identity_manager,
                                 const std::string& email) {
  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();

  DCHECK(account_tracker_service);
  DCHECK(account_tracker_service->FindAccountInfoByEmail(email).IsEmpty());

  // Wait until tokens are loaded, otherwise the account will be removed as soon
  // as tokens finish loading.
  WaitForRefreshTokensLoaded(identity_manager);

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
  WaitForRefreshTokensLoaded(identity_manager);

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
      identity_manager->GetAccountTrackerService(), identity_manager,
      account_id,
      token_value.empty()
          ? "refresh_token_for_" + account_id.ToString() + "_" +
                base::Uuid::GenerateRandomV4().AsLowercaseString()
          : token_value);
}

void SetInvalidRefreshTokenForAccount(IdentityManager* identity_manager,
                                      const CoreAccountId& account_id) {
  UpdateRefreshTokenForAccount(identity_manager->GetTokenService(),

                               identity_manager->GetAccountTrackerService(),
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

  // TODO(crbug.com/1226041): simplify this when all Lacros Profiles use Mirror.
#if BUILDFLAG(IS_CHROMEOS)
  if (ShouldUseAccountManagerFacade(identity_manager)) {
    const AccountInfo& account_info =
        identity_manager->GetAccountTrackerService()->GetAccountInfo(
            account_id);
    GetAccountManagerFacade(identity_manager)
        ->RemoveAccountForTesting(account_manager::AccountKey{
            account_info.gaia, account_manager::AccountType::kGaia});
  } else
#endif  // BUILDFLAG(IS_CHROMEOS)
  {
    identity_manager->GetTokenService()->RevokeCredentials(account_id);
  }

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

void WaitForErrorStateOfRefreshTokenUpdatedForAccount(
    IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    base::RepeatingCallback<bool(const GoogleServiceAuthError&)> predicate) {
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));

  base::RunLoop run_loop;
  base::RepeatingClosure check_error_status =
      base::BindRepeating(&CompareErrorStatusAndCallClosure, identity_manager,
                          account_id, predicate, run_loop.QuitClosure());
  TestIdentityManagerObserver token_updated_observer(identity_manager);
  token_updated_observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
      check_error_status);
  // Call callback explicitly to check the current error state before waiting.
  check_error_status.Run();

  run_loop.Run();
}

void DisableAccessTokenFetchRetries(IdentityManager* identity_manager) {
  identity_manager->GetTokenService()
      ->set_max_authorization_token_fetch_retries_for_testing(0);
}

void EnableAccountCapabilitiesFetches(IdentityManager* identity_manager) {
  identity_manager->GetAccountFetcherService()
      ->EnableAccountCapabilitiesFetcherForTest(true);
}

#if BUILDFLAG(IS_ANDROID)
void SetUpMockAccountManagerFacade() {
  Java_AccountManagerFacadeUtil_setUpMockFacade(
      base::android::AttachCurrentThread());
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
  base::Value::Dict user_info;
  user_info.Set("id", gaia);
  user_info.Set("email", email);
  user_info.Set("hd", hosted_domain);
  user_info.Set("name", full_name);
  user_info.Set("given_name", given_name);
  user_info.Set("locale", locale);
  user_info.Set("picture", picture_url);

  AccountTrackerService* account_tracker_service =
      identity_manager->GetAccountTrackerService();
  account_tracker_service->SetAccountInfoFromUserInfo(account_id, user_info);
}

#if BUILDFLAG(IS_CHROMEOS)
account_manager::AccountManagerFacade* GetAccountManagerFacade(
    IdentityManager* identity_manager) {
  return identity_manager->GetAccountManagerFacade();
}
#endif

}  // namespace signin
