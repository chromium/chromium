// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/download_keys_response_handler.h"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>

#include "base/check.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

namespace {

struct ExtractedSharedKey {
  int version;
  std::vector<uint8_t> trusted_vault_key;
  std::vector<uint8_t> rotation_proof;
};

const trusted_vault_pb::SecurityDomainMember::SecurityDomainMembership*
FindMembershipForSecurityDomain(
    const trusted_vault_pb::SecurityDomainMember& member,
    SecurityDomainId security_domain) {
  for (const auto& membership : member.memberships()) {
    if (membership.security_domain() ==
        GetSecurityDomainPath(security_domain)) {
      return &membership;
    }
  }
  return nullptr;
}

// Extracts (decrypts |wrapped_key| and converts to ExtractedSharedKey) shared
// keys from |membership| and sorts them by version. In case of decryption
// errors it returns nullopt.
std::optional<std::vector<ExtractedSharedKey>> ExtractAndSortSharedKeys(
    const trusted_vault_pb::SecurityDomainMember::SecurityDomainMembership&
        membership,
    const SecureBoxPrivateKey& member_private_key) {
  std::map<int, ExtractedSharedKey> epoch_to_extracted_key;
  for (const trusted_vault_pb::SharedMemberKey& shared_key :
       membership.keys()) {
    std::optional<std::vector<uint8_t>> decrypted_key =
        DecryptTrustedVaultWrappedKey(
            member_private_key, ProtoStringToBytes(shared_key.wrapped_key()));
    if (!decrypted_key.has_value()) {
      // Decryption failed.
      return std::nullopt;
    }
    epoch_to_extracted_key[shared_key.epoch()].version = shared_key.epoch();
    epoch_to_extracted_key[shared_key.epoch()].trusted_vault_key =
        *decrypted_key;
  }
  for (const trusted_vault_pb::RotationProof& rotation_proof :
       membership.rotation_proofs()) {
    if (epoch_to_extracted_key.count(rotation_proof.new_epoch()) == 0) {
      // There is no shared key corresponding to rotation proof. In theory it
      // shouldn't happen, but it's safe to ignore.
      continue;
    }
    epoch_to_extracted_key[rotation_proof.new_epoch()].rotation_proof =
        ProtoStringToBytes(rotation_proof.rotation_proof());
  }

  std::vector<ExtractedSharedKey> result;
  for (const auto& [epoch, extracted_key] : epoch_to_extracted_key) {
    result.push_back(extracted_key);
  }
  return result;
}

// Validates |rotation_proof| starting from the key next to
// last known trusted vault key.
// |key_chain| must be sorted by ascending version number.
// The highest version number in |key_chain| must be greater than the version
// in |last_known_trusted_vault_key_and_version|.
bool IsValidKeyChain(
    const std::vector<ExtractedSharedKey>& key_chain,
    const TrustedVaultKeyAndVersion& last_known_trusted_vault_key_and_version) {
  CHECK(!key_chain.empty());
  CHECK(std::ranges::is_sorted(key_chain, std::less<>(),
                               &ExtractedSharedKey::version));
  if (base::FeatureList::IsEnabled(kE2eeRotationProofBypassFix)) {
    CHECK(key_chain.back().version >
          last_known_trusted_vault_key_and_version.version);
  }

  int last_valid_key_version = last_known_trusted_vault_key_and_version.version;
  std::vector<uint8_t> last_valid_key =
      last_known_trusted_vault_key_and_version.key;
  for (const ExtractedSharedKey& next_key : key_chain) {
    if (next_key.version <= last_valid_key_version) {
      continue;
    }

    if (next_key.version != last_valid_key_version + 1) {
      // Missing intermediate key.
      return false;
    }

    if (!VerifyRotationProof(/*trusted_vault_key=*/next_key.trusted_vault_key,
                             /*prev_trusted_vault_key=*/last_valid_key,
                             next_key.rotation_proof)) {
      // |rotation_proof| isn't valid.
      return false;
    }

    last_valid_key_version = next_key.version;
    last_valid_key = next_key.trusted_vault_key;
  }

  return true;
}

}  // namespace

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    TrustedVaultDownloadKeysStatus status)
    : status(status), last_key_version(0) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    TrustedVaultDownloadKeysStatus status,
    std::vector<std::vector<uint8_t>> downloaded_keys,
    int last_key_version)
    : status(status),
      downloaded_keys(downloaded_keys),
      last_key_version(last_key_version) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse&
DownloadKeysResponseHandler::ProcessedResponse::operator=(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse::~ProcessedResponse() = default;

// static
std::optional<TrustedVaultDownloadKeysStatus>
DownloadKeysResponseHandler::GetErrorFromHttpStatus(
    TrustedVaultRequest::HttpStatus http_status) {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      return std::nullopt;
    case TrustedVaultRequest::HttpStatus::kNotFound:
      return TrustedVaultDownloadKeysStatus::kMemberNotFound;
    case TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRequest::HttpStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      return TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure;
    case TrustedVaultRequest::HttpStatus::kNetworkError:
      return TrustedVaultDownloadKeysStatus::kNetworkError;
    case TrustedVaultRequest::HttpStatus::kBadRequest:
    case TrustedVaultRequest::HttpStatus::kConflict:
    case TrustedVaultRequest::HttpStatus::kOtherError:
      return TrustedVaultDownloadKeysStatus::kOtherError;
  }

  NOTREACHED();
}

