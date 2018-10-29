// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "services/identity/public/cpp/access_token_info.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(OS_ANDROID)
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#endif

namespace policy {

// The key under which the hosted-domain value is stored in the UserInfo
// response.
const char kGetHostedDomainKey[] = "hd";

typedef base::Callback<void(const std::string&)> StringCallback;

// This class fetches an OAuth2 token scoped for the userinfo and DM services.
// On Android, we use a special API to allow us to fetch a token for an account
// that is not yet logged in to allow fetching the token before the sign-in
// process is finished.
class CloudPolicyClientRegistrationHelper::IdentityManagerHelper {
 public:
  IdentityManagerHelper() = default;

  void FetchAccessToken(identity::IdentityManager* identity_manager,
                        const std::string& username,
                        const StringCallback& callback);

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  identity::AccessTokenInfo token_info);

  StringCallback callback_;
  std::unique_ptr<identity::AccessTokenFetcher> access_token_fetcher_;
};

void CloudPolicyClientRegistrationHelper::IdentityManagerHelper::
    FetchAccessToken(identity::IdentityManager* identity_manager,
                     const std::string& account_id,
                     const StringCallback& callback) {
  DCHECK(!access_token_fetcher_);
  // The caller must supply a username.
  DCHECK(!account_id.empty());
  DCHECK(identity_manager->HasAccountWithRefreshToken(account_id));

  callback_ = callback;

  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);

  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "cloud_policy", scopes,
      base::BindOnce(&CloudPolicyClientRegistrationHelper::
                         IdentityManagerHelper::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      identity::AccessTokenFetcher::Mode::kImmediate);
}

void CloudPolicyClientRegistrationHelper::IdentityManagerHelper::
    OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                               identity::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);
  access_token_fetcher_.reset();

  if (error.state() == GoogleServiceAuthError::NONE)
    callback_.Run(token_info.token);
  else
    callback_.Run("");
}

#if !defined(OS_ANDROID)
// This class fetches the OAuth2 token scoped for the userinfo and DM services.
// It uses an OAuth2AccessTokenFetcher to fetch it, given a login refresh token
// that can be used to authorize that request. This class is not needed on
// Android because we can use OAuth2TokenService to fetch tokens for accounts
// even before they are signed in.
class CloudPolicyClientRegistrationHelper::LoginTokenHelper
    : public OAuth2AccessTokenConsumer {
 public:
  LoginTokenHelper();

  void FetchAccessToken(
      const std::string& login_refresh_token,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const StringCallback& callback);

 private:
  // OAuth2AccessTokenConsumer implementation:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

  StringCallback callback_;
  std::unique_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;
};

CloudPolicyClientRegistrationHelper::LoginTokenHelper::LoginTokenHelper() {}

void CloudPolicyClientRegistrationHelper::LoginTokenHelper::FetchAccessToken(
    const std::string& login_refresh_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StringCallback& callback) {
  DCHECK(!oauth2_access_token_fetcher_);
  callback_ = callback;

  // Start fetching an OAuth2 access token for the device management and
  // userinfo services.
  oauth2_access_token_fetcher_.reset(new OAuth2AccessTokenFetcherImpl(
      this, url_loader_factory, login_refresh_token));
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  oauth2_access_token_fetcher_->Start(
      gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(),
      GetScopes());
}

void CloudPolicyClientRegistrationHelper::LoginTokenHelper::OnGetTokenSuccess(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  callback_.Run(token_response.access_token);
}

void CloudPolicyClientRegistrationHelper::LoginTokenHelper::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  callback_.Run("");
}

#endif

CloudPolicyClientRegistrationHelper::CloudPolicyClientRegistrationHelper(
    CloudPolicyClient* client,
    enterprise_management::DeviceRegisterRequest::Type registration_type)
    : client_(client), registration_type_(registration_type) {
  DCHECK(client_);
}

CloudPolicyClientRegistrationHelper::~CloudPolicyClientRegistrationHelper() {
  // Clean up any pending observers in case the browser is shutdown while
  // trying to register for policy.
  if (client_)
    client_->RemoveObserver(this);
}

