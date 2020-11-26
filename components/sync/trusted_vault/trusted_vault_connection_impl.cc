// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "base/containers/span.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/download_keys_response_handler.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"
#include "crypto/hmac.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

const size_t kMemberProofLength = 32;
const uint8_t kWrappedKeyHeader[] = {'V', '1', ' ', 's', 'h', 'a', 'r',
                                     'e', 'd', '_', 'k', 'e', 'y'};
const char kJoinSecurityDomainsURLPath[] = "/domain:join";
const char kListSecurityDomainsURLPathAndQuery[] = "/domain:list?view=1";
const char kSecurityDomainName[] = "chromesync";

// Helper function for filling protobuf bytes field: protobuf represent them as
// std::string, while in code std::vector<uint8_t> or base::span<uint8_t> is
// more common.
// TODO(crbug.com/1113598): this function and its counterpart (proto string to
// bytes vector) is useful in many places under trusted_vault directory,
// consider moving it to some utility file.
void AssignBytesToProtoString(base::span<const uint8_t> bytes,
                              std::string* bytes_proto_field) {
  *bytes_proto_field = std::string(bytes.begin(), bytes.end());
}

void ProcessRegisterDeviceResponse(
    TrustedVaultConnection::RegisterAuthenticationFactorCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  TrustedVaultRequestStatus registration_status;
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      registration_status = TrustedVaultRequestStatus::kSuccess;
      break;
    case TrustedVaultRequest::HttpStatus::kOtherError:
      registration_status = TrustedVaultRequestStatus::kOtherError;
      break;
    case TrustedVaultRequest::HttpStatus::kBadRequest:
      // Bad request response indicates that client data is outdated (e.g.
      // locally available trusted vault key is not the recent one).
      registration_status = TrustedVaultRequestStatus::kLocalDataObsolete;
  }
  std::move(callback).Run(registration_status);
}

std::vector<uint8_t> ComputeWrappedKey(
    const SecureBoxPublicKey& public_key,
    const std::vector<uint8_t>& trusted_vault_key) {
  return public_key.Encrypt(
      /*shared_secret=*/base::span<const uint8_t>(), kWrappedKeyHeader,
      /*payload=*/trusted_vault_key);
}

std::vector<uint8_t> ComputeMemberProof(
    const SecureBoxPublicKey& public_key,
    const std::vector<uint8_t>& trusted_vault_key) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(trusted_vault_key));

  std::vector<uint8_t> digest_bytes(kMemberProofLength);
  CHECK(hmac.Sign(public_key.ExportToBytes(), digest_bytes));

  return digest_bytes;
}

sync_pb::SharedKey CreateMemberSharedKey(
    int trusted_vault_key_version,
    const std::vector<uint8_t>& trusted_vault_key,
    const SecureBoxPublicKey& public_key) {
  sync_pb::SharedKey shared_key;
  shared_key.set_epoch(trusted_vault_key_version);
  AssignBytesToProtoString(ComputeWrappedKey(public_key, trusted_vault_key),
                           shared_key.mutable_wrapped_key());
  AssignBytesToProtoString(ComputeMemberProof(public_key, trusted_vault_key),
                           shared_key.mutable_member_proof());
  return shared_key;
}

sync_pb::JoinSecurityDomainsRequest CreateJoinSecurityDomainsRequest(
    int last_trusted_vault_key_version,
    const std::vector<uint8_t>& last_trusted_vault_key,
    const SecureBoxPublicKey& public_key) {
  sync_pb::SecurityDomain::Member member;
  const std::vector<uint8_t> public_key_bytes = public_key.ExportToBytes();
  AssignBytesToProtoString(public_key.ExportToBytes(),
                           member.mutable_public_key());
  *member.add_keys() = CreateMemberSharedKey(
      last_trusted_vault_key_version, last_trusted_vault_key, public_key);

  sync_pb::SecurityDomain security_domain;
  security_domain.set_name(kSecurityDomainName);
  *security_domain.add_members() = member;

  sync_pb::JoinSecurityDomainsRequest result;
  *result.add_security_domains() = security_domain;
  return result;
}

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
    const std::vector<uint8_t>& last_trusted_vault_key,
    int last_trusted_vault_key_version,
    const SecureBoxPublicKey& public_key,
    RegisterAuthenticationFactorCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kPost,
      GURL(trusted_vault_service_url_.spec() + kJoinSecurityDomainsURLPath),
      /*serialized_request_proto=*/
      CreateJoinSecurityDomainsRequest(last_trusted_vault_key_version,
                                       last_trusted_vault_key, public_key)
          .SerializeAsString());
  request->FetchAccessTokenAndSendRequest(
      account_info.account_id, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_.get(),
      base::BindOnce(ProcessRegisterDeviceResponse, std::move(callback)));
  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadKeys(
    const CoreAccountInfo& account_info,
    const std::vector<uint8_t>& last_trusted_vault_key,
    int last_trusted_vault_key_version,
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
      base::BindOnce(ProcessDownloadKeysResponse,
                     /*response_processor=*/
                     std::make_unique<DownloadKeysResponseHandler>(
                         last_trusted_vault_key, last_trusted_vault_key_version,
                         std::move(device_key_pair)),
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
