// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_connection_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/download_keys_response_handler.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_request.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace trusted_vault {

namespace {

TrustedVaultRequest::RecordFetchStatusCallback MakeFetchStatusCallback(
    SecurityDomainId security_domain_id,
    TrustedVaultURLFetchReasonForUMA reason) {
  return base::BindRepeating(&RecordTrustedVaultURLFetchResponse,
                             security_domain_id, reason);
}

// Returns security domain epoch if valid (>0) and nullopt otherwise.
std::optional<int> GetLastKeyVersionFromJoinSecurityDomainsResponse(
    const trusted_vault_pb::JoinSecurityDomainsResponse response) {
  if (response.security_domain().current_epoch() > 0) {
    return response.security_domain().current_epoch();
  }
  return std::nullopt;
}

// Returns security domain epoch if input is a valid response for already exists
// error case and nullopt otherwise.
std::optional<int> GetLastKeyVersionFromAlreadyExistsResponse(
    const std::string& response_body) {
  trusted_vault_pb::RPCStatus rpc_status;
  rpc_status.ParseFromString(response_body);
  for (const trusted_vault_pb::Proto3Any& status_detail :
       rpc_status.details()) {
    if (status_detail.type_url() != kJoinSecurityDomainsErrorDetailTypeURL) {
      continue;
    }
    trusted_vault_pb::JoinSecurityDomainsErrorDetail error_detail;
    error_detail.ParseFromString(status_detail.value());
    return GetLastKeyVersionFromJoinSecurityDomainsResponse(
        error_detail.already_exists_response());
  }
  return std::nullopt;
}

trusted_vault_pb::SharedMemberKey CreateSharedMemberKey(
    const TrustedVaultKeyAndVersion& trusted_vault_key_and_version,
    const SecureBoxPublicKey& public_key) {
  trusted_vault_pb::SharedMemberKey shared_member_key;
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

trusted_vault_pb::SharedMemberKey CreateSharedMemberKey(
    const MemberKeys& precomputed) {
  trusted_vault_pb::SharedMemberKey shared_member_key;
  shared_member_key.set_epoch(precomputed.version);

  AssignBytesToProtoString(precomputed.wrapped_key,
                           shared_member_key.mutable_wrapped_key());
  AssignBytesToProtoString(precomputed.proof,
                           shared_member_key.mutable_member_proof());
  return shared_member_key;
}

trusted_vault_pb::PhysicalDeviceMetadata::DeviceType
GetLocalPhysicalDeviceType() {
  // Note that some of the below are unreachable in practice as this code isn't
  // currently used or even built on all platforms.
#if BUILDFLAG(IS_CHROMEOS)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_CHROMEOS;
#elif BUILDFLAG(IS_LINUX)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_LINUX;
#elif BUILDFLAG(IS_ANDROID)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_ANDROID;
#elif BUILDFLAG(IS_IOS)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_IOS;
#elif BUILDFLAG(IS_MAC)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_MAC_OS;
#elif BUILDFLAG(IS_WIN)
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_WINDOWS;
#elif BUILDFLAG(IS_FUCHSIA)
  // Not used in Fuchsia.
  return trusted_vault_pb::PhysicalDeviceMetadata::DEVICE_TYPE_UNKNOWN;
#else
#error Please handle your new device OS here.
#endif
}

trusted_vault_pb::SecurityDomainMember CreateSecurityDomainMember(
    const SecureBoxPublicKey& public_key,
    AuthenticationFactorType authentication_factor_type) {
  trusted_vault_pb::SecurityDomainMember member;
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

  absl::visit(
      base::Overloaded{
          [&member](const LocalPhysicalDevice&) {
            member.set_member_type(trusted_vault_pb::SecurityDomainMember::
                                       MEMBER_TYPE_PHYSICAL_DEVICE);
            auto* member_metadata = member.mutable_member_metadata();
            auto* physical_device_metadata =
                member_metadata->mutable_physical_device_metadata();
            physical_device_metadata->set_device_type(
                GetLocalPhysicalDeviceType());
          },
          [&member](const LockScreenKnowledgeFactor&) {
            member.set_member_type(trusted_vault_pb::SecurityDomainMember::
                                       MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR);
          },
          [&member](const UnspecifiedAuthenticationFactorType&) {
            member.set_member_type(trusted_vault_pb::SecurityDomainMember::
                                       MEMBER_TYPE_UNSPECIFIED);
            // The type hint field is in the request protobuf, not the
            // security domain member, and so is set in
            // `CreateJoinSecurityDomainsRequest`.
          },
          [&member](const GpmPinMetadata& gpm_pin_metadata) {
            member.set_member_type(trusted_vault_pb::SecurityDomainMember::
                                       MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN);
            auto* member_metadata = member.mutable_member_metadata();
            auto* pin_metadata =
                member_metadata->mutable_google_password_manager_pin_metadata();
            pin_metadata->set_encrypted_pin_hash(gpm_pin_metadata.wrapped_pin);
          },
          [&member](const ICloudKeychain&) {
            member.set_member_type(trusted_vault_pb::SecurityDomainMember::
                                       MEMBER_TYPE_ICLOUD_KEYCHAIN);
          }},
      authentication_factor_type);
  return member;
}

void AddSharedMemberKeysFromSource(
    trusted_vault_pb::JoinSecurityDomainsRequest* request,
    const SecureBoxPublicKey& public_key,
    const MemberKeysSource& member_keys_source) {
  absl::visit(
      base::Overloaded{
          [request, &public_key](
              const std::vector<TrustedVaultKeyAndVersion>& key_and_versions) {
            for (const TrustedVaultKeyAndVersion&
                     trusted_vault_key_and_version : key_and_versions) {
              *request->add_shared_member_key() = CreateSharedMemberKey(
                  trusted_vault_key_and_version, public_key);
            }
          },
          [request](const MemberKeys& precomputed) {
            *request->add_shared_member_key() =
                CreateSharedMemberKey(precomputed);
          }},
      member_keys_source);
}

trusted_vault_pb::JoinSecurityDomainsRequest CreateJoinSecurityDomainsRequest(
    SecurityDomainId security_domain,
    const MemberKeysSource& member_keys_source,
    const SecureBoxPublicKey& public_key,
    AuthenticationFactorType authentication_factor_type) {
  trusted_vault_pb::JoinSecurityDomainsRequest request;
  request.mutable_security_domain()->set_name(
      GetSecurityDomainPath(security_domain));
  *request.mutable_security_domain_member() =
      CreateSecurityDomainMember(public_key, authentication_factor_type);
  AddSharedMemberKeysFromSource(&request, public_key, member_keys_source);
  if (auto* unspecified_type =
          absl::get_if<UnspecifiedAuthenticationFactorType>(
              &authentication_factor_type)) {
    request.set_member_type_hint(unspecified_type->value());
  } else if (auto* gpm_pin_metadata =
                 absl::get_if<GpmPinMetadata>(&authentication_factor_type)) {
    if (gpm_pin_metadata->public_key) {
      request.set_current_public_key_to_replace(*gpm_pin_metadata->public_key);
    }
  }
  return request;
}

void RunRegisterAuthenticationFactorCallback(
    TrustedVaultConnection::RegisterAuthenticationFactorCallback callback,
    TrustedVaultRegistrationStatus status,
    int last_key_version) {
  std::move(callback).Run(status, last_key_version);
}

void ProcessJoinSecurityDomainsResponse(
    TrustedVaultConnectionImpl::JoinSecurityDomainsCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
    case TrustedVaultRequest::HttpStatus::kConflict:
      break;
    case TrustedVaultRequest::HttpStatus::kNetworkError:
      std::move(callback).Run(TrustedVaultRegistrationStatus::kNetworkError,
                              /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::kOtherError:
      std::move(callback).Run(TrustedVaultRegistrationStatus::kOtherError,
                              /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError:
      std::move(callback).Run(
          TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError,
          /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError:
      std::move(callback).Run(
          TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError,
          /*last_key_version=*/0);
      return;
    case TrustedVaultRequest::HttpStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      std::move(callback).Run(TrustedVaultRegistrationStatus::
                                  kPrimaryAccountChangeAccessTokenFetchError,
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

  std::optional<int> last_key_version;
  if (http_status == TrustedVaultRequest::HttpStatus::kConflict) {
    last_key_version =
        GetLastKeyVersionFromAlreadyExistsResponse(response_body);
  } else {
    trusted_vault_pb::JoinSecurityDomainsResponse response;
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

base::Time ToTime(const trusted_vault_pb::Timestamp& proto) {
  return base::Time::UnixEpoch() + base::Seconds(proto.seconds()) +
         base::Nanoseconds(proto.nanos());
}

void ProcessDownloadKeysResponse(
    std::unique_ptr<DownloadKeysResponseHandler> response_handler,
    TrustedVaultConnection::DownloadNewKeysCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  DownloadKeysResponseHandler::ProcessedResponse processed_response =
      response_handler->ProcessResponse(http_status, response_body);
  std::move(callback).Run(processed_response.status,
                          processed_response.downloaded_keys,
                          processed_response.last_key_version);
}

void ProcessDownloadIsRecoverabilityDegradedResponse(
    TrustedVaultConnection::IsRecoverabilityDegradedCallback callback,
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) {
  // TODO(crbug.com/40178774): consider special handling when security domain
  // doesn't exist.
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      break;
    case TrustedVaultRequest::HttpStatus::kNetworkError:
    case TrustedVaultRequest::HttpStatus::kOtherError:
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kBadRequest:
    case TrustedVaultRequest::HttpStatus::kConflict:
    case TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRequest::HttpStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      std::move(callback).Run(TrustedVaultRecoverabilityStatus::kError);
      return;
  }
  trusted_vault_pb::SecurityDomain security_domain;
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

class DownloadAuthenticationFactorsRegistrationStateRequest
    : public TrustedVaultConnection::Request {
 public:
  DownloadAuthenticationFactorsRegistrationStateRequest(
      SecurityDomainId security_domain,
      const GURL& request_url,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher,
      TrustedVaultConnection::
          DownloadAuthenticationFactorsRegistrationStateCallback callback)
      : security_domain_(security_domain),
        base_url_(request_url),
        account_id_(account_id),
        url_loader_factory_(std::move(url_loader_factory)),
        access_token_fetcher_(std::move(access_token_fetcher)),
        callback_(std::move(callback)) {
    result_.state =
        DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
    StartOrContinueRequest();
  }

 private:
  void StartOrContinueRequest(const std::string* next_page_token = nullptr) {
    request_ = std::make_unique<TrustedVaultRequest>(
        account_id_, TrustedVaultRequest::HttpMethod::kGet,
        next_page_token ? net::AppendQueryParameter(base_url_, "page_token",
                                                    *next_page_token)
                        : base_url_,
        /*serialized_request_proto=*/std::nullopt,
        /*max_retry_duration=*/base::Seconds(0), url_loader_factory_,
        access_token_fetcher_->Clone(),
        MakeFetchStatusCallback(
            security_domain_,
            TrustedVaultURLFetchReasonForUMA::
                kDownloadAuthenticationFactorsRegistrationState));

    // Unretained: this object owns `request_`. When `request_` is deleted, so
    // is the `SimpleURLLoader` and thus any callback is canceled.
    request_->FetchAccessTokenAndSendRequest(base::BindOnce(
        &DownloadAuthenticationFactorsRegistrationStateRequest::ProcessResponse,
        base::Unretained(this)));
  }

  void ProcessResponse(TrustedVaultRequest::HttpStatus http_status,
                       const std::string& response_body) {
    if (http_status != TrustedVaultRequest::HttpStatus::kSuccess) {
      result_.state =
          DownloadAuthenticationFactorsRegistrationStateResult::State::kError;
      FinishWithResultAndMaybeDestroySelf();
      return;
    }

    trusted_vault_pb::ListSecurityDomainMembersResponse response;
    if (!response.ParseFromString(response_body)) {
      result_.state =
          DownloadAuthenticationFactorsRegistrationStateResult::State::kError;
      FinishWithResultAndMaybeDestroySelf();
      return;
    }

    const std::string security_domain_name =
        GetSecurityDomainPath(security_domain_);
    for (const auto& member : response.security_domain_members()) {
      bool is_member_of_domain = false;
      for (const auto& membership : member.memberships()) {
        if (membership.security_domain() == security_domain_name) {
          is_member_of_domain = true;
          for (const auto& key : membership.keys()) {
            const int key_version = key.epoch();
            if (key_version != 0 &&
                (!result_.key_version.has_value() ||
                 result_.key_version.value() < key_version)) {
              result_.key_version = key_version;
            }
          }
          break;
        }
      }
      if (!is_member_of_domain) {
        continue;
      }

      if (member.has_member_metadata() &&
          member.member_metadata().usable_for_retrieval()) {
        result_.state = DownloadAuthenticationFactorsRegistrationStateResult::
            State::kRecoverable;
      } else if (result_.state ==
                 DownloadAuthenticationFactorsRegistrationStateResult::State::
                     kEmpty) {
        result_.state = DownloadAuthenticationFactorsRegistrationStateResult::
            State::kIrrecoverable;
      }

      if (member.member_type() == trusted_vault_pb::SecurityDomainMember::
                                      MEMBER_TYPE_GOOGLE_PASSWORD_MANAGER_PIN &&
          member.member_metadata().has_google_password_manager_pin_metadata()) {
        const auto& pin_metadata =
            member.member_metadata().google_password_manager_pin_metadata();
        result_.gpm_pin_metadata.emplace(
            member.public_key(), pin_metadata.encrypted_pin_hash(),
            ToTime(pin_metadata.expiration_time()));
      } else if (member.member_type() ==
                 trusted_vault_pb::SecurityDomainMember::
                     MEMBER_TYPE_ICLOUD_KEYCHAIN) {
        std::unique_ptr<SecureBoxPublicKey> public_key =
            SecureBoxPublicKey::CreateByImport(
                ProtoStringToBytes(member.public_key()));
        if (!public_key) {
          continue;
        }
        const auto& membership = std::ranges::find_if(
            member.memberships(),
            [&security_domain_name](const auto& membership) {
              return membership.security_domain() == security_domain_name;
            });
        CHECK(membership != member.memberships().end())
            << "iCloud member should have been tested for membership above";
        std::vector<MemberKeys> member_keys;
        std::ranges::transform(
            membership->keys(), std::back_inserter(member_keys),
            [](const auto& key) {
              return MemberKeys(key.epoch(),
                                std::vector<uint8_t>(key.wrapped_key().begin(),
                                                     key.wrapped_key().end()),
                                std::vector<uint8_t>(key.member_proof().begin(),
                                                     key.member_proof().end()));
            });
        result_.icloud_keys.emplace_back(std::move(public_key),
                                         std::move(member_keys));
      } else if (member.member_type() ==
                     trusted_vault_pb::SecurityDomainMember::
                         MEMBER_TYPE_LOCKSCREEN_KNOWLEDGE_FACTOR &&
                 member.member_metadata().has_lskf_metadata()) {
        const auto& metadata = member.member_metadata().lskf_metadata();
        result_.lskf_expiries.push_back(ToTime(metadata.expiration_time()));
      }
    }

    if (!response.next_page_token().empty()) {
      StartOrContinueRequest(&response.next_page_token());
      return;
    }

    FinishWithResultAndMaybeDestroySelf();
  }

  void FinishWithResultAndMaybeDestroySelf() {
    base::UmaHistogramEnumeration(
        "TrustedVault.DownloadAuthenticationFactorsRegistrationState." +
            GetSecurityDomainNameForUma(security_domain_),
        result_.state);
    std::move(callback_).Run(std::move(result_));
  }

  const SecurityDomainId security_domain_;
  const GURL base_url_;
  const CoreAccountId account_id_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher_;
  TrustedVaultConnection::DownloadAuthenticationFactorsRegistrationStateCallback
      callback_;
  std::unique_ptr<TrustedVaultRequest> request_;
  DownloadAuthenticationFactorsRegistrationStateResult result_;
};

TrustedVaultURLFetchReasonForUMA
GetURLFetchReasonForUMAForJoinSecurityDomainsRequest(
    AuthenticationFactorType authentication_factor_type) {
  return absl::visit(
      base::Overloaded{
          [](const LocalPhysicalDevice&) {
            return TrustedVaultURLFetchReasonForUMA::kRegisterDevice;
          },
          [](const LockScreenKnowledgeFactor&) {
            return TrustedVaultURLFetchReasonForUMA::
                kRegisterLockScreenKnowledgeFactor;
          },
          [](const UnspecifiedAuthenticationFactorType&) {
            return TrustedVaultURLFetchReasonForUMA::
                kRegisterUnspecifiedAuthenticationFactor;
          },
          [](const GpmPinMetadata&) {
            return TrustedVaultURLFetchReasonForUMA::kRegisterGpmPin;
          },
          [](const ICloudKeychain&) {
            return TrustedVaultURLFetchReasonForUMA::kRegisterICloudKeychain;
          }},
      authentication_factor_type);
}

std::vector<TrustedVaultKeyAndVersion> ConstantKeySource() {
  return {{GetConstantTrustedVaultKey(),
           /*version=*/kUnknownConstantKeyVersion}};
}

}  // namespace

std::vector<TrustedVaultKeyAndVersion> GetTrustedVaultKeysWithVersions(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_key_version) {
  const int first_key_version =
      last_key_version - static_cast<int>(trusted_vault_keys.size()) + 1;
  std::vector<TrustedVaultKeyAndVersion> result;
  for (size_t i = 0; i < trusted_vault_keys.size(); ++i) {
    result.emplace_back(trusted_vault_keys[i], first_key_version + i);
  }
  return result;
}

TrustedVaultConnectionImpl::TrustedVaultConnectionImpl(
    SecurityDomainId security_domain,
    const GURL& trusted_vault_service_url,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher)
    : security_domain_(security_domain),
      pending_url_loader_factory_(std::move(pending_url_loader_factory)),
      access_token_fetcher_(std::move(access_token_fetcher)),
      trusted_vault_service_url_(trusted_vault_service_url) {
  DCHECK(trusted_vault_service_url_.is_valid());
}

TrustedVaultConnectionImpl::~TrustedVaultConnectionImpl() = default;

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::RegisterAuthenticationFactor(
    const CoreAccountInfo& account_info,
    const MemberKeysSource& member_keys_source,
    const SecureBoxPublicKey& authentication_factor_public_key,
    AuthenticationFactorType authentication_factor_type,
    RegisterAuthenticationFactorCallback callback) {
  return SendJoinSecurityDomainsRequest(
      account_info, member_keys_source, authentication_factor_public_key,
      authentication_factor_type,
      base::BindOnce(&RunRegisterAuthenticationFactorCallback,
                     std::move(callback)));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::RegisterLocalDeviceWithoutKeys(
    const CoreAccountInfo& account_info,
    const SecureBoxPublicKey& device_public_key,
    RegisterAuthenticationFactorCallback callback) {
  return SendJoinSecurityDomainsRequest(
      account_info, ConstantKeySource(), device_public_key,
      LocalPhysicalDevice(),
      base::BindOnce(&RunRegisterAuthenticationFactorCallback,
                     std::move(callback)));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadNewKeys(
    const CoreAccountInfo& account_info,
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadNewKeysCallback callback) {
  // TODO(crbug.com/40255601): consider retries for keys downloading after
  // initial failure returned to the upper layers.
  auto request = std::make_unique<TrustedVaultRequest>(
      account_info.account_id, TrustedVaultRequest::HttpMethod::kGet,
      GetGetSecurityDomainMemberURL(
          trusted_vault_service_url_,
          device_key_pair->public_key().ExportToBytes()),
      /*serialized_request_proto=*/std::nullopt,
      /*max_retry_duration=*/base::Seconds(0), GetOrCreateURLLoaderFactory(),
      access_token_fetcher_->Clone(),
      MakeFetchStatusCallback(security_domain_,
                              TrustedVaultURLFetchReasonForUMA::kDownloadKeys));

  request->FetchAccessTokenAndSendRequest(
      base::BindOnce(&ProcessDownloadKeysResponse,
                     /*response_processor=*/
                     std::make_unique<DownloadKeysResponseHandler>(
                         security_domain_, last_trusted_vault_key_and_version,
                         std::move(device_key_pair)),
                     std::move(callback)));

  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    IsRecoverabilityDegradedCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      account_info.account_id, TrustedVaultRequest::HttpMethod::kGet,
      GetGetSecurityDomainURL(trusted_vault_service_url_, security_domain_),
      /*serialized_request_proto=*/std::nullopt,
      /*max_retry_duration=*/base::Seconds(0), GetOrCreateURLLoaderFactory(),
      access_token_fetcher_->Clone(),
      MakeFetchStatusCallback(
          security_domain_,
          TrustedVaultURLFetchReasonForUMA::kDownloadIsRecoverabilityDegraded));

  request->FetchAccessTokenAndSendRequest(base::BindOnce(
      &ProcessDownloadIsRecoverabilityDegradedResponse, std::move(callback)));

  return request;
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::DownloadAuthenticationFactorsRegistrationState(
    const CoreAccountInfo& account_info,
    DownloadAuthenticationFactorsRegistrationStateCallback callback) {
  return std::make_unique<
      DownloadAuthenticationFactorsRegistrationStateRequest>(
      security_domain_,
      GetGetSecurityDomainMembersURL(trusted_vault_service_url_),
      account_info.account_id, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_->Clone(), std::move(callback));
}

std::unique_ptr<TrustedVaultConnection::Request>
TrustedVaultConnectionImpl::SendJoinSecurityDomainsRequest(
    const CoreAccountInfo& account_info,
    const MemberKeysSource& member_keys_source,
    const SecureBoxPublicKey& authentication_factor_public_key,
    AuthenticationFactorType authentication_factor_type,
    JoinSecurityDomainsCallback callback) {
  auto request = std::make_unique<TrustedVaultRequest>(
      account_info.account_id, TrustedVaultRequest::HttpMethod::kPost,
      GetJoinSecurityDomainURL(trusted_vault_service_url_, security_domain_),
      /*serialized_request_proto=*/
      CreateJoinSecurityDomainsRequest(security_domain_, member_keys_source,
                                       authentication_factor_public_key,
                                       authentication_factor_type)
          .SerializeAsString(),
      kMaxJoinSecurityDomainRetryDuration, GetOrCreateURLLoaderFactory(),
      access_token_fetcher_->Clone(),
      MakeFetchStatusCallback(
          security_domain_,
          GetURLFetchReasonForUMAForJoinSecurityDomainsRequest(
              authentication_factor_type)));

  request->FetchAccessTokenAndSendRequest(
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

}  // namespace trusted_vault
