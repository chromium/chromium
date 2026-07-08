// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/crypto/agile_symmetric_key_set.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/sync/model/crypto/agile_symmetric_key.h"
#include "crypto/random.h"

namespace syncer {

// static
std::unique_ptr<AgileSymmetricKeySet> AgileSymmetricKeySet::CreateEmpty() {
  return base::WrapUnique(new AgileSymmetricKeySet());
}

// static
std::unique_ptr<AgileSymmetricKeySet> AgileSymmetricKeySet::FromProto(
    const sync_pb::AgileSymmetricKeySet& proto) {
  auto keyset = base::WrapUnique(new AgileSymmetricKeySet());

  if (proto.key_size() == 0 && proto.primary_key_id() == 0) {
    // Empty keyset.
    return keyset;
  }

  for (const auto& key_proto : proto.key()) {
    std::unique_ptr<AgileSymmetricKey> key =
        AgileSymmetricKey::FromProto(key_proto.key_data());
    if (!key) {
      // Failure to read an individual key, as long as it is not the primary one
      // (see below), is tolerated. This is a safe-guard against potential
      // scenarios involving not-yet-supported ciphers or, more likely,
      // no-longer-supported ones.
      continue;
    }
    // Key ID zero is considered invalid (malformed data).
    if (key_proto.key_id() == 0) {
      return nullptr;
    }
    // If the key ID is duplicated, it is considered malformed data and trying
    // to recover a subset would be a workaround with unpredictable outcomes, so
    // treat it as a failure case.
    if (!keyset->AddKey(std::move(key), key_proto.key_id())) {
      return nullptr;
    }
  }

  // The primary key must exist and be valid.
  if (!keyset->keys_.contains(proto.primary_key_id())) {
    return nullptr;
  }

  keyset->primary_key_id_ = proto.primary_key_id();
  CHECK_NE(0U, keyset->primary_key_id_);

  return keyset;
}

AgileSymmetricKeySet::~AgileSymmetricKeySet() = default;

sync_pb::AgileSymmetricKeySet AgileSymmetricKeySet::ToProto() const {
  sync_pb::AgileSymmetricKeySet proto;
  if (primary_key_id_ != 0) {
    proto.set_primary_key_id(primary_key_id_);
  }
  for (const auto& [key_id, key] : keys_) {
    sync_pb::AgileSymmetricKeySet::Key* key_proto = proto.add_key();
    key_proto->set_key_id(key_id);
    *key_proto->mutable_key_data() = key->ToProto();
  }
  return proto;
}

uint32_t AgileSymmetricKeySet::RotatePrimaryToNewlyGeneratedRandomKey() {
  uint32_t key_id = 0;
  while (key_id == 0 || keys_.contains(key_id)) {
    crypto::RandBytes(base::byte_span_from_ref(key_id));
  }

  std::unique_ptr<AgileSymmetricKey> new_key =
      AgileSymmetricKey::CreateRandom();
  CHECK(new_key);

  CHECK(AddKey(std::move(new_key), key_id));
  primary_key_id_ = key_id;
  return key_id;
}

std::optional<sync_pb::EncryptedData> AgileSymmetricKeySet::Encrypt(
    base::span<const uint8_t> plaintext) const {
  if (keys_.empty()) {
    return std::nullopt;
  }
  CHECK_NE(primary_key_id_, 0u);
  const AgileSymmetricKey* primary_key =
      base::FindPtrOrNull(keys_, primary_key_id_);
  CHECK(primary_key);

  const std::vector<uint8_t> raw_ciphertext = primary_key->Encrypt(plaintext);

  sync_pb::EncryptedData proto;
  const std::string legacy_key_name = primary_key->GetLegacyNigoriKeyName();
  if (!legacy_key_name.empty()) {
    // Legacy key: only write legacy fields.
    proto.set_key_name(legacy_key_name);
    proto.set_blob(base::Base64Encode(raw_ciphertext));
  } else {
    // Modern key: only write modern fields.
    proto.set_blob_v2(raw_ciphertext.data(), raw_ciphertext.size());
    proto.set_key_id_v2(primary_key_id_);
  }
  return proto;
}

std::optional<std::vector<uint8_t>> AgileSymmetricKeySet::Decrypt(
    const sync_pb::EncryptedData& encrypted_data) const {
  if (encrypted_data.has_blob_v2()) {
    return DecryptV2(base::as_byte_span(encrypted_data.blob_v2()),
                     encrypted_data.key_id_v2());
  }

  if (!encrypted_data.key_name().empty() && !encrypted_data.blob().empty()) {
    return DecryptLegacy(encrypted_data.key_name(), encrypted_data.blob());
  }

  return std::nullopt;
}

std::optional<std::vector<uint8_t>> AgileSymmetricKeySet::DecryptV2(
    base::span<const uint8_t> ciphertext,
    uint32_t key_id) const {
  if (key_id != 0) {
    // Fast path: lookup by key ID.
    const AgileSymmetricKey* key = base::FindPtrOrNull(keys_, key_id);
    if (!key) {
      return std::nullopt;  // Key not present in this set.
    }
    return key->Decrypt(ciphertext);
  }

  // Fallback path: try all keys. This is done as future-proof safeguard in case
  // future encrypted blobs don't populate the key ID.
  for (const auto& [id, key] : keys_) {
    if (const std::optional<std::vector<uint8_t>> plaintext =
            key->Decrypt(ciphertext)) {
      return plaintext;
    }
  }
  return std::nullopt;
}

std::optional<std::vector<uint8_t>> AgileSymmetricKeySet::DecryptLegacy(
    const std::string& key_name,
    const std::string& base64_ciphertext) const {
  CHECK(!key_name.empty());
  CHECK(!base64_ciphertext.empty());

  for (const auto& [key_id, key] : keys_) {
    const std::string legacy_name = key->GetLegacyNigoriKeyName();
    if (legacy_name == key_name) {
      // Found matching legacy key.
      const std::optional<std::vector<uint8_t>> raw_ciphertext =
          base::Base64Decode(base64_ciphertext);
      if (!raw_ciphertext) {
        return std::nullopt;  // Corrupted Base64 payload.
      }
      return key->Decrypt(*raw_ciphertext);
    }
  }
  return std::nullopt;
}

AgileSymmetricKeySet::AgileSymmetricKeySet() = default;

bool AgileSymmetricKeySet::AddKey(std::unique_ptr<AgileSymmetricKey> key,
                                  uint32_t key_id) {
  CHECK(key);
  if (key_id == 0) {
    return false;  // Invalid key ID.
  }
  auto [it, inserted] = keys_.emplace(key_id, std::move(key));
  return inserted;
}

}  // namespace syncer
