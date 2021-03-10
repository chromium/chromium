// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/download_keys_response_handler.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

sync_pb::SharedMemberKey CreateSharedMemberKey(
    const base::Optional<TrustedVaultKeyAndVersion>&
        trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key) {
  std::vector<uint8_t> trusted_vault_key;
  if (trusted_vault_key_and_version.has_value()) {
    trusted_vault_key = trusted_vault_key_and_version->key;
  } else {
    trusted_vault_key = GetConstantTrustedVaultKey();
  }

  sync_pb::SharedMemberKey shared_member_key;
  if (trusted_vault_key_and_version.has_value()) {
    shared_member_key.set_epoch(trusted_vault_key_and_version->version);
  }
  AssignBytesToProtoString(
      ComputeTrustedVaultWrappedKey(public_key, trusted_vault_key),
      shared_member_key.mutable_wrapped_key());
  AssignBytesToProtoString(
      ComputeTrustedVaultHMAC(
          /*key=*/trusted_vault_key, /*data=*/public_key.ExportToBytes()),
      shared_member_key.mutable_member_proof());
  return shared_member_key;
}

sync_pb::SecurityDomainMember CreateSecurityDomainMember(
    const SecureBoxPublicKey& public_key,
    AuthenticationFactorType authentication_factor_type) {
  sync_pb::SecurityDomainMember member;
  std::string public_key_string;
  AssignBytesToProtoString(public_key.ExportToBytes(), &public_key_string);

  std::string encoded_public_key;
  base::Base64UrlEncode(public_key_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_public_key);

  member.set_name(kSecurityDomainMemberNamePrefix + encoded_public_key);
  // Note: |public_key_string| using here is intentional, encoding is required
  // only to compute member name.
  member.set_public_key(public_key_string);
  switch (authentication_factor_type) {
    case AuthenticationFactorType::kPhysicalDevice:
      member.set_member_type(
          sync_pb::SecurityDomainMember::MEMBER_TYPE_PHYSICAL_DEVICE);
  }
  return member;
}

sync_pb::JoinSecurityDomainsRequest CreateJoinSecurityDomainsRequest(
    const base::Optional<TrustedVaultKeyAndVersion>&
        last_trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key,
    AuthenticationFactorType authentication_factor_type) {
  sync_pb::JoinSecurityDomainsRequest request;
  request.mutable_security_domain()->set_name(kSyncSecurityDomainName);
  *request.mutable_security_domain_member() =
      CreateSecurityDomainMember(public_key, authentication_factor_type);
  *request.mutable_shared_member_key() =
      CreateSharedMemberKey(last_trusted_vault_key_and_version, public_key);
  return request;
}

void ProcessRegisterAuthenticationFactorRequest(
    TrustedVaultConnection::RegisterAuthenticationFactorCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      std::move(callback).Run(TrustedVaultRequestStatus::kSuccess);
      return;
    case TrustedVaultRequest::HttpStatus::kOtherError:
      std::move(callback).Run(TrustedVaultRequestStatus::kOtherError);
      return;
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kFailedPrecondition:
      // Local trusted vault keys are outdated.
      std::move(callback).Run(TrustedVaultRequestStatus::kLocalDataObsolete);
      return;
  }
  NOTREACHED();
}

void ProcessDownloadKeysResponse(
    std::unique_ptr<DownloadKeysResponseHandler> response_handler,
    TrustedVaultConnection::DownloadNewKeysCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  DownloadKeysResponseHandler::ProcessedResponse processed_response =
      response_handler->ProcessResponse(http_status, response_body);
  std::move(callback).Run(processed_response.status,
                          processed_response.new_keys,
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
    AuthenticationFactorType authentication_factor_type,
    RegisterAuthenticationFactorCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kPost,
      GURL(trusted_vault_service_url_.spec() + kJoinSecurityDomainsURLPath),
      /*serialized_request_proto=*/
      CreateJoinSecurityDomainsRequest(last_trusted_vault_key_and_version,
                                       public_key, authentication_factor_type)
          .SerializeAsString());

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_.get(),
      base::BindOnce(&ProcessRegisterAuthenticationFactorRequest,
                     std::move(callback)));
  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadNewKeys(
    const CoreAccountInfo& account_info,
    const base::Optional<TrustedVaultKeyAndVersion>&
        last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadNewKeysCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kGet,
      GURL(trusted_vault_service_url_.spec() +
           GetGetSecurityDomainMemberURLPathAndQuery(
               device_key_pair->public_key().ExportToBytes())),
      /*serialized_request_proto=*/base::nullopt);

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_.get(),
      base::BindOnce(
          &ProcessDownloadKeysResponse,
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
