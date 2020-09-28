// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager.h"

#include <string>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/ubertoken_fetcher_impl.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/android/jni_headers/IdentityManager_jni.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#endif

namespace signin {

IdentityManager::InitParameters::InitParameters() = default;

IdentityManager::InitParameters::InitParameters(InitParameters&&) = default;

IdentityManager::InitParameters::~InitParameters() = default;

IdentityManager::IdentityManager(IdentityManager::InitParameters&& parameters)
    : account_tracker_service_(std::move(parameters.account_tracker_service)),
      token_service_(std::move(parameters.token_service)),
      gaia_cookie_manager_service_(
          std::move(parameters.gaia_cookie_manager_service)),
      primary_account_manager_(std::move(parameters.primary_account_manager)),
      account_fetcher_service_(std::move(parameters.account_fetcher_service)),
      identity_mutator_(std::move(parameters.primary_account_mutator),
                        std::move(parameters.accounts_mutator),
                        std::move(parameters.accounts_cookie_mutator),
                        std::move(parameters.device_accounts_synchronizer)),
      diagnostics_provider_(std::move(parameters.diagnostics_provider)) {
  DCHECK(account_fetcher_service_);
  DCHECK(diagnostics_provider_);

  primary_account_manager_observer_.Add(primary_account_manager_.get());
  token_service_observer_.Add(token_service_.get());
  token_service_->AddAccessTokenDiagnosticsObserver(this);

  // IdentityManager owns the ATS, GCMS and PO2TS instances and will outlive
  // them, so base::Unretained is safe.
  account_tracker_service_->SetOnAccountUpdatedCallback(base::BindRepeating(
      &IdentityManager::OnAccountUpdated, base::Unretained(this)));
  account_tracker_service_->SetOnAccountRemovedCallback(base::BindRepeating(
      &IdentityManager::OnAccountRemoved, base::Unretained(this)));
  gaia_cookie_manager_service_->SetGaiaAccountsInCookieUpdatedCallback(
      base::BindRepeating(&IdentityManager::OnGaiaAccountsInCookieUpdated,
                          base::Unretained(this)));
  gaia_cookie_manager_service_->SetGaiaCookieDeletedByUserActionCallback(
      base::BindRepeating(&IdentityManager::OnGaiaCookieDeletedByUserAction,
                          base::Unretained(this)));
  token_service_->SetRefreshTokenAvailableFromSourceCallback(
      base::BindRepeating(&IdentityManager::OnRefreshTokenAvailableFromSource,
                          base::Unretained(this)));
  token_service_->SetRefreshTokenRevokedFromSourceCallback(
      base::BindRepeating(&IdentityManager::OnRefreshTokenRevokedFromSource,
                          base::Unretained(this)));

#if defined(OS_ANDROID)
  java_identity_manager_ = Java_IdentityManager_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      token_service_->GetDelegate()->GetJavaObject());
#endif

#if defined(OS_CHROMEOS)
  chromeos_account_manager_ = parameters.chromeos_account_manager;
#endif
}

IdentityManager::~IdentityManager() {
  account_fetcher_service_->Shutdown();
  gaia_cookie_manager_service_->Shutdown();
  token_service_->Shutdown();
  account_tracker_service_->Shutdown();

  token_service_->RemoveAccessTokenDiagnosticsObserver(this);

#if defined(OS_ANDROID)
  if (java_identity_manager_)
    Java_IdentityManager_destroy(base::android::AttachCurrentThread(),
                                 java_identity_manager_);
#endif
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// TODO(862619) change return type to base::Optional<CoreAccountInfo>
CoreAccountInfo IdentityManager::GetPrimaryAccountInfo(
    ConsentLevel consent) const {
  if (consent == ConsentLevel::kNotRequired) {
    return primary_account_manager_->GetUnconsentedPrimaryAccountInfo();
  }
  return primary_account_manager_->GetAuthenticatedAccountInfo();
}

CoreAccountId IdentityManager::GetPrimaryAccountId(ConsentLevel consent) const {
  return GetPrimaryAccountInfo(consent).account_id;
}

bool IdentityManager::HasPrimaryAccount(ConsentLevel consent) const {
  if (consent == ConsentLevel::kNotRequired) {
    return primary_account_manager_->HasUnconsentedPrimaryAccount();
  }
  return primary_account_manager_->IsAuthenticated();
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    const ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(account_id, oauth_consumer_name,
                                              token_service_.get(), scopes,
                                              std::move(callback), mode);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, oauth_consumer_name, token_service_.get(), url_loader_factory,
      scopes, std::move(callback), mode);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& oauth_consumer_name,
    const ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, client_id, client_secret, oauth_consumer_name,
      token_service_.get(), scopes, std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const CoreAccountId& account_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(account_id, scopes, access_token);
}

