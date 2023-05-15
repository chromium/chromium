// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REGISTRATION_VERIFIER_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REGISTRATION_VERIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace trusted_vault {

class TrustedVaultRegistrationVerifier {
 public:
  TrustedVaultRegistrationVerifier(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  TrustedVaultRegistrationVerifier(const TrustedVaultRegistrationVerifier&) =
      delete;
  ~TrustedVaultRegistrationVerifier();

  TrustedVaultRegistrationVerifier& operator=(
      TrustedVaultRegistrationVerifier&) = delete;

  void VerifyMembership(const std::string& gaia_id,
                        const std::vector<uint8_t>& public_key);

 private:
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  TrustedVaultAccessTokenFetcherFrontend access_token_fetcher_frontend_;
  TrustedVaultAccessTokenFetcherImpl access_token_fetcher_;

  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_verify_registration_request_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REGISTRATION_VERIFIER_H_
