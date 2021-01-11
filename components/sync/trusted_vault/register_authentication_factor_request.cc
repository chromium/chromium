// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/register_authentication_factor_request.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

// TODO(crbug.com/1113598): Find a good place for kSecurityDomainName constant,
// which is used in several files.
const char kSecurityDomainName[] = "chromesync";

std::vector<uint8_t> CreateConstantTrustedVaultKey() {
  return std::vector<uint8_t>(16, 0);
}

// Returns pointer to sync security domain in |response|. Returns nullptr if
// there is no sync security domain.
const sync_pb::SecurityDomain* FindSyncSecurityDomain(
    const sync_pb::ListSecurityDomainsResponse& response) {
  for (const sync_pb::SecurityDomain& security_domain :
       response.security_domains()) {
    if (security_domain.name() == kSecurityDomainName) {
      return &security_domain;
    }
  }
  return nullptr;
}

sync_pb::SharedKey CreateMemberSharedKey(
    const TrustedVaultKeyAndVersion& trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key) {
  sync_pb::SharedKey shared_key;
  shared_key.set_epoch(trusted_vault_key_and_version.version);
  AssignBytesToProtoString(ComputeTrustedVaultWrappedKey(
                               public_key, trusted_vault_key_and_version.key),
                           shared_key.mutable_wrapped_key());
  AssignBytesToProtoString(
      ComputeTrustedVaultHMAC(/*key=*/trusted_vault_key_and_version.key,
                              /*data=*/public_key.ExportToBytes()),
      shared_key.mutable_member_proof());
  return shared_key;
}

sync_pb::JoinSecurityDomainsRequest CreateJoinSecurityDomainsRequest(
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key) {
  sync_pb::SecurityDomain::Member member;
  const std::vector<uint8_t> public_key_bytes = public_key.ExportToBytes();
  AssignBytesToProtoString(public_key.ExportToBytes(),
                           member.mutable_public_key());
  *member.add_keys() =
      CreateMemberSharedKey(last_trusted_vault_key_and_version, public_key);

  sync_pb::SecurityDomain security_domain;
  security_domain.set_name(kSecurityDomainName);
  *security_domain.add_members() = member;

  sync_pb::JoinSecurityDomainsRequest result;
  *result.add_security_domains() = security_domain;
  return result;
}

}  // namespace

RegisterAuthenticationFactorRequest::RegisterAuthenticationFactorRequest(
    const GURL& join_security_domains_url,
    const GURL& list_security_domains_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const CoreAccountId& account_id,
    const SecureBoxPublicKey& authentication_factor_public_key,
    TrustedVaultAccessTokenFetcher* access_token_fetcher)
    : join_security_domains_url_(join_security_domains_url),
      list_security_domains_url_(list_security_domains_url),
      url_loader_factory_(url_loader_factory),
      account_id_(account_id),
      // TODO(crbug.com/1101813): Support copy or move semantic for
      // SecureBoxPublicKey.
      authentication_factor_public_key_(SecureBoxPublicKey::CreateByImport(
          authentication_factor_public_key.ExportToBytes())),
      access_token_fetcher_(access_token_fetcher) {
  DCHECK(access_token_fetcher_);
}

RegisterAuthenticationFactorRequest::~RegisterAuthenticationFactorRequest() =
    default;

void RegisterAuthenticationFactorRequest::StartWithConstantKey(
    TrustedVaultConnection::RegisterAuthenticationFactorCallback
        completion_callback) {
  DCHECK(!ongoing_request_);
  completion_callback_ = std::move(completion_callback);
  ongoing_request_ = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kGet, list_security_domains_url_,
      /*serialized_request_proto=*/base::nullopt);

  // Using base::Unretained() is safe here, because |this| will outlive
  // |ongoing_request_|.
  ongoing_request_->FetchAccessTokenAndSendRequest(
      account_id_, url_loader_factory_, access_token_fetcher_,
      base::BindOnce(
          &RegisterAuthenticationFactorRequest::OnListSecurityDomainsCompleted,
          base::Unretained(this)));
}

void RegisterAuthenticationFactorRequest::
    StartWithKnownTrustedVaultKeyAndVersion(
        const TrustedVaultKeyAndVersion& trusted_vault_key_and_version,
        TrustedVaultConnection::RegisterAuthenticationFactorCallback
            completion_callback) {
  completion_callback_ = std::move(completion_callback);
  StartJoinSecurityDomainsRequest(trusted_vault_key_and_version);
}

