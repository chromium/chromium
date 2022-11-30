// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/files/important_file_writer.h"
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

// Returns security domain epoch if valid (>0) and nullopt otherwise.
absl::optional<int> GetLastKeyVersionFromJoinSecurityDomainsResponse(
    const sync_pb::JoinSecurityDomainsResponse response) {
  if (response.security_domain().current_epoch() > 0) {
    return response.security_domain().current_epoch();
  }
  return absl::nullopt;
}

// Returns security domain epoch if input is a valid response for already exists
// error case and nullopt otherwise.
absl::optional<int> GetLastKeyVersionFromAlreadyExistsResponse(
    const std::string& response_body) {
  sync_pb::RPCStatus rpc_status;
  rpc_status.ParseFromString(response_body);
  for (const sync_pb::Proto3Any& status_detail : rpc_status.details()) {
    if (status_detail.type_url() != kJoinSecurityDomainsErrorDetailTypeURL) {
      continue;
    }
    sync_pb::JoinSecurityDomainsErrorDetail error_detail;
    error_detail.ParseFromString(status_detail.value());
    return GetLastKeyVersionFromJoinSecurityDomainsResponse(
        error_detail.already_exists_response());
  }
  return absl::nullopt;
}

std::vector<TrustedVaultKeyAndVersion> GetTrustedVaultKeysWithVersions(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_trusted_vault_key_version) {
  const int first_key_version = last_trusted_vault_key_version -
                                static_cast<int>(trusted_vault_keys.size()) + 1;
  std::vector<TrustedVaultKeyAndVersion> result;
  for (size_t i = 0; i < trusted_vault_keys.size(); ++i) {
    result.emplace_back(trusted_vault_keys[i], first_key_version + i);
  }
  return result;
}

sync_pb::SharedMemberKey CreateSharedMemberKey(
    const TrustedVaultKeyAndVersion& trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key) {
  sync_pb::SharedMemberKey shared_member_key;
  shared_member_key.set_epoch(trusted_vault_key_and_version.version);

  const std::vector<uint8_t>& trusted_vault_key =
      trusted_vault_key_and_version.key;
  AssignBytesToProtoString(
      ComputeTrustedVaultWrappedKey(public_key, trusted_vault_key),
      shared_member_key.mutable_wrapped_key());
  AssignBytesToProtoString(ComputeMemberProof(public_key, trusted_vault_key),
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
      break;
    case AuthenticationFactorType::kUnspecified:
      member.set_member_type(
          sync_pb::SecurityDomainMember::MEMBER_TYPE_UNSPECIFIED);
      break;
  }
  return member;
}

sync_pb::JoinSecurityDomainsRequest CreateJoinSecurityDomainsRequest(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_trusted_vault_key_version,
    const SecureBoxPublicKey& public_key,
    AuthenticationFactorType authentication_factor_type,
    absl::optional<int> authentication_factor_type_hint) {
  sync_pb::JoinSecurityDomainsRequest request;
  request.mutable_security_domain()->set_name(kSyncSecurityDomainName);
  *request.mutable_security_domain_member() =
      CreateSecurityDomainMember(public_key, authentication_factor_type);
  for (const TrustedVaultKeyAndVersion& trusted_vault_key_and_version :
       GetTrustedVaultKeysWithVersions(trusted_vault_keys,
                                       last_trusted_vault_key_version)) {
    *request.add_shared_member_key() =
        CreateSharedMemberKey(trusted_vault_key_and_version, public_key);
  }
  if (authentication_factor_type_hint.has_value()) {
    request.set_member_type_hint(authentication_factor_type_hint.value());
  }
  return request;
}

void RunRegisterAuthenticationFactorCallback(
    TrustedVaultConnection::RegisterAuthenticationFactorCallback callback,
    TrustedVaultRegistrationStatus status,
    int last_key_version) {
  std::move(callback).Run(status);
}

void RunRegisterDeviceWithoutKeysCallback(
    TrustedVaultConnection::RegisterDeviceWithoutKeysCallback callback,
    TrustedVaultRegistrationStatus status,
    int last_key_version) {
  std::move(callback).Run(
      status, TrustedVaultKeyAndVersion{GetConstantTrustedVaultKey(),
                                        last_key_version});
}

