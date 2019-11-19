// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth2_token_service_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/signin/core/browser/android/jni_headers/OAuth2TokenService_jni.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Callback from FetchOAuth2TokenWithUsername().
// Arguments:
// - the error, or NONE if the token fetch was successful.
// - the OAuth2 access token.
// - the expiry time of the token (may be null, indicating that the expiry
//   time is unknown.
typedef base::Callback<
    void(const GoogleServiceAuthError&, const std::string&, const base::Time&)>
    FetchOAuth2TokenCallback;

class AndroidAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  AndroidAccessTokenFetcher(
      OAuth2TokenServiceDelegateAndroid* oauth2_token_service_delegate,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& account_id);
  ~AndroidAccessTokenFetcher() override;

  // Overrides from OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(const GoogleServiceAuthError& error,
                             const std::string& access_token,
                             const base::Time& expiration_time);

 private:
  std::string CombineScopes(const std::vector<std::string>& scopes);

  OAuth2TokenServiceDelegateAndroid* oauth2_token_service_delegate_;
  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<AndroidAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidAccessTokenFetcher);
};

AndroidAccessTokenFetcher::AndroidAccessTokenFetcher(
    OAuth2TokenServiceDelegateAndroid* oauth2_token_service_delegate,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& account_id)
    : OAuth2AccessTokenFetcher(consumer),
      oauth2_token_service_delegate_(oauth2_token_service_delegate),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {}

AndroidAccessTokenFetcher::~AndroidAccessTokenFetcher() {}

void AndroidAccessTokenFetcher::Start(const std::string& client_id,
                                      const std::string& client_secret,
                                      const std::vector<std::string>& scopes) {
  JNIEnv* env = AttachCurrentThread();
  std::string scope = CombineScopes(scopes);
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF8ToJavaString(env, account_id_);
  ScopedJavaLocalRef<jstring> j_scope = ConvertUTF8ToJavaString(env, scope);
  std::unique_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(
          base::Bind(&AndroidAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr())));

  // Call into Java to get a new token.
  Java_OAuth2TokenService_getAccessTokenFromNative(
      env, oauth2_token_service_delegate_->GetJavaObject(), j_username, j_scope,
      reinterpret_cast<intptr_t>(heap_callback.release()));
}

void AndroidAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void AndroidAccessTokenFetcher::OnAccessTokenResponse(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_time) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  if (error.state() == GoogleServiceAuthError::NONE) {
    FireOnGetTokenSuccess(OAuth2AccessTokenConsumer::TokenResponse(
        access_token, expiration_time, std::string()));
  } else {
    FireOnGetTokenFailure(error);
  }
}

// static
std::string AndroidAccessTokenFetcher::CombineScopes(
    const std::vector<std::string>& scopes) {
  // The Android AccountManager supports multiple scopes separated by a space:
  // https://code.google.com/p/google-api-java-client/wiki/OAuth2#Android
  std::string scope;
  for (std::vector<std::string>::const_iterator it = scopes.begin();
       it != scopes.end(); ++it) {
    if (!scope.empty())
      scope += " ";
    scope += *it;
  }
  return scope;
}

}  // namespace

// TODO(crbug.com/1009957) Remove disable_interation_with_system_accounts_
// from OAuth2TokenServiceDelegateAndroid
bool OAuth2TokenServiceDelegateAndroid::
    disable_interaction_with_system_accounts_ = false;

OAuth2TokenServiceDelegateAndroid::OAuth2TokenServiceDelegateAndroid(
    AccountTrackerService* account_tracker_service,
    const base::android::JavaRef<jobject>& account_manager_facade)
    : account_tracker_service_(account_tracker_service),
      fire_refresh_token_loaded_(RT_LOAD_NOT_START) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ctor";
  DCHECK(account_tracker_service_);

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_OAuth2TokenService_create(env, reinterpret_cast<intptr_t>(this),
                                     account_tracker_service_->GetJavaObject(),
                                     account_manager_facade);
  java_ref_.Reset(env, local_java_ref.obj());

  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    std::vector<CoreAccountId> accounts = GetAccounts();
    std::vector<CoreAccountId> accounts_id;
    for (auto account_name : accounts) {
      AccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(account_name.id);
      DCHECK(!account_info.gaia.empty());
      accounts_id.push_back(account_info.gaia);
    }
    SetAccounts(accounts_id);
  }
}

