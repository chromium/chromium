// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_SET_H_
#define COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_SET_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

class AgileSymmetricKey;

// AgileSymmetricKeySet represents a set of keys, containing one primary key
// used for encryption, and multiple candidate keys used for decryption.
// This is analogous to Tink's Keyset.
class AgileSymmetricKeySet {
 public:
  // Creates an empty keyset. Note that an empty keyset returns nullopt when
  // attempting encryption or decryption, until a primary key is generated or
  // selected.
  static std::unique_ptr<AgileSymmetricKeySet> CreateEmpty();

  // Deserializes a keyset from proto, returning a valid instance (possibly
  // empty) or nullptr if the proto is determined invalid.
  static std::unique_ptr<AgileSymmetricKeySet> FromProto(
      const sync_pb::AgileSymmetricKeySet& proto);

  ~AgileSymmetricKeySet();

  AgileSymmetricKeySet(const AgileSymmetricKeySet&) = delete;
  AgileSymmetricKeySet& operator=(const AgileSymmetricKeySet&) = delete;

  // Serializes this keyset to proto.
  sync_pb::AgileSymmetricKeySet ToProto() const;

  size_t size() const { return keys_.size(); }

  uint32_t primary_key_id() const { return primary_key_id_; }

  // Generates a new random modern key (AES_256_GCM), adds it to this keyset
  // with a unique random key ID, and sets it as the primary key.
  // Returns the newly generated key's ID.
  uint32_t RotatePrimaryToNewlyGeneratedRandomKey();

  // Encrypts `plaintext` using the primary key of this set.
  // The output is packed into a sync_pb::EncryptedData proto, writing into
  // modern fields (blob_v2, key_id_v2) or legacy fields (key_name, blob)
  // depending on whether the primary key is AEAD-based or Legacy CBC-based.
  // Returns nullopt if `this` is empty (i.e. `size()` returns zero).
  std::optional<sync_pb::EncryptedData> Encrypt(
      base::span<const uint8_t> plaintext) const;

  // Decrypts the payload inside `encrypted_data`.
  // Supports both modern (using fast ID lookup in the set) and legacy
  // ciphertexts (by mapping legacy names against active keys in the set).
  // Returns the decrypted plaintext bytes, or nullopt on failure.
  std::optional<std::vector<uint8_t>> Decrypt(
      const sync_pb::EncryptedData& encrypted_data) const;

 private:
  AgileSymmetricKeySet();

  // Helper to add an existing key with a known ID. Returns false if the ID
  // already exists.
  bool AddKey(std::unique_ptr<AgileSymmetricKey> key, uint32_t key_id);

  std::optional<std::vector<uint8_t>> DecryptV2(
      base::span<const uint8_t> ciphertext,
      uint32_t key_id) const;

  std::optional<std::vector<uint8_t>> DecryptLegacy(
      const std::string& key_name,
      const std::string& base64_ciphertext) const;

  // Key ID zero is considered invalid. `primary_key_id_` set to zero is
  // only allowed for the case where `keys_` is empty.
  uint32_t primary_key_id_ = 0;
  base::flat_map<uint32_t, std::unique_ptr<AgileSymmetricKey>> keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_SET_H_
