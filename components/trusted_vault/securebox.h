// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_SECUREBOX_H_
#define COMPONENTS_TRUSTED_VAULT_SECUREBOX_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {
class OpenSSLErrStackTracer;
}  // namespace crypto

namespace trusted_vault {

// Encrypts |payload| according to SecureBox v2 spec:
// 1. Encryption key is derived from |shared_secret| using HKDF-SHA256.
// 2. |payload| is encrypted using AES-128-GCM, using random 96-bit nonce and
// given |header|.
// |shared_secret|, |header| and |payload| may be empty, though empty
// |shared_secret| shouldn't be used.
std::vector<uint8_t> SecureBoxSymmetricEncrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> payload);

// Decrypts |encrypted_payload| according to SecureBox v2 spec (see
// above). Returns nullopt if payload was encrypted with different parameters or
// |encrypted_payload| isn't a valid SecureBox encrypted data.
std::optional<std::vector<uint8_t>> SecureBoxSymmetricDecrypt(
    base::span<const uint8_t> shared_secret,
    base::span<const uint8_t> header,
    base::span<const uint8_t> encrypted_payload);

class SecureBoxPublicKey {
 public:
  // Creates public key given a X9.62 formatted NIST P-256 point as |key_bytes|
  // (e.g. by the output of SecureBoxPublicKey::ExportToBytes()). Returns
  // nullptr if point format isn't correct or it doesn't represent a valid P-256
  // point.
  static std::unique_ptr<SecureBoxPublicKey> CreateByImport(
      base::span<const uint8_t> key_bytes);

  // |key| must be a valid NIST P-256 key with filled public key. This method
  // shouldn't be used outside internal SecureBox implementation.
  static std::unique_ptr<SecureBoxPublicKey> CreateInternal(
      bssl::UniquePtr<EC_KEY> key,
      const crypto::OpenSSLErrStackTracer& err_tracer);

  SecureBoxPublicKey(const SecureBoxPublicKey& other) = delete;
  SecureBoxPublicKey& operator=(const SecureBoxPublicKey& other) = delete;
  ~SecureBoxPublicKey();

  // Returns a X9.62 formatted NIST P-256 point.
  std::vector<uint8_t> ExportToBytes() const;

  // Encrypts |payload| according to SecureBox v2 spec (go/securebox2):
  // 1. Key material is P-256 ECDH key derived from |key_| and randomly
  // generated P-256 key pair, concatenated with |shared_secret|.
  // 2. Encryption key is derived from key material using HKDF-SHA256.
  // 3. |payload| is encrypted using AES-128-GCM, using random 96-bit nonce and
  // given |header|.
  // |shared_secret|, |header| and |payload| may be empty.
  std::vector<uint8_t> Encrypt(base::span<const uint8_t> shared_secret,
                               base::span<const uint8_t> header,
                               base::span<const uint8_t> payload) const;

 private:
  // |key| must be a valid NIST P-256 key with filled public key.
  SecureBoxPublicKey(bssl::UniquePtr<EC_KEY> key,
                     const crypto::OpenSSLErrStackTracer& err_tracer);

  bssl::UniquePtr<EC_KEY> key_;
};

class SecureBoxPrivateKey {
 public:
  // Creates private key given NIST P-256 scalar in padded big-endian format
  // (e.g. by the output of SecureBoxPrivateKey::ExportToBytes()). Returns
  // nullptr if P-256 key can't be decoded from |key_bytes| or its format is
  // incorrect.
  static std::unique_ptr<SecureBoxPrivateKey> CreateByImport(
      base::span<const uint8_t> key_bytes);

  // |key| must be a valid NIST P-256 key with filled private and public key.
  // This method shouldn't be used outside internal SecureBox implementation.
  static std::unique_ptr<SecureBoxPrivateKey> CreateInternal(
      bssl::UniquePtr<EC_KEY> key,
      const crypto::OpenSSLErrStackTracer& err_tracer);

  SecureBoxPrivateKey(const SecureBoxPrivateKey& other) = delete;
  SecureBoxPrivateKey& operator=(const SecureBoxPrivateKey& other) = delete;
  ~SecureBoxPrivateKey();

  // Returns NIST P-256 scalar in padded big-endian format.
  std::vector<uint8_t> ExportToBytes() const;

  // Decrypts |encrypted_payload| according to SecureBox v2 spec (see
  // SecureBoxPublicKey::Encrypt()). Returns nullopt if payload was encrypted
  // with different parameters or |encrypted_payload| isn't a valid SecureBox
  // encrypted data.
  std::optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> shared_secret,
      base::span<const uint8_t> header,
      base::span<const uint8_t> encrypted_payload) const;

 private:
  // |key| must be a valid NIST P-256 key with filled private and public key.
  explicit SecureBoxPrivateKey(bssl::UniquePtr<EC_KEY> key,
                               const crypto::OpenSSLErrStackTracer& err_tracer);

  bssl::UniquePtr<EC_KEY> key_;
};

class SecureBoxKeyPair {
 public:
  // Generates new random key pair. Never returns nullptr.
  static std::unique_ptr<SecureBoxKeyPair> GenerateRandom();

  // Creates key pair given NIST P-256 scalar in padded big-endian format
  // (e.g. by the output of SecureBoxPrivateKey::ExportToBytes()). Returns
  // nullptr if P-256 key can't be decoded from |private_key_bytes| or its
  // format is incorrect.
  static std::unique_ptr<SecureBoxKeyPair> CreateByPrivateKeyImport(
      base::span<const uint8_t> private_key_bytes);

  SecureBoxKeyPair(const SecureBoxKeyPair& other) = delete;
  SecureBoxKeyPair& operator=(const SecureBoxKeyPair& other) = delete;
  ~SecureBoxKeyPair();

  const SecureBoxPrivateKey& private_key() const { return *private_key_; }

  const SecureBoxPublicKey& public_key() const { return *public_key_; }

 private:
  SecureBoxKeyPair(bssl::UniquePtr<EC_KEY> private_ec_key,
                   const crypto::OpenSSLErrStackTracer& err_tracer);

  std::unique_ptr<SecureBoxPrivateKey> private_key_;
  std::unique_ptr<SecureBoxPublicKey> public_key_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_SECUREBOX_H_