OAuth2TokenServiceDelegateAndroid::~OAuth2TokenServiceDelegateAndroid() {}

ScopedJavaLocalRef<jobject> OAuth2TokenServiceDelegateAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_ref_);
}

bool OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  DCHECK(!disable_interaction_with_system_accounts_)
      << __FUNCTION__
      << " needs to interact with system accounts and cannot be used with "
         "disable_interaction_with_system_accounts_";
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable"
           << " account= " << account_id;
  std::string account_name = MapAccountIdToAccountName(account_id);
  if (account_name.empty()) {
    // This corresponds to the case when the account with id |account_id| is not
    // present on the device and thus was not seeded.
    DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable"
             << " cannot find account name for account id " << account_id;
    return false;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_id =
      ConvertUTF8ToJavaString(env, account_name);
  jboolean refresh_token_is_available =
      Java_OAuth2TokenService_hasOAuth2RefreshToken(env, java_ref_,
                                                    j_account_id);
  return refresh_token_is_available == JNI_TRUE;
}

GoogleServiceAuthError OAuth2TokenServiceDelegateAndroid::GetAuthError(
    const CoreAccountId& account_id) const {
  auto it = errors_.find(account_id);
  return (it == errors_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                               : it->second;
}

void OAuth2TokenServiceDelegateAndroid::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAuthError"
           << " account=" << account_id << " error=" << error.ToString();

  if (error.IsTransientError())
    return;

  auto it = errors_.find(account_id);
  if (error.state() == GoogleServiceAuthError::NONE) {
    if (it == errors_.end())
      return;
    errors_.erase(it);
  } else {
    if (it != errors_.end() && it->second == error)
      return;
    errors_[account_id] = error;
  }
  FireAuthErrorChanged(account_id, error);
}

std::vector<CoreAccountId> OAuth2TokenServiceDelegateAndroid::GetAccounts()
    const {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getAccounts(env);
  ;
  // TODO(fgorski): We may decide to filter out some of the accounts.
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts,
                                                     &accounts);
  return std::vector<CoreAccountId>(accounts.begin(), accounts.end());
}

std::vector<std::string>
OAuth2TokenServiceDelegateAndroid::GetSystemAccountNames() {
  DCHECK(!disable_interaction_with_system_accounts_)
      << __FUNCTION__
      << " needs to interact with system accounts and cannot be used with "
         "disable_interaction_with_system_accounts_";
  std::vector<std::string> account_names;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getSystemAccountNames(env, java_ref_);
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts,
                                                     &account_names);
  return account_names;
}

std::vector<CoreAccountId>
OAuth2TokenServiceDelegateAndroid::GetSystemAccounts() {
  std::vector<CoreAccountId> ids;
  for (const std::string& name : GetSystemAccountNames()) {
    CoreAccountId id(MapAccountNameToAccountId(name));
    if (!id.empty())
      ids.push_back(std::move(id));
  }
  return ids;
}

std::vector<CoreAccountId>
OAuth2TokenServiceDelegateAndroid::GetValidAccounts() {
  std::vector<CoreAccountId> ids;
  for (const CoreAccountId& id : GetAccounts()) {
    if (ValidateAccountId(id))
      ids.emplace_back(id);
  }
  return ids;
}

void OAuth2TokenServiceDelegateAndroid::SetAccounts(
    const std::vector<CoreAccountId>& accounts) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<std::string> str_ids(accounts.begin(), accounts.end());
  ScopedJavaLocalRef<jobjectArray> java_accounts(
      base::android::ToJavaArrayOfStrings(env, str_ids));
  Java_OAuth2TokenService_setAccounts(env, java_accounts);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher"
           << " account= " << account_id;
  ValidateAccountId(account_id);
  std::string account_name = MapAccountIdToAccountName(account_id);
  DCHECK(!account_name.empty())
      << "Cannot find account name for account id " << account_id;
  return std::make_unique<AndroidAccessTokenFetcher>(this, consumer,
                                                     account_name);
}

