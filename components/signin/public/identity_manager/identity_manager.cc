// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/android/jni_headers/IdentityManager_jni.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/account.h"
#include "components/signin/public/identity_manager/tribool.h"
#endif

namespace signin {

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)

void SetPrimaryAccount(IdentityManager* identity_manager,
                       AccountTrackerService* account_tracker_service,
                       SigninClient* signin_client,
                       const account_manager::Account& device_account,
                       signin::Tribool device_account_is_child,
                       ConsentLevel requested_level) {
  if (device_account.key.account_type() != account_manager::AccountType::kGaia)
    return;

  // An account can be set as the Primary Account only if it exists in
  // `AccountTrackerService`. However, for the first run, when accounts have not
  // yet been received from `AccountManagerFacade`, entities can ask about the
  // Primary Account and expect it to be available pretty early. Manually seed
  // the account in `AccountTrackerService` to get around this issue.
  const CoreAccountId device_account_id =
      account_tracker_service->SeedAccountInfo(
          /*gaia=*/device_account.key.id(), device_account.raw_email);

  const CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(requested_level);
  DCHECK(signin_client);

  if (primary_account_id == device_account_id) {
    identity_manager->GetAccountsMutator()->UpdateAccountInfo(
        device_account_id, device_account_is_child, signin::Tribool::kUnknown);

    return;  // Already correct primary account set, nothing to do.
  }

  if (!primary_account_id.empty()) {
    // Different primary account found, have to clear it first.
    // TODO(crbug.com/40774609): Replace this if with a CHECK after all
    //                                  the existing users have been migrated.
    identity_manager->GetPrimaryAccountMutator()->ClearPrimaryAccount(
        signin_metrics::ProfileSignout::kAccountRemovedFromDevice);
  }

  PrimaryAccountMutator::PrimaryAccountError error =
      identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
          device_account_id, requested_level);
  identity_manager->GetAccountsMutator()->UpdateAccountInfo(
      device_account_id, device_account_is_child, signin::Tribool::kUnknown);
  CHECK_EQ(PrimaryAccountMutator::PrimaryAccountError::kNoError, error)
      << "SetPrimaryAccount error: " << static_cast<int>(error);
  CHECK(identity_manager->HasPrimaryAccount(requested_level));
  CHECK_EQ(identity_manager->GetPrimaryAccountInfo(requested_level).gaia,
           device_account.key.id());
}
#endif

}  // namespace

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
      signin_client_(parameters.signin_client),
#if BUILDFLAG(IS_CHROMEOS)
      account_manager_facade_(parameters.account_manager_facade),
#endif
      identity_mutator_(std::make_unique<IdentityMutator>(
          std::move(parameters.primary_account_mutator),
          std::move(parameters.accounts_mutator),
          std::move(parameters.accounts_cookie_mutator),
          std::move(parameters.device_accounts_synchronizer))),
      diagnostics_provider_(std::move(parameters.diagnostics_provider)),
      account_consistency_(parameters.account_consistency),
      require_sync_consent_for_scope_verification_(
          parameters.require_sync_consent_for_scope_verification) {
  DCHECK(account_fetcher_service_);
  DCHECK(diagnostics_provider_);
  DCHECK(signin_client_);

  primary_account_manager_observation_.Observe(primary_account_manager_.get());
  token_service_observation_.Observe(token_service_.get());
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

#if BUILDFLAG(IS_ANDROID)
  java_identity_manager_ = Java_IdentityManager_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      token_service_->GetDelegate()->GetJavaObject());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // We need to set the Primary Account in Lacros. In Ash, this happens in
  // `UserSessionManager::InitProfilePreferences`, before anyone starts using
  // Profile / KeyedServices - but with the availability of IdentityManager. We
  // don't have such a place in Lacros - which guarantees that the Primary
  // Account will be available on startup - just like Ash.
  std::optional<account_manager::Account> initial_account =
      signin_client_->GetInitialPrimaryAccount();
  if (initial_account.has_value()) {
    const std::optional<bool>& initial_account_is_child =
        signin_client_->IsInitialPrimaryAccountChild();
    CHECK(initial_account_is_child.has_value());
    SetPrimaryAccount(this, account_tracker_service_.get(), signin_client_,
                      initial_account.value(),
                      initial_account_is_child.value()
                          ? signin::Tribool::kTrue
                          : signin::Tribool::kFalse,
                      ConsentLevel::kSignin);
  }