void ProcessJoinSecurityDomainsResponse(
    TrustedVaultConnectionImpl::JoinSecurityDomainsCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
    case TrustedVaultRequest::HttpStatus::kConflict:
      break;
    case TrustedVaultRequest::HttpStatus::kOtherError:
      std::move(callback).Run(TrustedVaultRegistrationStatus::kOtherError,
                              /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::kAccessTokenFetchingFailure:
      std::move(callback).Run(
          TrustedVaultRegistrationStatus::kAccessTokenFetchingFailure,
          /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kBadRequest:
      // Local trusted vault keys are outdated.
      std::move(callback).Run(
          TrustedVaultRegistrationStatus::kLocalDataObsolete,
          /*last_key_version=*/0);
      return;
  }

  absl::optional<int> last_key_version;
  if (http_status == TrustedVaultRequest::HttpStatus::kConflict) {
    last_key_version =
        GetLastKeyVersionFromAlreadyExistsResponse(response_body);
  } else {
    sync_pb::JoinSecurityDomainsResponse response;
    response.ParseFromString(response_body);
    last_key_version =
        GetLastKeyVersionFromJoinSecurityDomainsResponse(response);
  }

  if (!last_key_version.has_value()) {
    std::move(callback).Run(TrustedVaultRegistrationStatus::kOtherError,
                            /*last_key_version=*/0);
    return;
  }
  std::move(callback).Run(
      http_status == TrustedVaultRequest::HttpStatus::kConflict
          ? TrustedVaultRegistrationStatus::kAlreadyRegistered
          : TrustedVaultRegistrationStatus::kSuccess,
      *last_key_version);
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

void ProcessDownloadIsRecoverabilityDegradedResponse(
    TrustedVaultConnection::IsRecoverabilityDegradedCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  // TODO(crbug.com/1201659): consider special handling when security domain
  // doesn't exist.
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      break;
    case TrustedVaultRequest::HttpStatus::kOtherError:
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kBadRequest:
    case TrustedVaultRequest::HttpStatus::kConflict:
    case TrustedVaultRequest::HttpStatus::kAccessTokenFetchingFailure:
      std::move(callback).Run(TrustedVaultRecoverabilityStatus::kError);
      return;
  }
  sync_pb::SecurityDomain security_domain;
  if (!security_domain.ParseFromString(response_body) ||
      !security_domain.security_domain_details().has_sync_details()) {
    std::move(callback).Run(TrustedVaultRecoverabilityStatus::kError);
    return;
  }
  TrustedVaultRecoverabilityStatus status =
      TrustedVaultRecoverabilityStatus::kNotDegraded;
  if (security_domain.security_domain_details()
          .sync_details()
          .degraded_recoverability()) {
    status = TrustedVaultRecoverabilityStatus::kDegraded;
  }
  std::move(callback).Run(status);
}

TrustedVaultURLFetchReasonForUMA
GetURLFetchReasonForUMAForJoinSecurityDomainsRequest(
    AuthenticationFactorType authentication_factor_type) {
  switch (authentication_factor_type) {
    case AuthenticationFactorType::kPhysicalDevice:
      return TrustedVaultURLFetchReasonForUMA::kRegisterDevice;
    case AuthenticationFactorType::kUnspecified:
      return TrustedVaultURLFetchReasonForUMA::
          kRegisterUnspecifiedAuthenticationFactor;
  }

  NOTREACHED();
  return TrustedVaultURLFetchReasonForUMA::kUnspecified;
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
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_trusted_vault_key_version,
    const SecureBoxPublicKey& authentication_factor_public_key,
    AuthenticationFactorType authentication_factor_type,
    absl::optional<int> authentication_factor_type_hint,
    RegisterAuthenticationFactorCallback callback) {
  return SendJoinSecurityDomainsRequest(
      account_info, trusted_vault_keys, last_trusted_vault_key_version,
      authentication_factor_public_key, authentication_factor_type,
      authentication_factor_type_hint,
      base::BindOnce(&RunRegisterAuthenticationFactorCallback,
                     std::move(callback)));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::RegisterDeviceWithoutKeys(
    const CoreAccountInfo& account_info,
    const SecureBoxPublicKey& device_public_key,
    RegisterDeviceWithoutKeysCallback callback) {
  return SendJoinSecurityDomainsRequest(
      account_info, /*trusted_vault_keys=*/{GetConstantTrustedVaultKey()},
      /*last_trusted_vault_key_version=*/kUnknownConstantKeyVersion,
      device_public_key, AuthenticationFactorType::kPhysicalDevice,
      /*authentication_factor_type_hint=*/absl::nullopt,
      base::BindOnce(&RunRegisterDeviceWithoutKeysCallback,
                     std::move(callback)));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadNewKeys(
    const CoreAccountInfo& account_info,
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadNewKeysCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kGet,
      GURL(trusted_vault_service_url_.spec() +
           GetGetSecurityDomainMemberURLPathAndQuery(
               device_key_pair->public_key().ExportToBytes())),
      /*serialized_request_proto=*/absl::nullopt, GetOrCreateURLLoaderFactory(),
      TrustedVaultURLFetchReasonForUMA::kDownloadKeys);

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, access_token_fetcher_.get(),
      base::BindOnce(
          &ProcessDownloadKeysResponse,
          /*response_processor=*/
          std::make_unique<DownloadKeysResponseHandler>(
              last_trusted_vault_key_and_version, std::move(device_key_pair)),
          std::move(callback)));

  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    IsRecoverabilityDegradedCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kGet,
      GURL(trusted_vault_service_url_.spec() +
           kGetSecurityDomainURLPathAndQuery),
      /*serialized_request_proto=*/absl::nullopt, GetOrCreateURLLoaderFactory(),
      TrustedVaultURLFetchReasonForUMA::kDownloadIsRecoverabilityDegraded);

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, access_token_fetcher_.get(),
      base::BindOnce(&ProcessDownloadIsRecoverabilityDegradedResponse,
                     std::move(callback)));

  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::SendJoinSecurityDomainsRequest(
    const CoreAccountInfo& account_info,
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_trusted_vault_key_version,
    const SecureBoxPublicKey& authentication_factor_public_key,
    AuthenticationFactorType authentication_factor_type,
    absl::optional<int> authentication_factor_type_hint,
    JoinSecurityDomainsCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kPost,
      GURL(trusted_vault_service_url_.spec() + kJoinSecurityDomainsURLPath),
      /*serialized_request_proto=*/
      CreateJoinSecurityDomainsRequest(
          trusted_vault_keys, last_trusted_vault_key_version,
          authentication_factor_public_key, authentication_factor_type,
          authentication_factor_type_hint)
          .SerializeAsString(),
      GetOrCreateURLLoaderFactory(),
      GetURLFetchReasonForUMAForJoinSecurityDomainsRequest(
          authentication_factor_type));

  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, access_token_fetcher_.get(),
      base::BindOnce(&ProcessJoinSecurityDomainsResponse, std::move(callback)));
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