std::vector<CoreAccountInfo> IdentityManager::GetAccountsWithRefreshTokens()
    const {
  std::vector<CoreAccountId> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<CoreAccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const CoreAccountId& account_id : account_ids_with_tokens) {
    accounts.push_back(GetAccountInfoForAccountWithRefreshToken(account_id));
  }

  return accounts;
}

std::vector<AccountInfo>
IdentityManager::GetExtendedAccountInfoForAccountsWithRefreshToken() const {
  std::vector<CoreAccountId> account_ids_with_tokens =
      token_service_->GetAccounts();

  std::vector<AccountInfo> accounts;
  accounts.reserve(account_ids_with_tokens.size());

  for (const CoreAccountId& account_id : account_ids_with_tokens) {
    accounts.push_back(GetAccountInfoForAccountWithRefreshToken(account_id));
  }

  return accounts;
}

bool IdentityManager::HasPrimaryAccountWithRefreshToken() const {
  return HasAccountWithRefreshToken(GetPrimaryAccountId());
}

bool IdentityManager::HasAccountWithRefreshToken(
    const CoreAccountId& account_id) const {
  return token_service_->RefreshTokenIsAvailable(account_id);
}

bool IdentityManager::AreRefreshTokensLoaded() const {
  return token_service_->AreAllCredentialsLoaded();
}

bool IdentityManager::HasAccountWithRefreshTokenInPersistentErrorState(
    const CoreAccountId& account_id) const {
  return GetErrorStateOfRefreshTokenForAccount(account_id).IsPersistentError();
}

GoogleServiceAuthError IdentityManager::GetErrorStateOfRefreshTokenForAccount(
    const CoreAccountId& account_id) const {
  return token_service_->GetAuthError(account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindExtendedAccountInfoForAccountWithRefreshToken(
    const CoreAccountInfo& account_info) const {
  AccountInfo extended_account_info =
      account_tracker_service_->GetAccountInfo(account_info.account_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(extended_account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
    const CoreAccountId& account_id) const {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo> IdentityManager::
    FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
        const std::string& email_address) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByEmail(email_address);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

base::Optional<AccountInfo>
IdentityManager::FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
    const std::string& gaia_id) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByGaiaId(gaia_id);

  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  if (!HasAccountWithRefreshToken(account_info.account_id))
    return base::nullopt;

  return GetAccountInfoForAccountWithRefreshToken(account_info.account_id);
}

std::unique_ptr<UbertokenFetcher>
IdentityManager::CreateUbertokenFetcherForAccount(
    const CoreAccountId& account_id,
    UbertokenFetcher::CompletionCallback callback,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<UbertokenFetcherImpl>(
      account_id, token_service_.get(), std::move(callback), source,
      url_loader_factory);
}

AccountsInCookieJarInfo IdentityManager::GetAccountsInCookieJar() const {
  std::vector<gaia::ListedAccount> signed_in_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  bool accounts_are_fresh = gaia_cookie_manager_service_->ListAccounts(
      &signed_in_accounts, &signed_out_accounts);

  return AccountsInCookieJarInfo(accounts_are_fresh, signed_in_accounts,
                                 signed_out_accounts);
}

PrimaryAccountMutator* IdentityManager::GetPrimaryAccountMutator() {
  return identity_mutator_.GetPrimaryAccountMutator();
}

AccountsMutator* IdentityManager::GetAccountsMutator() {
  return identity_mutator_.GetAccountsMutator();
}

AccountsCookieMutator* IdentityManager::GetAccountsCookieMutator() {
  return identity_mutator_.GetAccountsCookieMutator();
}

DeviceAccountsSynchronizer* IdentityManager::GetDeviceAccountsSynchronizer() {
  return identity_mutator_.GetDeviceAccountsSynchronizer();
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

void IdentityManager::OnNetworkInitialized() {
  gaia_cookie_manager_service_->InitCookieListener();
  account_fetcher_service_->OnNetworkInitialized();
}

IdentityManager::AccountIdMigrationState
IdentityManager::GetAccountIdMigrationState() const {
  return static_cast<IdentityManager::AccountIdMigrationState>(
      account_tracker_service_->GetMigrationState());
}

CoreAccountId IdentityManager::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) const {
  return account_tracker_service_->PickAccountIdForAccount(gaia, email);
}

// static
void IdentityManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  PrimaryAccountManager::RegisterPrefs(registry);
}

// static
void IdentityManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
  PrimaryAccountManager::RegisterProfilePrefs(registry);
  AccountFetcherService::RegisterPrefs(registry);
  AccountTrackerService::RegisterPrefs(registry);
  GaiaCookieManagerService::RegisterPrefs(registry);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MutableProfileOAuth2TokenServiceDelegate::RegisterProfilePrefs(registry);
#endif
}