DownloadKeysResponseHandler::DownloadKeysResponseHandler(
    SecurityDomainId security_domain,
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair)
    : security_domain_(security_domain),
      last_trusted_vault_key_and_version_(last_trusted_vault_key_and_version),
      device_key_pair_(std::move(device_key_pair)) {
  DCHECK(device_key_pair_);
}

DownloadKeysResponseHandler::~DownloadKeysResponseHandler() = default;

DownloadKeysResponseHandler::ProcessedResponse
DownloadKeysResponseHandler::ProcessResponse(
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) const {
  std::optional<TrustedVaultDownloadKeysStatus> error_from_http_status =
      GetErrorFromHttpStatus(http_status);
  if (error_from_http_status.has_value()) {
    return ProcessedResponse(*error_from_http_status);
  }

  trusted_vault_pb::SecurityDomainMember member;
  if (!member.ParseFromString(response_body)) {
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kOtherError);
  }

  // TODO(crbug.com/40143544): consider validation of member public key.
  const trusted_vault_pb::SecurityDomainMember::SecurityDomainMembership*
      membership = FindMembershipForSecurityDomain(member, security_domain_);
  if (!membership) {
    // Member is not in this security domain.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kMembershipNotFound);
  }

  const std::optional<std::vector<ExtractedSharedKey>> extracted_keys =
      ExtractAndSortSharedKeys(*membership, device_key_pair_->private_key());
  if (!extracted_keys.has_value()) {
    // |current_member| appears corrupt, as its keys could not be decrypted.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kMembershipCorrupted);
  }
  if (extracted_keys->empty()) {
    // |current_member| doesn't have any keys, should be treated as a corrupt
    // state.
    return ProcessedResponse(
        /*status=*/TrustedVaultDownloadKeysStatus::kMembershipEmpty);
  }
  if (base::FeatureList::IsEnabled(kE2eeRotationProofBypassFix)) {
    if (extracted_keys->back().version <
        last_trusted_vault_key_and_version_.version) {
      // |current_member| appears corrupt, as its keys have a lower version than
      // the last currently trusted one.
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::kMembershipCorrupted);
    }

    if (auto it = std::ranges::find(*extracted_keys,
                                    last_trusted_vault_key_and_version_.version,
                                    &ExtractedSharedKey::version);
        it != extracted_keys->end() &&
        it->trusted_vault_key != last_trusted_vault_key_and_version_.key) {
      // |current_member| appears corrupt, as its key with the version of the
      // last trusted vault key doesn't match the key bytes of the last
      // trusted vault key.
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::kMembershipCorrupted);
    }

    if (extracted_keys->back().version >
            last_trusted_vault_key_and_version_.version &&
        !IsValidKeyChain(*extracted_keys,
                         last_trusted_vault_key_and_version_)) {
      // New keys provided by |current_member| failed rotation proof
      // verification.
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::
              kKeyProofsVerificationFailed);
    }
  } else {
    if (!IsValidKeyChain(*extracted_keys,
                         last_trusted_vault_key_and_version_)) {
      // Data corresponding to |current_member| is corrupted or
      // |last_trusted_vault_key_and_version_| is too old.
      return ProcessedResponse(
          /*status=*/TrustedVaultDownloadKeysStatus::
              kKeyProofsVerificationFailed);
    }
  }

  std::vector<std::vector<uint8_t>> trusted_vault_keys;
  for (const ExtractedSharedKey& key : *extracted_keys) {
    trusted_vault_keys.push_back(key.trusted_vault_key);
  }

  TrustedVaultDownloadKeysStatus status =
      TrustedVaultDownloadKeysStatus::kSuccess;
  const bool is_no_new_keys =
      base::FeatureList::IsEnabled(kE2eeRotationProofBypassFix)
          ? (extracted_keys->back().version ==
             last_trusted_vault_key_and_version_.version)
          : (extracted_keys->back().version <=
             last_trusted_vault_key_and_version_.version);
  if (is_no_new_keys) {
    // In theory, the check when the feature flag is disabled could be ==
    // instead of <=, since server version should not decrease, but it was
    // tolerated to make the legacy implementation more robust. When the
    // `kE2eeRotationProofBypassFix` flag is enabled, strictly lower version
    // cases are already rejected in the conditions above, so checking for
    // strict equality here is sufficient and more consistent.
    status = TrustedVaultDownloadKeysStatus::kNoNewKeys;
  }

  return ProcessedResponse(status, trusted_vault_keys,
                           /*last_key_version=*/extracted_keys->back().version);
}

}  // namespace trusted_vault