void RegisterAuthenticationFactorRequest::StartJoinSecurityDomainsRequest(
    const TrustedVaultKeyAndVersion& trusted_vault_key_and_version) {
  DCHECK(!ongoing_request_);
  ongoing_request_ = std::make_unique<TrustedVaultRequest>(
      TrustedVaultRequest::HttpMethod::kPost, join_security_domains_url_,
      /*serialized_request_proto=*/
      CreateJoinSecurityDomainsRequest(trusted_vault_key_and_version,
                                       *authentication_factor_public_key_)
          .SerializeAsString());

  // Using base::Unretained() is safe here, because |this| will outlive
  // |ongoing_request_|.
  ongoing_request_->FetchAccessTokenAndSendRequest(
      account_id_, url_loader_factory_, access_token_fetcher_,
      base::BindOnce(
          &RegisterAuthenticationFactorRequest::OnJoinSecurityDomainsResponse,
          base::Unretained(this)));
}

void RegisterAuthenticationFactorRequest::OnListSecurityDomainsCompleted(
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  DCHECK(ongoing_request_);
  ongoing_request_ = nullptr;

  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      break;
    case TrustedVaultRequest::HttpStatus::kOtherError:
    case TrustedVaultRequest::HttpStatus::kBadRequest:
      // Don't distinguish kBadRequest here, because JoinSecurityDomains request
      // content doesn't depend on the local state.
      RunCallbackAndMaybeDestroySelf(TrustedVaultRequestStatus::kOtherError);
      return;
  }

  sync_pb::ListSecurityDomainsResponse deserialized_response;
  if (!deserialized_response.ParseFromString(response_body)) {
    RunCallbackAndMaybeDestroySelf(TrustedVaultRequestStatus::kOtherError);
    return;
  }

  const sync_pb::SecurityDomain* sync_security_domain =
      FindSyncSecurityDomain(deserialized_response);
  if (!sync_security_domain) {
    // Sync security domain doesn't exist yet, |version| should be set to 0.
    StartJoinSecurityDomainsRequest(TrustedVaultKeyAndVersion(
        /*key=*/CreateConstantTrustedVaultKey(), /*version=*/0));
    return;
  }

  const sync_pb::SecurityDomain::Member* member_with_largest_epoch = nullptr;
  const sync_pb::SharedKey* shared_key_with_largest_epoch = nullptr;
  int largest_epoch = 0;
  for (const sync_pb::SecurityDomain::Member& member :
       sync_security_domain->members()) {
    for (const sync_pb::SharedKey& shared_key : member.keys()) {
      if (shared_key.epoch() > largest_epoch) {
        largest_epoch = shared_key.epoch();
        member_with_largest_epoch = &member;
        shared_key_with_largest_epoch = &shared_key;
      }
    }
  }

  if (!member_with_largest_epoch) {
    // Security domain doesn't contain any member with a valid shared key,
    // |version| should be set to 0.
    StartJoinSecurityDomainsRequest(TrustedVaultKeyAndVersion(
        /*key=*/CreateConstantTrustedVaultKey(), /*version=*/0));
    return;
  }

  DCHECK(shared_key_with_largest_epoch);
  if (!VerifyTrustedVaultHMAC(
          /*key=*/CreateConstantTrustedVaultKey(),
          /*data=*/ProtoStringToBytes(member_with_largest_epoch->public_key()),
          /*digest=*/
          ProtoStringToBytes(shared_key_with_largest_epoch->member_proof()))) {
    // Latest trusted vault key isn't constant one.
    RunCallbackAndMaybeDestroySelf(
        TrustedVaultRequestStatus::kLocalDataObsolete);
    return;
  }

  StartJoinSecurityDomainsRequest(TrustedVaultKeyAndVersion(
      /*key=*/CreateConstantTrustedVaultKey(),
      /*version=*/shared_key_with_largest_epoch->epoch()));
}

void RegisterAuthenticationFactorRequest::OnJoinSecurityDomainsResponse(
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      RunCallbackAndMaybeDestroySelf(TrustedVaultRequestStatus::kSuccess);
      return;
    case TrustedVaultRequest::HttpStatus::kOtherError:
      RunCallbackAndMaybeDestroySelf(TrustedVaultRequestStatus::kOtherError);
      return;
    case TrustedVaultRequest::HttpStatus::kBadRequest:
      RunCallbackAndMaybeDestroySelf(
          TrustedVaultRequestStatus::kLocalDataObsolete);
      return;
  }
  NOTREACHED();
}

void RegisterAuthenticationFactorRequest::RunCallbackAndMaybeDestroySelf(
    TrustedVaultRequestStatus status) {
  std::move(completion_callback_).Run(status);
}

}  // namespace syncer