DiagnosticsProvider* IdentityManager::GetDiagnosticsProvider() {
  return diagnostics_provider_.get();
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
IdentityManager::LegacyGetAccountTrackerServiceJavaObject() {
  return account_tracker_service_->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject> IdentityManager::GetJavaObject() {
  DCHECK(java_identity_manager_);
  return base::android::ScopedJavaLocalRef<jobject>(java_identity_manager_);
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::GetIdentityMutatorJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(
      identity_mutator_.GetJavaObject());
}

void IdentityManager::ForceRefreshOfExtendedAccountInfo(
    const CoreAccountId& account_id) {
  DCHECK(HasAccountWithRefreshToken(account_id));
  account_fetcher_service_->ForceRefreshOfAccountInfo(account_id);
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::GetPrimaryAccountInfo(JNIEnv* env, jint consent_level) const {
  CoreAccountInfo account_info =
      GetPrimaryAccountInfo(static_cast<ConsentLevel>(consent_level));
  if (account_info.IsEmpty())
    return nullptr;
  return ConvertToJavaCoreAccountInfo(env, account_info);
}

base::android::ScopedJavaLocalRef<jobject> IdentityManager::
    FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
        JNIEnv* env,
        const base::android::JavaParamRef<jstring>& j_email) const {
  auto account_info =
      FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
          base::android::ConvertJavaStringToUTF8(env, j_email));
  if (!account_info.has_value())
    return nullptr;
  return ConvertToJavaAccountInfo(env, account_info.value());
}

base::android::ScopedJavaLocalRef<jobjectArray>
IdentityManager::GetAccountsWithRefreshTokens(JNIEnv* env) const {
  std::vector<CoreAccountInfo> accounts = GetAccountsWithRefreshTokens();

  base::android::ScopedJavaLocalRef<jclass> coreaccountinfo_clazz =
      base::android::GetClass(
          env, "org/chromium/components/signin/base/CoreAccountInfo");
  base::android::ScopedJavaLocalRef<jobjectArray> array(
      env, env->NewObjectArray(accounts.size(), coreaccountinfo_clazz.obj(),
                               nullptr));
  base::android::CheckException(env);

  for (size_t i = 0; i < accounts.size(); ++i) {
    base::android::ScopedJavaLocalRef<jobject> item =
        ConvertToJavaCoreAccountInfo(env, accounts[i]);
    env->SetObjectArrayElement(array.obj(), i, item.obj());
  }
  return array;
}
#endif

PrimaryAccountManager* IdentityManager::GetPrimaryAccountManager() const {
  return primary_account_manager_.get();
}

ProfileOAuth2TokenService* IdentityManager::GetTokenService() const {
  return token_service_.get();
}

AccountTrackerService* IdentityManager::GetAccountTrackerService() const {
  return account_tracker_service_.get();
}

AccountFetcherService* IdentityManager::GetAccountFetcherService() const {
  return account_fetcher_service_.get();
}

GaiaCookieManagerService* IdentityManager::GetGaiaCookieManagerService() const {
  return gaia_cookie_manager_service_.get();
}

#if defined(OS_CHROMEOS)
chromeos::AccountManager* IdentityManager::GetChromeOSAccountManager() const {
  return chromeos_account_manager_;
}
#endif

AccountInfo IdentityManager::GetAccountInfoForAccountWithRefreshToken(
    const CoreAccountId& account_id) const {
  // TODO(https://crbug.com/919793): This invariant is not currently possible to
  // enforce on Android due to the underlying relationship between
  // O2TS::GetAccounts(), O2TS::RefreshTokenIsAvailable(), and
  // O2TS::Observer::OnRefreshTokenAvailable().
#if !defined(OS_ANDROID)
  DCHECK(HasAccountWithRefreshToken(account_id));
#endif

  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());

  return account_info;
}