void OAuth2TokenServiceDelegateAndroid::OnAccessTokenInvalidated(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  DCHECK(!disable_interaction_with_system_accounts_)
      << __FUNCTION__
      << " needs to interact with system accounts and cannot be used with "
         "disable_interaction_with_system_accounts_";
  ValidateAccountId(account_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_access_token =
      ConvertUTF8ToJavaString(env, access_token);
  Java_OAuth2TokenService_invalidateAccessToken(env, java_ref_, j_access_token);
}

void OAuth2TokenServiceDelegateAndroid::
    ReloadAllAccountsFromSystemWithPrimaryAccount(
        const base::Optional<CoreAccountId>& primary_account_id) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_account_id =
      primary_account_id.has_value()
          ? ConvertUTF8ToJavaString(env, primary_account_id->id)
          : nullptr;
  Java_OAuth2TokenService_seedAndReloadAccountsWithPrimaryAccount(
      env, java_ref_, j_account_id);
}

void OAuth2TokenServiceDelegateAndroid::
    ReloadAllAccountsWithPrimaryAccountAfterSeeding(
        JNIEnv* env,
        const base::android::JavaParamRef<jstring>& account_id) {
  base::Optional<CoreAccountId> core_account_id;
  if (account_id) {
    core_account_id = CoreAccountId();
    core_account_id->id = ConvertJavaStringToUTF8(env, account_id);
  }
  UpdateAccountList(core_account_id, GetValidAccounts(), GetSystemAccounts());
}

void OAuth2TokenServiceDelegateAndroid::UpdateAccountList(
    const base::Optional<CoreAccountId>& signed_in_account_id,
    const std::vector<CoreAccountId>& prev_ids,
    const std::vector<CoreAccountId>& curr_ids) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
           << " sigined_in_account_id="
           << (signed_in_account_id.has_value() ? signed_in_account_id->id
                                                : std::string())
           << " prev_ids=" << prev_ids.size()
           << " curr_ids=" << curr_ids.size();
  // Clear any auth errors so that client can retry to get access tokens.
  errors_.clear();

  std::vector<CoreAccountId> refreshed_ids;
  std::vector<CoreAccountId> revoked_ids;
  bool keep_accounts = UpdateAccountList(
      signed_in_account_id, prev_ids, curr_ids, &refreshed_ids, &revoked_ids);

  ScopedBatchChange batch(this);

  // Save the current accounts in the token service before calling
  // FireRefreshToken* methods.
  SetAccounts(keep_accounts ? curr_ids : std::vector<CoreAccountId>());

  for (const CoreAccountId& refreshed_id : refreshed_ids)
    FireRefreshTokenAvailable(refreshed_id);
  for (const CoreAccountId& revoked_id : revoked_ids)
    FireRefreshTokenRevoked(revoked_id);
  if (fire_refresh_token_loaded_ == RT_WAIT_FOR_VALIDATION) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_HAS_BEEN_VALIDATED;
  }

  // Clear accounts that no longer exist on device from AccountTrackerService.
  std::vector<AccountInfo> accounts_info =
      account_tracker_service_->GetAccounts();
  for (const AccountInfo& info : accounts_info) {
    if (!base::Contains(curr_ids, info.account_id))
      account_tracker_service_->RemoveAccount(info.account_id);
  }

  // No need to wait for PrimaryAccountManager to finish migration if not signed
  // in.
  if (account_tracker_service_->GetMigrationState() ==
          AccountTrackerService::MIGRATION_IN_PROGRESS &&
      !signed_in_account_id.has_value()) {
    account_tracker_service_->SetMigrationDone();
  }

  if (!last_update_accounts_time_.is_null()) {
    base::TimeDelta sample = base::Time::Now() - last_update_accounts_time_;
    UmaHistogramLongTimes("Signin.AndroidTimeBetweenUpdateAccountList", sample);
  }
  last_update_accounts_time_ = base::Time::Now();
}

