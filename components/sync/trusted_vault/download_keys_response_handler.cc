// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/download_keys_response_handler.h"

#include <map>
#include <utility>

#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"

namespace syncer {

namespace {

struct ExtractedSharedKey {
  int version;
  std::vector<uint8_t> trusted_vault_key;
  std::vector<uint8_t> rotation_proof;
};

const sync_pb::SecurityDomainMember::SecurityDomainMembership*
FindSyncMembership(const sync_pb::SecurityDomainMember& member) {
  for (const auto& membership : member.memberships()) {
    if (membership.security_domain() == kSyncSecurityDomainName) {
      return &membership;
    }
  }
  return nullptr;
}

// Extracts (decrypts |wrapped_key| and converts to ExtractedSharedKey) shared
// keys from |membership| and sorts them by version.
std::vector<ExtractedSharedKey> ExtractAndSortSharedKeys(
    const sync_pb::SecurityDomainMember::SecurityDomainMembership& membership,
    const SecureBoxPrivateKey& member_private_key) {
  std::map<int, ExtractedSharedKey> epoch_to_extracted_key;
  for (const sync_pb::SharedMemberKey& shared_key : membership.keys()) {
    base::Optional<std::vector<uint8_t>> decrypted_key =
        DecryptTrustedVaultWrappedKey(
            member_private_key, ProtoStringToBytes(shared_key.wrapped_key()));
    if (!decrypted_key.has_value()) {
      // Decryption failed.
      return std::vector<ExtractedSharedKey>();
    }
    epoch_to_extracted_key[shared_key.epoch()].version = shared_key.epoch();
    epoch_to_extracted_key[shared_key.epoch()].trusted_vault_key =
        *decrypted_key;
  }
  for (const sync_pb::RotationProof& rotation_proof :
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
  for (const auto& epoch_and_extracted_key : epoch_to_extracted_key) {
    result.push_back(epoch_and_extracted_key.second);
  }
  return result;
}

// Validates |rotation_proof| starting from the key next to
// last known trusted vault key, returns false if validation fails or |keys|
// doesn't have a key next to last known trusted vault key.
bool IsValidKeyChain(
    const std::vector<ExtractedSharedKey>& key_chain,
    const TrustedVaultKeyAndVersion& last_known_trusted_vault_key_and_version) {
  DCHECK(!key_chain.empty());
  if (key_chain.back().version <=
      last_known_trusted_vault_key_and_version.version) {
    // |keys| doesn't contain any new key. Note: this may mean that key rotation
    // happened, but state corresponding to the current member wasn't updated.
    return false;
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

    if (!VerifyTrustedVaultHMAC(last_valid_key, next_key.trusted_vault_key,
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
    TrustedVaultRequestStatus status)
    : status(status), last_key_version(0) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    TrustedVaultRequestStatus status,
    std::vector<std::vector<uint8_t>> new_keys,
    int last_key_version)
    : status(status), new_keys(new_keys), last_key_version(last_key_version) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse&
DownloadKeysResponseHandler::ProcessedResponse::operator=(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse::~ProcessedResponse() = default;

DownloadKeysResponseHandler::DownloadKeysResponseHandler(
    const base::Optional<TrustedVaultKeyAndVersion>&
        last_trusted_vault_key_and_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair)
    : last_trusted_vault_key_and_version_(last_trusted_vault_key_and_version),
      device_key_pair_(std::move(device_key_pair)) {
  DCHECK(device_key_pair_);
}

DownloadKeysResponseHandler::~DownloadKeysResponseHandler() = default;

DownloadKeysResponseHandler::ProcessedResponse
DownloadKeysResponseHandler::ProcessResponse(
    TrustedVaultRequest::HttpStatus http_status,
    const std::string& response_body) const {
  switch (http_status) {
    case TrustedVaultRequest::HttpStatus::kSuccess:
      break;
    case TrustedVaultRequest::HttpStatus::kNotFound:
    case TrustedVaultRequest::HttpStatus::kFailedPrecondition:
      // TODO(crbug.com/1113598): expose more detailed status.
      return ProcessedResponse(
          /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
    case TrustedVaultRequest::HttpStatus::kOtherError:
      return ProcessedResponse(
          /*status=*/TrustedVaultRequestStatus::kOtherError);
  }

  sync_pb::SecurityDomainMember member;
  if (!member.ParseFromString(response_body)) {
    return ProcessedResponse(/*status=*/TrustedVaultRequestStatus::kOtherError);
  }

  // TODO(crbug.com/1113598): consider validation of member public key.
  const sync_pb::SecurityDomainMember::SecurityDomainMembership* membership =
      FindSyncMembership(member);
  if (!membership) {
    // Member is not in sync security domain.
    return ProcessedResponse(
        /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
  }

  std::vector<ExtractedSharedKey> extracted_keys =
      ExtractAndSortSharedKeys(*membership, device_key_pair_->private_key());
  if (extracted_keys.empty()) {
    // |current_member| doesn't have any keys, should be treated as not
    // registered member.
    return ProcessedResponse(
        /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
  }

  if (last_trusted_vault_key_and_version_.has_value() &&
      !IsValidKeyChain(extracted_keys, *last_trusted_vault_key_and_version_)) {
    // Data corresponding to |current_member| is corrupted or
    // |last_trusted_vault_key_and_version_| is too old.
    return ProcessedResponse(
        /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
  }

  std::vector<std::vector<uint8_t>> new_keys;
  for (const ExtractedSharedKey& key : extracted_keys) {
    if (!last_trusted_vault_key_and_version_.has_value() ||
        key.version > last_trusted_vault_key_and_version_->version) {
      // Don't include previous keys into the result, because they weren't
      // validated using |last_trusted_vault_key_and_version| and client should
      // be already aware of them.
      new_keys.push_back(key.trusted_vault_key);
    }
  }
  return ProcessedResponse(/*status=*/TrustedVaultRequestStatus::kSuccess,
                           new_keys,
                           /*last_key_version=*/extracted_keys.back().version);
}

}  // namespace syncer