void IdentityManager::GoogleSigninSucceeded(
    const CoreAccountInfo& account_info) {
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
#if defined(OS_ANDROID)
  if (java_identity_manager_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onPrimaryAccountSet(
        env, java_identity_manager_,
        ConvertToJavaCoreAccountInfo(env, account_info));
  }
#endif
}

void IdentityManager::UnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& account_info) {
  for (auto& observer : observer_list_)
    observer.OnUnconsentedPrimaryAccountChanged(account_info);
}

void IdentityManager::GoogleSignedOut(const CoreAccountInfo& account_info) {
  DCHECK(!HasPrimaryAccount());
  DCHECK(!account_info.IsEmpty());
  for (auto& observer : observer_list_) {
    observer.BeforePrimaryAccountCleared(account_info);
  }

  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }

#if defined(OS_ANDROID)
  if (java_identity_manager_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onPrimaryAccountCleared(
        env, java_identity_manager_,
        ConvertToJavaCoreAccountInfo(env, account_info));
  }
#endif
}

void IdentityManager::OnRefreshTokenAvailable(const CoreAccountId& account_id) {
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info);
  }
}

void IdentityManager::OnRefreshTokenRevoked(const CoreAccountId& account_id) {
  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenRemovedForAccount(account_id);
  }
}

void IdentityManager::OnRefreshTokensLoaded() {
  for (auto& observer : observer_list_)
    observer.OnRefreshTokensLoaded();
}

void IdentityManager::OnEndBatchChanges() {
  for (auto& observer : observer_list_)
    observer.OnEndBatchOfRefreshTokenStateChanges();
}

void IdentityManager::OnAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_)
    observer.OnErrorStateOfRefreshTokenUpdatedForAccount(account_info,
                                                         auth_error);
}

void IdentityManager::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& signed_in_accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  AccountsInCookieJarInfo accounts_in_cookie_jar_info(
      error == GoogleServiceAuthError::AuthErrorNone(), signed_in_accounts,
      signed_out_accounts);

  for (auto& observer : observer_list_) {
    observer.OnAccountsInCookieUpdated(accounts_in_cookie_jar_info, error);
  }
}

void IdentityManager::OnGaiaCookieDeletedByUserAction() {
  for (auto& observer : observer_list_) {
    observer.OnAccountsCookieDeletedByUserAction();
  }
#if defined(OS_ANDROID)
  if (java_identity_manager_) {
    Java_IdentityManager_onAccountsCookieDeletedByUserAction(
        base::android::AttachCurrentThread(), java_identity_manager_);
  }
#endif
}

void IdentityManager::OnAccessTokenRequested(const CoreAccountId& account_id,
                                             const std::string& consumer_id,
                                             const ScopeSet& scopes) {
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

void IdentityManager::OnFetchAccessTokenComplete(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const ScopeSet& scopes,
    GoogleServiceAuthError error,
    base::Time expiration_time) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRequestCompleted(account_id, consumer_id, scopes,
                                           error, expiration_time);
}

void IdentityManager::OnAccessTokenRemoved(const CoreAccountId& account_id,
                                           const ScopeSet& scopes) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnAccessTokenRemovedFromCache(account_id, scopes);
}

void IdentityManager::OnRefreshTokenAvailableFromSource(
    const CoreAccountId& account_id,
    bool is_refresh_token_valid,
    const std::string& source) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnRefreshTokenUpdatedForAccountFromSource(
        account_id, is_refresh_token_valid, source);
}

void IdentityManager::OnRefreshTokenRevokedFromSource(
    const CoreAccountId& account_id,
    const std::string& source) {
  for (auto& observer : diagnostics_observer_list_)
    observer.OnRefreshTokenRemovedForAccountFromSource(account_id, source);
}

void IdentityManager::OnAccountUpdated(const AccountInfo& info) {
  if (HasPrimaryAccount()) {
    const CoreAccountId primary_account_id = GetPrimaryAccountId();
    if (primary_account_id == info.account_id) {
      primary_account_manager_->UpdateAuthenticatedAccountInfo();
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnExtendedAccountInfoUpdated(info);
  }
#if defined(OS_ANDROID)
  if (java_identity_manager_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onExtendedAccountInfoUpdated(
        env, java_identity_manager_, ConvertToJavaAccountInfo(env, info));
  }
#endif
}

void IdentityManager::OnAccountRemoved(const AccountInfo& info) {
  for (auto& observer : observer_list_)
    observer.OnExtendedAccountInfoRemoved(info);
}

}  // namespace signin