#endif
}

IdentityManager::~IdentityManager() {
#if BUILDFLAG(IS_ANDROID)
  if (java_identity_manager_)
    Java_IdentityManager_destroy(base::android::AttachCurrentThread(),
                                 java_identity_manager_);
#endif
}

void IdentityManager::Shutdown() {
  for (auto& observer : observer_list_)
    observer.OnIdentityManagerShutdown(this);

  // It is no longer safe to use the SigninClient beyond this point, everything
  // depending on it must be destroyed.
  token_service_->RemoveAccessTokenDiagnosticsObserver(this);
  token_service_observation_.Reset();
  primary_account_manager_observation_.Reset();

  diagnostics_provider_.reset();
  identity_mutator_.reset();
  account_fetcher_service_.reset();
  gaia_cookie_manager_service_.reset();
  primary_account_manager_.reset();
  token_service_.reset();
  account_tracker_service_.reset();
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// TODO(crbug.com/40584518) change return type to std::optional<CoreAccountInfo>
CoreAccountInfo IdentityManager::GetPrimaryAccountInfo(
    ConsentLevel consent) const {
  return primary_account_manager_->GetPrimaryAccountInfo(consent);
}

CoreAccountId IdentityManager::GetPrimaryAccountId(ConsentLevel consent) const {
  return GetPrimaryAccountInfo(consent).account_id;
}

bool IdentityManager::HasPrimaryAccount(ConsentLevel consent) const {
  return primary_account_manager_->HasPrimaryAccount(consent);
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    const ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    AccessTokenFetcher::Mode mode) {
  return std::make_unique<AccessTokenFetcher>(
      account_id, oauth_consumer_name, token_service_.get(),
      primary_account_manager_.get(), scopes, std::move(callback), mode,
      require_sync_consent_for_scope_verification_);
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
      account_id, oauth_consumer_name, token_service_.get(),
      primary_account_manager_.get(), url_loader_factory, scopes,
      std::move(callback), mode, require_sync_consent_for_scope_verification_);
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

bool IdentityManager::HasPrimaryAccountWithRefreshToken(
    ConsentLevel consent_level) const {
  return HasAccountWithRefreshToken(GetPrimaryAccountId(consent_level));
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::vector<uint8_t>
IdentityManager::GetWrappedBindingKeyOfRefreshTokenForAccount(
    const CoreAccountId& account_id) const {
  return token_service_->GetWrappedBindingKey(account_id);
}
#endif

GoogleServiceAuthError IdentityManager::GetErrorStateOfRefreshTokenForAccount(
    const CoreAccountId& account_id) const {
  return token_service_->GetAuthError(account_id);
}

AccountInfo IdentityManager::FindExtendedAccountInfo(
    const CoreAccountInfo& account_info) const {
  return FindExtendedAccountInfoByAccountId(account_info.account_id);
}

AccountInfo IdentityManager::FindExtendedAccountInfoByAccountId(
    const CoreAccountId& account_id) const {
  if (!HasAccountWithRefreshToken(account_id))
    return AccountInfo();

  // AccountTrackerService returns an empty AccountInfo if the account is not
  // found.
  return account_tracker_service_->GetAccountInfo(account_id);
}

AccountInfo IdentityManager::FindExtendedAccountInfoByEmailAddress(
    const std::string& email_address) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByEmail(email_address);
  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  return HasAccountWithRefreshToken(account_info.account_id) ? account_info
                                                             : AccountInfo();
}

AccountInfo IdentityManager::FindExtendedAccountInfoByGaiaId(
    const std::string& gaia_id) const {
  AccountInfo account_info =
      account_tracker_service_->FindAccountInfoByGaiaId(gaia_id);
  // AccountTrackerService always returns an AccountInfo, even on failure. In
  // case of failure, the AccountInfo will be unpopulated, thus we should not
  // be able to find a valid refresh token.
  return HasAccountWithRefreshToken(account_info.account_id) ? account_info
                                                             : AccountInfo();
}

AccountsInCookieJarInfo IdentityManager::GetAccountsInCookieJar() const {
  return gaia_cookie_manager_service_->ListAccounts();
}

PrimaryAccountMutator* IdentityManager::GetPrimaryAccountMutator() {
  return identity_mutator_->GetPrimaryAccountMutator();
}

AccountsMutator* IdentityManager::GetAccountsMutator() {
  return identity_mutator_->GetAccountsMutator();
}

AccountsCookieMutator* IdentityManager::GetAccountsCookieMutator() {
  return identity_mutator_->GetAccountsCookieMutator();
}

DeviceAccountsSynchronizer* IdentityManager::GetDeviceAccountsSynchronizer() {
  return identity_mutator_->GetDeviceAccountsSynchronizer();
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observation_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observation_list_.RemoveObserver(observer);
}

void IdentityManager::OnNetworkInitialized() {
  gaia_cookie_manager_service_->InitCookieListener();
  account_fetcher_service_->OnNetworkInitialized();
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
}

DiagnosticsProvider* IdentityManager::GetDiagnosticsProvider() {
  return diagnostics_provider_.get();
}

void IdentityManager::PrepareForAddingNewAccount() {
  account_fetcher_service_->PrepareForFetchingAccountCapabilities();
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> IdentityManager::GetJavaObject() {
  DCHECK(java_identity_manager_);
  return base::android::ScopedJavaLocalRef<jobject>(java_identity_manager_);
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::GetIdentityMutatorJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(
      identity_mutator_->GetJavaObject());
}

void IdentityManager::RefreshAccountInfoIfStale(
    const CoreAccountId& account_id) {
  DCHECK(HasAccountWithRefreshToken(account_id));
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  if (account_info.account_image.IsEmpty()) {
    account_info_fetch_start_times_[account_id] = base::TimeTicks::Now();
  }
  account_fetcher_service_->RefreshAccountInfoIfStale(account_id);
}

void IdentityManager::RefreshAccountInfoIfStale(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_core_account_id) {
  if (j_core_account_id) {
    RefreshAccountInfoIfStale(
        ConvertFromJavaCoreAccountId(env, j_core_account_id));
  }
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::GetPrimaryAccountInfo(JNIEnv* env, jint consent_level) const {
  CoreAccountInfo account_info =
      GetPrimaryAccountInfo(static_cast<ConsentLevel>(consent_level));
  if (account_info.IsEmpty())
    return nullptr;
  return ConvertToJavaCoreAccountInfo(env, account_info);
}

base::android::ScopedJavaLocalRef<jobject>
IdentityManager::FindExtendedAccountInfoByEmailAddress(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_email) const {
  AccountInfo account_info = FindExtendedAccountInfoByEmailAddress(
      base::android::ConvertJavaStringToUTF8(env, j_email));
  if (account_info.IsEmpty())
    return nullptr;
  return ConvertToJavaAccountInfo(env, account_info);
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

jboolean IdentityManager::IsClearPrimaryAccountAllowed(JNIEnv* env) const {
  return signin_client_->IsClearPrimaryAccountAllowed(
      HasPrimaryAccount(signin::ConsentLevel::kSync));
}
#endif

AccountInfo IdentityManager::FindExtendedPrimaryAccountInfo(
    ConsentLevel consent_level) {
  CoreAccountId account_id = GetPrimaryAccountId(consent_level);
  return account_tracker_service_->GetAccountInfo(account_id);
}

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

#if BUILDFLAG(IS_CHROMEOS)
account_manager::AccountManagerFacade*
IdentityManager::GetAccountManagerFacade() const {
  return account_manager_facade_;
}
#endif

AccountInfo IdentityManager::GetAccountInfoForAccountWithRefreshToken(
    const CoreAccountId& account_id) const {
  // TODO(crbug.com/41434401): This invariant is not currently possible to
  // enforce on Android due to the underlying relationship between
  // O2TS::GetAccounts(), O2TS::RefreshTokenIsAvailable(), and
  // O2TS::Observer::OnRefreshTokenAvailable().
#if !BUILDFLAG(IS_ANDROID)
  DCHECK(HasAccountWithRefreshToken(account_id));
#endif

  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());

  return account_info;
}

void IdentityManager::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event_details) {
  CoreAccountId event_primary_account_id =
      event_details.GetCurrentState().primary_account.account_id;
  DCHECK_EQ(event_primary_account_id,
            GetPrimaryAccountId(event_details.GetCurrentState().consent_level));
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountChanged(event_details);
    // Ensure that |observer| did not change the primary account as otherwise
    // |event_details| would not longer be correct.
    DCHECK_EQ(
        event_primary_account_id,
        GetPrimaryAccountId(event_details.GetCurrentState().consent_level));
  }

#if BUILDFLAG(IS_ANDROID)
  if (java_identity_manager_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onPrimaryAccountChanged(
        env, java_identity_manager_,
        ConvertToJavaPrimaryAccountChangeEvent(env, event_details));
  }
#endif
}

void IdentityManager::OnRefreshTokenAvailable(const CoreAccountId& account_id) {
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info);
  }
