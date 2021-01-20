// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "google_apis/gaia/google_service_auth_error.h"

// A specialization of ProfileOAuth2TokenServiceDelegate that will be returned
// by OAuth2TokenServiceDelegateFactory for OS_ANDROID.  This instance uses
// native Android features to lookup OAuth2 tokens.
//
// See |ProfileOAuth2TokenServiceDelegate| for usage details.
//
// Note: requests should be started from the UI thread.
class ProfileOAuth2TokenServiceDelegateAndroid
    : public ProfileOAuth2TokenServiceDelegate {
 public:
  ProfileOAuth2TokenServiceDelegateAndroid(
      AccountTrackerService* account_tracker_service,
      const base::android::JavaRef<jobject>& account_manager_facade);
  ~ProfileOAuth2TokenServiceDelegateAndroid() override;

  // Creates a new instance of the ProfileOAuth2TokenServiceDelegateAndroid.
  static ProfileOAuth2TokenServiceDelegateAndroid* Create();

  // Returns a reference to the corresponding Java OAuth2TokenService object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;

  // Called by the TestingProfile class to disable account validation in
  // tests.  This prevents the token service from building the java objects
  // which require prior initialization (AccountManagerFacade)
  // TODO(crbug.com/1009957) Remove disable_interation_with_system_accounts_
  // from ProfileOAuth2TokenServiceDelegateAndroid
  static void set_disable_interaction_with_system_accounts() {
    disable_interaction_with_system_accounts_ = true;
  }

  static bool get_disable_interaction_with_system_accounts() {
    return disable_interaction_with_system_accounts_;
  }

  // ProfileOAuth2TokenServiceDelegate overrides:
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const override;
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error) override;
  std::vector<CoreAccountId> GetAccounts() const override;

  // Overridden from ProfileOAuth2TokenService to complete signout of all
  // POA2TService aware accounts.
  void RevokeAllCredentials() override;

  void LoadCredentials(const CoreAccountId& primary_account_id) override;

  void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const base::Optional<CoreAccountId>& primary_account_id) override;

  // Resumes the reload of accounts once the account seeding is complete.
  // TODO(crbug.com/934688) Once ProfileOAuth2TokenServiceDelegate.java is
  // internalized, use CoreAccountId instead of String.
  void ReloadAllAccountsWithPrimaryAccountAfterSeeding(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& account_id);

  // Takes a the signed in sync account as well as all the other
  // android account ids and check the token status of each.
  // NOTE: TokenAvailable notifications will be sent for all accounts, even if
  // they were already known. See https://crbug.com/939470 for details.
  void UpdateAccountList(
      const base::Optional<CoreAccountId>& signed_in_account_id,
      const std::vector<CoreAccountId>& prev_ids,
      const std::vector<CoreAccountId>& curr_ids);

 protected:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // Overridden from ProfileOAuth2TokenServiceDelegate to intercept token fetch
  // requests and redirect them to the Account Manager.
  void OnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override;

  // Called to notify observers when refresh tokans have been loaded.
  void FireRefreshTokensLoaded() override;

 private:
  std::string MapAccountIdToAccountName(const CoreAccountId& account_id) const;
  CoreAccountId MapAccountNameToAccountId(
      const std::string& account_name) const;

  enum RefreshTokenLoadStatus {
    RT_LOAD_NOT_START,
    RT_WAIT_FOR_VALIDATION,
    RT_HAS_BEEN_VALIDATED,
    RT_LOADED
  };

  // Return whether accounts are valid and we have access to all the tokens in
  // |curr_ids|.
  bool UpdateAccountList(const base::Optional<CoreAccountId>& signed_in_id,
                         const std::vector<CoreAccountId>& prev_ids,
                         const std::vector<CoreAccountId>& curr_ids,
                         std::vector<CoreAccountId>* refreshed_ids,
                         std::vector<CoreAccountId>* revoked_ids);

  // Lists account names at the OS level.
  std::vector<std::string> GetSystemAccountNames();
  // As |GetSystemAccountNames| but returning account IDs.
  std::vector<CoreAccountId> GetSystemAccounts();
  // As |GetAccounts| but with only validated account IDs.
  std::vector<CoreAccountId> GetValidAccounts();
  // Set accounts that have been advertised by OnRefreshTokenAvailable.
  virtual void SetAccounts(const std::vector<CoreAccountId>& accounts);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Accounts that have been advertised by OnRefreshTokenAvailable.
  std::vector<CoreAccountId> accounts_;

  // Maps account_id to the last error for that account.
  std::map<CoreAccountId, GoogleServiceAuthError> errors_;

  AccountTrackerService* account_tracker_service_;
  RefreshTokenLoadStatus fire_refresh_token_loaded_;

  // For testing, disables the creation of the java counterpart, see
  // set_disable_interaction_with_system_accounts().
  // TODO(crbug.com/1009957) Remove disable_interation_with_system_accounts_
  // from ProfileOAuth2TokenServiceDelegateAndroid
  static bool disable_interaction_with_system_accounts_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenServiceDelegateAndroid);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
