// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_REGISTER_AUTHENTICATION_FACTOR_REQUEST_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_REGISTER_AUTHENTICATION_FACTOR_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {

class TrustedVaultAccessTokenFetcher;

// This class supports registration of the authentication factor within trusted
// vault server. Registration may require sending two requests to the server, if
// trusted vault key (constant key is used) is not yet know
// (ListSecurityDomainsRequest to retrieve the server-side state and current key
// version, then JoinSecurityDomainsRequest to actually register
// the authentication factor). If trusted vault key is already known, only
// JoinSecurityDomainsRequest will be issued.
class RegisterAuthenticationFactorRequest
    : public TrustedVaultConnection::Request {
 public:
  // |access_token_fetcher| must not be null and must outlive this object.
  RegisterAuthenticationFactorRequest(
      const GURL& join_security_domains_url,
      const GURL& list_security_domains_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const CoreAccountId& account_id,
      const SecureBoxPublicKey& authentication_factor_public_key,
      TrustedVaultAccessTokenFetcher* access_token_fetcher);
  RegisterAuthenticationFactorRequest(
      const RegisterAuthenticationFactorRequest& other) = delete;
  RegisterAuthenticationFactorRequest& operator=(
      RegisterAuthenticationFactorRequest& other) = delete;
  ~RegisterAuthenticationFactorRequest() override;

  void StartWithConstantKey(
      TrustedVaultConnection::RegisterAuthenticationFactorCallback
          completion_callback);
  void StartWithKnownTrustedVaultKeyAndVersion(
      const TrustedVaultKeyAndVersion& trusted_vault_key_and_version,
      TrustedVaultConnection::RegisterAuthenticationFactorCallback
          completion_callback);

 private:
  void StartJoinSecurityDomainsRequest(
      const TrustedVaultKeyAndVersion& trusted_vault_key_and_version);
  void OnListSecurityDomainsCompleted(
      TrustedVaultRequest::HttpStatus http_status,
      const std::string& response_body);
  void OnJoinSecurityDomainsResponse(
      TrustedVaultRequest::HttpStatus http_status,
      const std::string& response_body);

  void RunCallbackAndMaybeDestroySelf(TrustedVaultRequestStatus status);

  const GURL join_security_domains_url_;
  const GURL list_security_domains_url_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const CoreAccountId account_id_;
  const std::unique_ptr<SecureBoxPublicKey> authentication_factor_public_key_;
  TrustedVaultAccessTokenFetcher* const access_token_fetcher_;

  std::unique_ptr<TrustedVaultRequest> ongoing_request_;
  TrustedVaultConnection::RegisterAuthenticationFactorCallback
      completion_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_REGISTER_AUTHENTICATION_FACTOR_REQUEST_H_