#if BUILDFLAG(IS_ANDROID)
  if (java_identity_manager_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onRefreshTokenUpdatedForAccount(
        env, java_identity_manager_,
        ConvertToJavaCoreAccountInfo(env, account_info));
  }
#endif
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
    const GoogleServiceAuthError& auth_error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  CoreAccountInfo account_info =
      GetAccountInfoForAccountWithRefreshToken(account_id);

  for (auto& observer : observer_list_)
    observer.OnErrorStateOfRefreshTokenUpdatedForAccount(
        account_info, auth_error, token_operation_source);
}

void IdentityManager::OnGaiaAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  bool succeeded = error == GoogleServiceAuthError::AuthErrorNone();
  CHECK(accounts_in_cookie_jar_info.AreAccountsFresh() == succeeded);

  for (auto& observer : observer_list_) {
    observer.OnAccountsInCookieUpdated(accounts_in_cookie_jar_info, error);
  }
}

void IdentityManager::OnGaiaCookieDeletedByUserAction() {
  for (auto& observer : observer_list_) {
    observer.OnAccountsCookieDeletedByUserAction();
  }
#if BUILDFLAG(IS_ANDROID)
  if (java_identity_manager_) {
    Java_IdentityManager_onAccountsCookieDeletedByUserAction(
        base::android::AttachCurrentThread(), java_identity_manager_);
  }
#endif
}

