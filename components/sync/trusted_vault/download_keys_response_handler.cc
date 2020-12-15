// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/download_keys_response_handler.h"

#include <algorithm>
#include <utility>

#include "components/sync/protocol/vault.pb.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_crypto.h"

namespace syncer {

namespace {

const char kSecurityDomainName[] = "chromesync";

struct ExtractedSharedKey {
  int version;
  std::vector<uint8_t> trusted_vault_key;
  std::vector<uint8_t> key_proof;
};

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

// Returns pointer to the member in |response| corresponding to
// |member_public_key|. Returns nullptr if sync security domain doesn't exist
// in |response| or there is no such member in sync security domain.
const sync_pb::SecurityDomain::Member* FindMember(
    const sync_pb::ListSecurityDomainsResponse& response,
    const std::vector<uint8_t>& member_public_key_bytes) {
  const sync_pb::SecurityDomain* sync_security_domain =
      FindSyncSecurityDomain(response);
  if (!sync_security_domain) {
    return nullptr;
  }

  const std::string member_public_key_string(member_public_key_bytes.begin(),
                                             member_public_key_bytes.end());
  for (const sync_pb::SecurityDomain::Member& member :
       sync_security_domain->members()) {
    if (member.public_key() == member_public_key_string) {
      return &member;
    }
  }
  return nullptr;
}

// Extracts (decrypts |wrapped_key| and converts to ExtractedSharedKey) shared
// keys from |member| and sorts them by version.
std::vector<ExtractedSharedKey> ExtractAndSortSharedKeys(
    const sync_pb::SecurityDomain::Member& member,
    const SecureBoxPrivateKey& member_private_key) {
  std::vector<ExtractedSharedKey> result;
  for (const sync_pb::SharedKey& shared_key : member.keys()) {
    base::Optional<std::vector<uint8_t>> decrypted_key =
        DecryptTrustedVaultWrappedKey(
            member_private_key, ProtoStringToBytes(shared_key.wrapped_key()));
    if (!decrypted_key.has_value()) {
      // Decryption failed.
      return std::vector<ExtractedSharedKey>();
    }
    result.push_back(ExtractedSharedKey{
        /*version=*/shared_key.epoch(), *decrypted_key,
        /*key_proof=*/ProtoStringToBytes(shared_key.key_proof())});
  }

  std::sort(result.begin(), result.end(),
            [](const ExtractedSharedKey& a, const ExtractedSharedKey& b) {
              return a.version < b.version;
            });
  return result;
}

// Validates |key_proof| starting from the key next to
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
                                next_key.key_proof)) {
      // |key_proof| isn't valid.
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
    std::vector<std::vector<uint8_t>> keys,
    int last_key_version)
    : status(status), keys(keys), last_key_version(last_key_version) {}

DownloadKeysResponseHandler::ProcessedResponse::ProcessedResponse(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse&
DownloadKeysResponseHandler::ProcessedResponse::operator=(
    const ProcessedResponse& other) = default;

DownloadKeysResponseHandler::ProcessedResponse::~ProcessedResponse() = default;

DownloadKeysResponseHandler::DownloadKeysResponseHandler(
    const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
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
    case TrustedVaultRequest::HttpStatus::kOtherError:
    case TrustedVaultRequest::HttpStatus::kBadRequest:
      // Don't distinguish kBadRequest here, because request content doesn't
      // depend on the local state.
      return ProcessedResponse(
          /*status=*/TrustedVaultRequestStatus::kOtherError);
  }

  sync_pb::ListSecurityDomainsResponse deserialized_response;
  if (!deserialized_response.ParseFromString(response_body)) {
    return ProcessedResponse(/*status=*/TrustedVaultRequestStatus::kOtherError);
  }

  const sync_pb::SecurityDomain::Member* current_member = FindMember(
      deserialized_response, device_key_pair_->public_key().ExportToBytes());
  if (!current_member) {
    // |device_key_pair_| isn't registered server-side, while client assumes
    // it's registered when downloading keys.
    return ProcessedResponse(
        /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
  }

  std::vector<ExtractedSharedKey> extracted_keys = ExtractAndSortSharedKeys(
      *current_member, device_key_pair_->private_key());
  if (extracted_keys.empty() ||
      !IsValidKeyChain(extracted_keys, last_trusted_vault_key_and_version_)) {
    return ProcessedResponse(
        /*status=*/TrustedVaultRequestStatus::kLocalDataObsolete);
  }

  std::vector<std::vector<uint8_t>> new_keys;
  for (const ExtractedSharedKey& key : extracted_keys) {
    if (key.version >= last_trusted_vault_key_and_version_.version) {
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
