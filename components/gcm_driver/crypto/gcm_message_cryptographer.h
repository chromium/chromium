// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"

namespace gcm {

// Messages delivered through GCM may be encrypted according to the IETF Web
// Push protocol. We support two versions of ietf-webpush-encryption. The user
// of this class must pass in the version to use when constructing an instance.
//
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-03
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-08 (WGLC)
//
// This class implements the ability to encrypt or decrypt such messages using
// AEAD_AES_128_GCM with a 16-octet authentication tag. The encrypted payload
// will be stored in a single record.
//
// Note that while this class is not responsible for creating or storing the
// actual keys, it uses a key derivation function for the actual message
// encryption/decryption, thus allowing for the safe re-use of keys in multiple
// messages provided that a cryptographically-strong random salt is used.
class GCMMessageCryptographer {
 public:
  // Size, in bytes, of the authentication tag included in the messages.
  static const size_t kAuthenticationTagBytes;

  // Salt size, in bytes, that will be used together with the key to create a
  // unique content encryption key for a given message.
  static const size_t kSaltSize;

  // Version of the encryption scheme desired by the consumer.
  enum class Version {
    // https://tools.ietf.org/html/draft-ietf-webpush-encryption-03
    DRAFT_03,

    // https://tools.ietf.org/html/draft-ietf-webpush-encryption-08 (WGLC)
    DRAFT_08
  };

  // Interface that different versions of the encryption scheme must implement.
  class EncryptionScheme {
   public:
    virtual ~EncryptionScheme() = default;

    // Type of encoding to produce in GenerateInfoForContentEncoding().
    enum class EncodingType { CONTENT_ENCRYPTION_KEY, NONCE };

    // Derives the pseudo random key (PRK) to use for deriving the content
    // encryption key and the nonce.
    virtual std::string DerivePseudoRandomKey(
        std::string_view recipient_public_key,
        std::string_view sender_public_key,
        std::string_view ecdh_shared_secret,
        std::string_view auth_secret) = 0;

    // Generates the info string used for generating the content encryption key
    // and the nonce used for the cryptographic transformation.
    virtual std::string GenerateInfoForContentEncoding(
        EncodingType type,
        std::string_view recipient_public_key,
        std::string_view sender_public_key) = 0;

    // Creates an encryption record to contain the given |plaintext|.
    virtual std::string CreateRecord(std::string_view plaintext) = 0;

    // Validates that the |ciphertext_size| is valid following the scheme.
    virtual bool ValidateCiphertextSize(size_t ciphertext_size,
                                        size_t record_size) = 0;

    // Verifies that the padding included in |record| is valid and removes it
    // from the std::string_view. Returns whether the padding was valid.
    virtual bool ValidateAndRemovePadding(std::string_view& record) = 0;
  };

  // Creates a new cryptographer for |version| of the encryption scheme.
  explicit GCMMessageCryptographer(Version version);
  ~GCMMessageCryptographer();

  // Encrypts the |plaintext| in accordance with the Web Push Encryption scheme
  // this cryptographer represents, storing the result in |*record_size| and
  // |*ciphertext|. Returns whether encryption was successful.
  //
  // |recipient_public_key|: Recipient's key as an uncompressed P-256 EC point.
  // |sender_public_key|: Sender's key as an uncompressed P-256 EC point.
  // |ecdh_shared_secret|: 32-byte shared secret between the key pairs.
  // |auth_secret|: 16-byte prearranged secret between recipient and sender.
  // |salt|: 16-byte cryptographically secure salt unique to the message.
  // |plaintext|: The plaintext that is to be encrypted.
  // |*record_size|: Out parameter in which the record size will be written.
  // |*ciphertext|: Out parameter in which the ciphertext will be written.
  [[nodiscard]] bool Encrypt(std::string_view recipient_public_key,
                             std::string_view sender_public_key,
                             std::string_view ecdh_shared_secret,
                             std::string_view auth_secret,
                             std::string_view salt,
                             std::string_view plaintext,
                             size_t* record_size,
                             std::string* ciphertext) const;

  // Decrypts the |ciphertext| in accordance with the Web Push Encryption scheme
  // this cryptographer represents, storing the result in |*plaintext|. Returns
  // whether decryption was successful.
  //
  // |recipient_public_key|: Recipient's key as an uncompressed P-256 EC point.
  // |sender_public_key|: Sender's key as an uncompressed P-256 EC point.
  // |ecdh_shared_secret|: 32-byte shared secret between the key pairs.
  // |auth_secret|: 16-byte prearranged secret between recipient and sender.
  // |salt|: 16-byte cryptographically secure salt unique to the message.
  // |ciphertext|: The ciphertext that is to be decrypted.
  // |record_size|: Size of a single record. Must be larger than or equal to
  //                len(plaintext) plus the ciphertext's overhead (18 bytes).
  // |*plaintext|: Out parameter in which the plaintext will be written.
  [[nodiscard]] bool Decrypt(std::string_view recipient_public_key,
                             std::string_view sender_public_key,
                             std::string_view ecdh_shared_secret,
                             std::string_view auth_secret,
                             std::string_view salt,
                             std::string_view ciphertext,
                             size_t record_size,
                             std::string* plaintext) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, AuthSecretAffectsPRK);
  FRIEND_TEST_ALL_PREFIXES(GCMMessageCryptographerTest, InvalidRecordPadding);

  enum class Direction { ENCRYPT, DECRYPT };

  // Derives the content encryption key from |ecdh_shared_secret| and |salt|.
  std::string DeriveContentEncryptionKey(std::string_view recipient_public_key,
                                         std::string_view sender_public_key,
                                         std::string_view ecdh_shared_secret,
                                         std::string_view salt) const;

  // Derives the nonce from |ecdh_shared_secret| and |salt|.
  std::string DeriveNonce(std::string_view recipient_public_key,
                          std::string_view sender_public_key,
                          std::string_view ecdh_shared_secret,
                          std::string_view salt) const;

  // Private implementation of the encryption and decryption routines.
  bool TransformRecord(Direction direction,
                       std::string_view input,
                       std::string_view key,
                       std::string_view nonce,
                       std::string* output) const;

  // Implementation of the encryption scheme. Set in the constructor depending
  // on the version requested by the consumer.
  std::unique_ptr<EncryptionScheme> encryption_scheme_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_GCM_MESSAGE_CRYPTOGRAPHER_H_