bool OAuth2TokenServiceDelegateAndroid::UpdateAccountList(
    const base::Optional<CoreAccountId>& signed_in_id,
    const std::vector<CoreAccountId>& prev_ids,
    const std::vector<CoreAccountId>& curr_ids,
    std::vector<CoreAccountId>* refreshed_ids,
    std::vector<CoreAccountId>* revoked_ids) {
  bool keep_accounts =
      base::FeatureList::IsEnabled(signin::kMiceFeature) ||
      (signed_in_id.has_value() && base::Contains(curr_ids, *signed_in_id));
  if (keep_accounts) {
    // Revoke token for ids that have been removed from the device.
    for (const CoreAccountId& prev_id : prev_ids) {
      if (signed_in_id.has_value() && prev_id == *signed_in_id)
        continue;
      if (!base::Contains(curr_ids, prev_id)) {
        DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
                 << "revoked=" << prev_id;
        revoked_ids->push_back(prev_id);
      }
    }

    if (signed_in_id.has_value()) {
      // Always fire the primary signed in account first.
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
               << "refreshed=" << *signed_in_id;
      refreshed_ids->push_back(*signed_in_id);
    }
    for (const CoreAccountId& curr_id : curr_ids) {
      if (signed_in_id.has_value() && curr_id == *signed_in_id)
        continue;
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
               << "refreshed=" << curr_id;
      refreshed_ids->push_back(curr_id);
    }
  } else {
    // Revoke all ids with signed in account first.
    if (signed_in_id.has_value() && base::Contains(prev_ids, *signed_in_id)) {
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
               << "revoked=" << *signed_in_id;
      revoked_ids->push_back(*signed_in_id);
    }
    for (const CoreAccountId& prev_id : prev_ids) {
      if (signed_in_id.has_value() && prev_id == *signed_in_id)
        continue;
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAccountList:"
               << "revoked=" << prev_id;
      revoked_ids->push_back(prev_id);
    }
  }
  return keep_accounts;
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded";
  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            load_credentials_state());
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  ProfileOAuth2TokenServiceDelegate::FireRefreshTokensLoaded();
}

void OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials";
  ScopedBatchChange batch(this);
  std::vector<CoreAccountId> accounts_to_revoke = GetAccounts();

  // Clear accounts in the token service before calling
  // |FireRefreshTokenRevoked|.
  SetAccounts(std::vector<CoreAccountId>());

  for (const CoreAccountId& account : accounts_to_revoke)
    FireRefreshTokenRevoked(account);
}

void OAuth2TokenServiceDelegateAndroid::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            load_credentials_state());
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);
  if (primary_account_id.empty() &&
      !base::FeatureList::IsEnabled(signin::kMiceFeature)) {
    FireRefreshTokensLoaded();
    return;
  }
  if (fire_refresh_token_loaded_ == RT_HAS_BEEN_VALIDATED) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_WAIT_FOR_VALIDATION;
  }
}

std::string OAuth2TokenServiceDelegateAndroid::MapAccountIdToAccountName(
    const CoreAccountId& account_id) const {
  return account_tracker_service_->GetAccountInfo(account_id).email;
}

CoreAccountId OAuth2TokenServiceDelegateAndroid::MapAccountNameToAccountId(
    const std::string& account_name) const {
  CoreAccountId account_id =
      account_tracker_service_->FindAccountInfoByEmail(account_name).account_id;
  DCHECK(!account_id.empty() || account_name.empty())
      << "Can't find account id, account_name=" << account_name;
  return account_id;
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void JNI_OAuth2TokenService_OnOAuth2TokenFetched(
    JNIEnv* env,
    const JavaParamRef<jstring>& authToken,
    jboolean isTransientError,
    jlong nativeCallback) {
  std::string token;
  if (authToken)
    token = ConvertJavaStringToUTF8(env, authToken);
  std::unique_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(nativeCallback));
  GoogleServiceAuthError err = GoogleServiceAuthError::AuthErrorNone();
  if (!authToken) {
    err =
        isTransientError
            ? GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED)
            : GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_SERVER);
  }
  heap_callback->Run(err, token, base::Time());
}
