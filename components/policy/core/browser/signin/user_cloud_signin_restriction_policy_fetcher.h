// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/policy_export.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

struct CoreAccountId;

namespace network {
class SimpleURLLoader;
}

namespace signin {
class AccessTokenFetcher;
class IdentityManager;
}  // namespace signin

namespace policy {

class BrowserPolicyConnector;
class ProfileSeparationPolicies;

class POLICY_EXPORT UserCloudSigninRestrictionPolicyFetcher {
 public:
  UserCloudSigninRestrictionPolicyFetcher(
      policy::BrowserPolicyConnector* browser_policy_connector,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~UserCloudSigninRestrictionPolicyFetcher();

  UserCloudSigninRestrictionPolicyFetcher(
      const UserCloudSigninRestrictionPolicyFetcher&) = delete;
  UserCloudSigninRestrictionPolicyFetcher& operator=(
      const UserCloudSigninRestrictionPolicyFetcher&) = delete;

  // Fetched the value of the ManagedAccountsSigninRestriction set at the
  // account level for `account_id` and calls `callback` with the resulting
  // value. If the policy was not set or we were not able to retrieve it, the
  // result will be an empty string.
  void GetManagedAccountsSigninRestriction(
      signin::IdentityManager* identity_manager,
      const CoreAccountId& account_id,
      base::OnceCallback<void(const ProfileSeparationPolicies&)> callback,
      const std::string& response_for_testing = std::string());

  void SetURLLoaderFactoryForTesting(
      network::mojom::URLLoaderFactory* factory) {
    url_loader_factory_for_testing_ = factory;
  }

 private:
  // Fetches an access token for `accound_id` and calls `callback` with the
  // access token.
  void FetchAccessToken(signin::IdentityManager* identity_manager,
                        const CoreAccountId& account_id,
                        base::OnceCallback<void(const std::string&)> callback);

  void OnFetchAccessTokenResult(
      base::OnceCallback<void(const std::string&)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo token_info);

  // Calls the SecureConnect API to get the ManagedAccountsSigninRestriction
  // policy using `access_token` for the authentication. Calls
  // `OnManagedAccountsSigninRestrictionResult` with the result from the API.
  void GetManagedAccountsSigninRestrictionInternal(
      base::OnceCallback<void(const ProfileSeparationPolicies&)> callback,
      const std::string& access_token);

  // Retrieves the policy value from `response_body` and calls `callback` with
  // that value.
  void OnManagedAccountsSigninRestrictionResult(
      base::OnceCallback<void(const ProfileSeparationPolicies&)> callback,
      std::unique_ptr<std::string> response_body);

  GURL GetSecureConnectApiGetAccountSigninRestrictionUrl() const;

  const raw_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_for_testing_ =
      nullptr;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  //  namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_SIGNIN_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_