void IdentityManager::OnAccessTokenRequested(const CoreAccountId& account_id,
                                             const std::string& consumer_id,
                                             const ScopeSet& scopes) {
  for (auto& observer : diagnostics_observation_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

void IdentityManager::OnFetchAccessTokenComplete(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const ScopeSet& scopes,
    const GoogleServiceAuthError& error,
    base::Time expiration_time) {
  for (auto& observer : diagnostics_observation_list_)
    observer.OnAccessTokenRequestCompleted(account_id, consumer_id, scopes,
                                           error, expiration_time);
}

void IdentityManager::OnAccessTokenRemoved(const CoreAccountId& account_id,
                                           const ScopeSet& scopes) {
  for (auto& observer : diagnostics_observation_list_)
    observer.OnAccessTokenRemovedFromCache(account_id, scopes);
}

void IdentityManager::OnRefreshTokenAvailableFromSource(
    const CoreAccountId& account_id,
    bool is_refresh_token_valid,
    const std::string& source) {
  for (auto& observer : diagnostics_observation_list_)
    observer.OnRefreshTokenUpdatedForAccountFromSource(
        account_id, is_refresh_token_valid, source);
}

void IdentityManager::OnRefreshTokenRevokedFromSource(
    const CoreAccountId& account_id,
    const std::string& source) {
  for (auto& observer : diagnostics_observation_list_)
    observer.OnRefreshTokenRemovedForAccountFromSource(account_id, source);
}

void IdentityManager::OnAccountUpdated(const AccountInfo& info) {
  if (HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    const CoreAccountId primary_account_id =
        GetPrimaryAccountId(ConsentLevel::kSignin);
    if (primary_account_id == info.account_id) {
      primary_account_manager_->UpdatePrimaryAccountInfo();
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnExtendedAccountInfoUpdated(info);
  }
#if BUILDFLAG(IS_ANDROID)
  if (java_identity_manager_) {
    if (account_info_fetch_start_times_.count(info.account_id) &&
        !info.account_image.IsEmpty()) {
      base::UmaHistogramTimes(
          "Signin.AndroidAccountInfoFetchTime",
          base::TimeTicks::Now() -
              account_info_fetch_start_times_[info.account_id]);
      account_info_fetch_start_times_.erase(info.account_id);
    }
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_IdentityManager_onExtendedAccountInfoUpdated(
        env, java_identity_manager_, ConvertToJavaAccountInfo(env, info));
  }
#endif
}

void IdentityManager::OnAccountRemoved(const AccountInfo& info) {
#if (BUILDFLAG(IS_ANDROID))
    account_fetcher_service_->DestroyFetchers(info.account_id);
#endif
  for (auto& observer : observer_list_)
    observer.OnExtendedAccountInfoRemoved(info);
}

}  // namespace signin
