// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "base/containers/span.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/download_keys_response_handler.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/register_authentication_factor_request.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

const char kJoinSecurityDomainsURLPath[] = "/domain:join";
const char kListSecurityDomainsURLPathAndQuery[] = "/domain:list?view=1";

void ProcessDownloadKeysResponse(
    std::unique_ptr<DownloadKeysResponseHandler> response_handler,
    TrustedVaultConnection::DownloadKeysCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  DownloadKeysResponseHandler::ProcessedResponse processed_response =
      response_handler->ProcessResponse(http_status, response_body);
  std::move(callback).Run(processed_response.status, processed_response.keys,
                          processed_response.last_key_version);
}

}  // namespace

TrustedVaultConnectionImpl::TrustedVaultConnectionImpl(
    const GURL& trusted_vault_service_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher)
    : pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      access_token_fetcher_(std::move(access_token_fetcher)),
      trusted_vault_service_url_(trusted_vault_service_url) {
  DCHECK(trusted_vault_service_url_.is_valid());
}

TrustedVaultConnectionImpl::~TrustedVaultConnectionImpl() = default;

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::RegisterAuthenticationFactor(
    const CoreAccountInfo& account_info,
    const base::Optional<TrustedVaultKeyAndVersion>&
        last_trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key,
    RegisterAuthenticationFactorCallback callback) {
  auto request = std::make_unique<RegisterAuthenticationFactorRequest>(
      /*join_security_domains_url=*/GURL(trusted_vault_service_url_.spec() +
                                         kJoinSecurityDomainsURLPath),
      /*list_security_domains_url=*/
      GURL(trusted_vault_service_url_.spec() +
           kListSecurityDomainsURLPathAndQuery),
      GetOrCreateURLLoaderFactory(), account_info.account_id, public_key,
      access_token_fetcher_.get());
  if (last_trusted_vault_key_and_version.has_value()) {
    request->StartWithKnownTrustedVaultKeyAndVersion(
        *last_trusted_vault_key_and_version, std::move(callback));
  } else {
    request->StartWithConstantKey(std::move(callback));
  }
  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadKeys(
    const CoreAccountInfo& account_info,
    const base::Optional<TrustedVaultKeyAndVersion>&
        last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadKeysCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kGet,
      GURL(trusted_vault_service_url_.spec() +
           kListSecurityDomainsURLPathAndQuery),
      /*serialized_request_proto=*/base::nullopt);

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_.get(),
      base::BindOnce(
          ProcessDownloadKeysResponse,
          /*response_processor=*/
          std::make_unique<DownloadKeysResponseHandler>(
              last_trusted_vault_key_and_version, std::move(device_key_pair)),
          std::move(callback)));

  return request;
}

scoped_refptr<network::SharedURLLoaderFactory>
TrustedVaultConnectionImpl::GetOrCreateURLLoaderFactory() {
  if (!url_loader_factory_) {
    url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory_));
  }
  return url_loader_factory_;
}

}  // namespace syncer