void CloudPolicyClientRegistrationHelper::StartRegistration(
    identity::IdentityManager* identity_manager,
    const std::string& account_id,
    const base::Closure& callback) {
  DVLOG(1) << "Starting registration process with account_id";
  DCHECK(!client_->is_registered());
  callback_ = callback;
  client_->AddObserver(this);

  identity_manager_helper_.reset(new IdentityManagerHelper());
  identity_manager_helper_->FetchAccessToken(
      identity_manager, account_id,
      base::Bind(&CloudPolicyClientRegistrationHelper::OnTokenFetched,
                 base::Unretained(this)));
}

void CloudPolicyClientRegistrationHelper::StartRegistrationWithEnrollmentToken(
    const std::string& token,
    const std::string& client_id,
    const base::Closure& callback) {
  DVLOG(1) << "Starting registration process with enrollment token";
  DCHECK(!client_->is_registered());
  callback_ = callback;
  client_->AddObserver(this);
  client_->RegisterWithToken(token, client_id);
}

#if !defined(OS_ANDROID)
void CloudPolicyClientRegistrationHelper::StartRegistrationWithLoginToken(
    const std::string& login_refresh_token,
    const base::Closure& callback) {
  DVLOG(1) << "Starting registration process with login token";
  DCHECK(!client_->is_registered());
  callback_ = callback;
  client_->AddObserver(this);

  login_token_helper_.reset(
      new CloudPolicyClientRegistrationHelper::LoginTokenHelper());
  login_token_helper_->FetchAccessToken(
      login_refresh_token, client_->GetURLLoaderFactory(),
      base::Bind(&CloudPolicyClientRegistrationHelper::OnTokenFetched,
                 base::Unretained(this)));
}

// static
std::vector<std::string>
CloudPolicyClientRegistrationHelper::GetScopes() {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.push_back(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  return scopes;
}
#endif

void CloudPolicyClientRegistrationHelper::OnTokenFetched(
    const std::string& access_token) {
#if !defined(OS_ANDROID)
  login_token_helper_.reset();
#endif
  identity_manager_helper_.reset();

  if (access_token.empty()) {
    DLOG(WARNING) << "Could not fetch access token for "
                  << GaiaConstants::kDeviceManagementServiceOAuth;
    RequestCompleted();
    return;
  }

  // Cache the access token to be used after the GetUserInfo call.
  oauth_access_token_ = access_token;
  DVLOG(1) << "Fetched new scoped OAuth token:" << oauth_access_token_;
  // Now we've gotten our access token - contact GAIA to see if this is a
  // hosted domain.
  user_info_fetcher_.reset(
      new UserInfoFetcher(this, client_->GetURLLoaderFactory()));
  user_info_fetcher_->Start(oauth_access_token_);
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Failed to fetch user info from GAIA: " << error.state();
  user_info_fetcher_.reset();
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnGetUserInfoSuccess(
    const base::DictionaryValue* data) {
  user_info_fetcher_.reset();
  if (!data->HasKey(kGetHostedDomainKey)) {
    DVLOG(1) << "User not from a hosted domain - skipping registration";
    RequestCompleted();
    return;
  }
  DVLOG(1) << "Registering CloudPolicyClient for user from hosted domain";
  // The user is from a hosted domain, so it's OK to register the
  // CloudPolicyClient and make requests to DMServer.
  if (client_->is_registered()) {
    // Client should not be registered yet.
    NOTREACHED();
    RequestCompleted();
    return;
  }

  // Kick off registration of the CloudPolicyClient with our newly minted
  // oauth_access_token_.
  client_->Register(
      registration_type_,
      enterprise_management::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION,
      enterprise_management::DeviceRegisterRequest::LIFETIME_INDEFINITE,
      enterprise_management::LicenseType::UNDEFINED,
      DMAuth::FromOAuthToken(oauth_access_token_), std::string(), std::string(),
      std::string());
}

void CloudPolicyClientRegistrationHelper::OnPolicyFetched(
    CloudPolicyClient* client) {
  // Ignored.
}

void CloudPolicyClientRegistrationHelper::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration succeeded";
  DCHECK_EQ(client, client_);
  DCHECK(client->is_registered());
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::OnClientError(
    CloudPolicyClient* client) {
  DVLOG(1) << "Client registration failed";
  DCHECK_EQ(client, client_);
  RequestCompleted();
}

void CloudPolicyClientRegistrationHelper::RequestCompleted() {
  if (client_) {
    client_->RemoveObserver(this);
    // |client_| may be freed by the callback so clear it now.
    client_ = nullptr;
    callback_.Run();
  }
}

}  // namespace policy
