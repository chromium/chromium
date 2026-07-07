// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_H_
#define COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "crypto/aead.h"

namespace syncer {

// AgileSymmetricKey represents an AEAD-based symmetric key that supports
// multiple cryptographic ciphers and conversion to/from protos, providing some
// level of crypto-agility.
class AgileSymmetricKey {
 public:
  // Creates a random AgileSymmetricKey instance with a random key,
  // using the default algorithm (AES_256_GCM).
  static std::unique_ptr<AgileSymmetricKey> CreateRandom();

  // Creates a AgileSymmetricKey that wraps a legacy Nigori key.
  static std::unique_ptr<AgileSymmetricKey> FromLegacyNigoriProto(
      const sync_pb::NigoriKey& proto);

  // Deserializes a AgileSymmetricKey from the given proto. This supports
  // agility by reading the algorithm choice from the proto.
  static std::unique_ptr<AgileSymmetricKey> FromProto(
      const sync_pb::AgileSymmetricKey& proto);

  ~AgileSymmetricKey();

  AgileSymmetricKey(const AgileSymmetricKey&) = delete;
  AgileSymmetricKey& operator=(const AgileSymmetricKey&) = delete;

  // Encrypts `plaintext` using AEAD. The output contains both the nonce and the
  // ciphertext, packed in a way that `Decrypt` can parse.
  std::vector<uint8_t> Encrypt(base::span<const uint8_t> plaintext) const;

  // Decrypts `ciphertext` using AEAD. The input must be the output of
  // `Encrypt`. Returns the plaintext, or nullopt if decryption/verification
  // fails.
  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext) const;

  // Serializes this key to proto.
  sync_pb::AgileSymmetricKey ToProto() const;

  // Returns the legacy Nigori key name if this wraps a legacy Nigori key,
  // otherwise empty.
  std::string GetLegacyNigoriKeyName() const;

 private:
  // Private interface for the actual key implementation.
  class Cipher {
   public:
    virtual ~Cipher() = default;
    virtual std::vector<uint8_t> Encrypt(
        base::span<const uint8_t> plaintext) const = 0;
    virtual std::optional<std::vector<uint8_t>> Decrypt(
        base::span<const uint8_t> ciphertext) const = 0;
    virtual sync_pb::AgileSymmetricKey ToProto() const = 0;
    virtual std::string GetLegacyNigoriKeyName() const = 0;
  };

  class AeadCipher;
  class LegacyNigoriCipher;

  // Private constructor.
  explicit AgileSymmetricKey(std::unique_ptr<Cipher> cipher);

  static std::unique_ptr<AgileSymmetricKey> CreateFromCipherIfNotNull(
      std::unique_ptr<Cipher> cipher);

  std::unique_ptr<Cipher> cipher_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CRYPTO_AGILE_SYMMETRIC_KEY_H_
