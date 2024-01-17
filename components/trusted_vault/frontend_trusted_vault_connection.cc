// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/frontend_trusted_vault_connection.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"
#include "components/trusted_vault/trusted_vault_connection_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace trusted_vault {

namespace {

// Exposes a `TrustedVaultAccessTokenFetcherImpl` while keeping ownership of
// it and its `TrustedVaultAccessTokenFetcherFrontend`.
class AccessTokenFetcherWrapper : public TrustedVaultAccessTokenFetcher {
 public:
  explicit AccessTokenFetcherWrapper(signin::IdentityManager* identity_manager)
      : identity_manager_(identity_manager),
        access_token_fetcher_frontend_(identity_manager) {}

  // TrustedVaultAccessTokenFetcher:
  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override {
    return access_token_fetcher_frontend_.FetchAccessToken(account_id,
                                                           std::move(callback));
  }

  std::unique_ptr<TrustedVaultAccessTokenFetcher> Clone() override {
    return std::make_unique<AccessTokenFetcherWrapper>(identity_manager_);
  }

 private:
  const raw_ptr<signin::IdentityManager> identity_manager_;
  TrustedVaultAccessTokenFetcherFrontend access_token_fetcher_frontend_;
};

}  // namespace

std::unique_ptr<TrustedVaultConnection> NewFrontendTrustedVaultConnection(
    SecurityDomainId security_domain,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  CHECK(identity_manager);
  CHECK(url_loader_factory);
  return std::make_unique<TrustedVaultConnectionImpl>(
      security_domain,
      trusted_vault::ExtractTrustedVaultServiceURLFromCommandLine(),
      url_loader_factory->Clone(),
      std::make_unique<AccessTokenFetcherWrapper>(identity_manager));
}

}  // namespace trusted_vault
