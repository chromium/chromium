// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/ostream_operators.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"

namespace gcm {

namespace {

// Size, in bytes, of the nonce for a record. This must be at least the size
// of a uint64_t, which is used to indicate the record sequence number.
const uint64_t kNonceSize = 12;

// The default record size as defined by httpbis-encryption-encoding-06.
const size_t kDefaultRecordSize = 4096;

// Key size, in bytes, of a valid AEAD_AES_128_GCM key.
const size_t kContentEncryptionKeySize = 16;

// Implementation of draft 03 of the Web Push Encryption standard:
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-03
// https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-02
class WebPushEncryptionDraft03
    : public GCMMessageCryptographer::EncryptionScheme {
 public:
  WebPushEncryptionDraft03() = default;

  WebPushEncryptionDraft03(const WebPushEncryptionDraft03&) = delete;
  WebPushEncryptionDraft03& operator=(const WebPushEncryptionDraft03&) = delete;

  ~WebPushEncryptionDraft03() override = default;

  // GCMMessageCryptographer::EncryptionScheme implementation.
  std::string DerivePseudoRandomKey(std::string_view /* recipient_public_key */,
                                    std::string_view /* sender_public_key */,
                                    std::string_view ecdh_shared_secret,
                                    std::string_view auth_secret) override {
    const char kInfo[] = "Content-Encoding: auth";

    // This deliberately copies over the NUL terminus.
    std::string_view info(kInfo, sizeof(kInfo));

    return crypto::HkdfSha256(ecdh_shared_secret, auth_secret, info, 32);
  }

  // Creates the info parameter for an HKDF value for the given
  // |content_encoding| in accordance with draft-ietf-webpush-encryption-03.
  //
  // cek_info = "Content-Encoding: aesgcm" || 0x00 || context
  // nonce_info = "Content-Encoding: nonce" || 0x00 || context
  //
  // context = "P-256" || 0x00 ||
  //           length(recipient_public) || recipient_public ||
  //           length(sender_public) || sender_public
  //
  // The length of the public keys must be written as a two octet unsigned
  // integer in network byte order (big endian).
  std::string GenerateInfoForContentEncoding(
      EncodingType type,
      std::string_view recipient_public_key,
      std::string_view sender_public_key) override {
    std::string info;
    info += "Content-Encoding: ";

    switch (type) {
      case EncodingType::CONTENT_ENCRYPTION_KEY:
        info += "aesgcm";
        break;
      case EncodingType::NONCE:
        info += "nonce";
        break;
    }

    info += '\x00';
    info += "P-256";
    info += '\x00';

    info += base::as_string_view(base::U16ToBigEndian(
        base::checked_cast<uint16_t>(recipient_public_key.size())));
    info += recipient_public_key;

    info += base::as_string_view(base::U16ToBigEndian(
        base::checked_cast<uint16_t>(sender_public_key.size())));
    info += sender_public_key;

    return info;
  }

  // draft-ietf-webpush-encryption-03 defines that the padding is included at
  // the beginning of the message. The first two bytes, in network byte order,
  // contain the length of the included padding. Then that exact number of bytes
  // must follow as padding, all of which must have a zero value.
  //
  // TODO(peter): Add support for message padding if the GCMMessageCryptographer
  // starts encrypting payloads for reasons other than testing.
  std::string CreateRecord(std::string_view plaintext) override {
    std::string record;
    record.reserve(sizeof(uint16_t) + plaintext.size());
    record.append(sizeof(uint16_t), '\x00');
    record.append(plaintext);
    return record;
  }

  // The |ciphertext| must be at least of size kAuthenticationTagBytes with two
  // padding bytes, which is the case for an empty message with zero padding.
  // The |record_size| must be large enough to use only one record.
  // https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-03#section-2
  bool ValidateCiphertextSize(size_t ciphertext_size,
                              size_t record_size) override {
    return ciphertext_size >=
               sizeof(uint16_t) +
                   GCMMessageCryptographer::kAuthenticationTagBytes &&
           ciphertext_size <=
               record_size + GCMMessageCryptographer::kAuthenticationTagBytes;
  }

  // The record padding in draft-ietf-webpush-encryption-03 is included at the
  // beginning of the record. The first two bytes indicate the length of the
  // padding. All padding bytes immediately follow, and must be set to zero.
  bool ValidateAndRemovePadding(std::string_view& record) override {
    // Records must be at least two octets in size (to hold the padding).
    // Records that are smaller, i.e. a single octet, are invalid.
    if (record.size() < sizeof(uint16_t))
      return false;

    // Records contain a two-byte, big-endian padding length followed by zero to
    // 65535 bytes of padding. Padding bytes must be zero but, since AES-GCM
    // authenticates the plaintext, checking and removing padding need not be
    // done in constant-time.
    uint16_t padding_length = (static_cast<uint8_t>(record[0]) << 8) |
                              static_cast<uint8_t>(record[1]);
    record.remove_prefix(sizeof(uint16_t));

    if (padding_length > record.size()) {
      return false;
    }

    for (size_t i = 0; i < padding_length; ++i) {
      if (record[i] != 0)
        return false;
    }

    record.remove_prefix(padding_length);
    return true;
  }
};

// Implementation of draft 08 of the Web Push Encryption standard:
// https://tools.ietf.org/html/draft-ietf-webpush-encryption-08
// https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-07
class WebPushEncryptionDraft08
    : public GCMMessageCryptographer::EncryptionScheme {
 public:
  WebPushEncryptionDraft08() = default;

  WebPushEncryptionDraft08(const WebPushEncryptionDraft08&) = delete;
  WebPushEncryptionDraft08& operator=(const WebPushEncryptionDraft08&) = delete;

  ~WebPushEncryptionDraft08() override = default;

  // GCMMessageCryptographer::EncryptionScheme implementation.
  std::string DerivePseudoRandomKey(std::string_view recipient_public_key,
                                    std::string_view sender_public_key,
                                    std::string_view ecdh_shared_secret,
                                    std::string_view auth_secret) override {
    DCHECK_EQ(recipient_public_key.size(), 65u);
    DCHECK_EQ(sender_public_key.size(), 65u);

    const char kInfo[] = "WebPush: info";

    // This deliberately copies over the NUL terminus.
    std::string info = base::StrCat({std::string_view(kInfo, sizeof(kInfo)),
                                     recipient_public_key, sender_public_key});

    return crypto::HkdfSha256(ecdh_shared_secret, auth_secret, info, 32);
  }

  // The info string used for generating the content encryption key and the
  // nonce was simplified in draft-ietf-webpush-encryption-08, because the
  // public keys of both the recipient and the sender are now in the PRK.
  std::string GenerateInfoForContentEncoding(
      EncodingType type,
      std::string_view /* recipient_public_key */,
      std::string_view /* sender_public_key */) override {
    std::string info;
    info += "Content-Encoding: ";

    switch (type) {
      case EncodingType::CONTENT_ENCRYPTION_KEY:
        info += "aes128gcm";
        break;
      case EncodingType::NONCE:
        info += "nonce";
        break;
    }

    info += '\x00';
    return info;
  }

  // draft-ietf-webpush-encryption-08 defines that the padding follows the
  // plaintext of a message. A delimiter byte (0x02 for the final record) will
  // be added, and then zero or more bytes of padding.
  //
  // TODO(peter): Add support for message padding if the GCMMessageCryptographer
  // starts encrypting payloads for reasons other than testing.
  std::string CreateRecord(std::string_view plaintext) override {
    std::string record;
    record.reserve(plaintext.size() + sizeof(uint8_t));
    record.append(plaintext);
    record.append(sizeof(uint8_t), '\x02');
    return record;
  }

  // The |ciphertext| must be at least of size kAuthenticationTagBytes with one
  // padding delimiter, which is the case for an empty message with minimal
  // padding. The |record_size| must be large enough to use only one record.
  // https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-08#section-2
  bool ValidateCiphertextSize(size_t ciphertext_size,
                              size_t record_size) override {
    return ciphertext_size >=
               sizeof(uint8_t) +
                   GCMMessageCryptographer::kAuthenticationTagBytes &&
           ciphertext_size <=
               record_size + GCMMessageCryptographer::kAuthenticationTagBytes;
  }

  // The record padding in draft-ietf-webpush-encryption-08 is included at the
  // end of the record. The length is not defined, but all padding bytes must be
  // zero until the delimiter (0x02) is found.
  bool ValidateAndRemovePadding(std::string_view& record) override {
    DCHECK_GE(record.size(), 1u);

    size_t padding_length = 1;
    for (; padding_length <= record.size(); ++padding_length) {
      size_t offset = record.size() - padding_length;

      if (record[offset] == 0x02 /* padding delimiter octet */)
        break;

      if (record[offset] != 0x00 /* valid padding byte */)
        return false;
    }

    record.remove_suffix(padding_length);
    return true;
  }
};

}  // namespace

const size_t GCMMessageCryptographer::kAuthenticationTagBytes = 16;
const size_t GCMMessageCryptographer::kSaltSize = 16;

GCMMessageCryptographer::GCMMessageCryptographer(Version version) {
  switch (version) {
    case Version::DRAFT_03:
      encryption_scheme_ = std::make_unique<WebPushEncryptionDraft03>();
      return;
    case Version::DRAFT_08:
      encryption_scheme_ = std::make_unique<WebPushEncryptionDraft08>();
      return;
  }

  NOTREACHED();
}

GCMMessageCryptographer::~GCMMessageCryptographer() = default;

bool GCMMessageCryptographer::Encrypt(std::string_view recipient_public_key,
                                      std::string_view sender_public_key,
                                      std::string_view ecdh_shared_secret,
                                      std::string_view auth_secret,
                                      std::string_view salt,
                                      std::string_view plaintext,
                                      size_t* record_size,
                                      std::string* ciphertext) const {
  DCHECK_EQ(recipient_public_key.size(), 65u);
  DCHECK_EQ(sender_public_key.size(), 65u);
  DCHECK_EQ(ecdh_shared_secret.size(), 32u);
  DCHECK_EQ(auth_secret.size(), 16u);
  DCHECK_EQ(salt.size(), 16u);
  DCHECK(record_size);
  DCHECK(ciphertext);

  std::string prk = encryption_scheme_->DerivePseudoRandomKey(
      recipient_public_key, sender_public_key, ecdh_shared_secret, auth_secret);

  std::string content_encryption_key = DeriveContentEncryptionKey(
      recipient_public_key, sender_public_key, prk, salt);
  std::string nonce =
      DeriveNonce(recipient_public_key, sender_public_key, prk, salt);

  std::string record = encryption_scheme_->CreateRecord(plaintext);
  std::string encrypted_record;

  if (!TransformRecord(Direction::ENCRYPT, record, content_encryption_key,
                       nonce, &encrypted_record)) {
    return false;
  }

  // The advertised record size must be at least one more than the padded
  // plaintext to ensure only one record.
  *record_size = std::max(kDefaultRecordSize, record.size() + 1);

  ciphertext->swap(encrypted_record);
  return true;
}

bool GCMMessageCryptographer::Decrypt(std::string_view recipient_public_key,
                                      std::string_view sender_public_key,
                                      std::string_view ecdh_shared_secret,
                                      std::string_view auth_secret,
                                      std::string_view salt,
                                      std::string_view ciphertext,
                                      size_t record_size,
                                      std::string* plaintext) const {
  DCHECK_EQ(recipient_public_key.size(), 65u);
  DCHECK_EQ(sender_public_key.size(), 65u);
  DCHECK_EQ(ecdh_shared_secret.size(), 32u);
  DCHECK_EQ(auth_secret.size(), 16u);
  DCHECK_EQ(salt.size(), 16u);
  DCHECK(plaintext);

  if (record_size <= 1) {
    LOG(ERROR) << "Invalid record size passed.";
    return false;
  }

  std::string prk = encryption_scheme_->DerivePseudoRandomKey(
      recipient_public_key, sender_public_key, ecdh_shared_secret, auth_secret);

  std::string content_encryption_key = DeriveContentEncryptionKey(
      recipient_public_key, sender_public_key, prk, salt);

  std::string nonce =
      DeriveNonce(recipient_public_key, sender_public_key, prk, salt);

  if (!encryption_scheme_->ValidateCiphertextSize(ciphertext.size(),
                                                  record_size)) {
    LOG(ERROR) << "Invalid ciphertext size passed.";
    return false;
  }

  std::string decrypted_record_string;
  if (!TransformRecord(Direction::DECRYPT, ciphertext, content_encryption_key,
                       nonce, &decrypted_record_string)) {
    LOG(ERROR) << "Unable to transform the record.";
    return false;
  }

  DCHECK(!decrypted_record_string.empty());

  std::string_view decrypted_record(decrypted_record_string);
  if (!encryption_scheme_->ValidateAndRemovePadding(decrypted_record)) {
    LOG(ERROR) << "Padding could not be validated or removed.";
    return false;
  }

  *plaintext = decrypted_record;
  return true;
}

bool GCMMessageCryptographer::TransformRecord(Direction direction,
                                              std::string_view input_str,
                                              std::string_view key_str,
                                              std::string_view nonce_str,
                                              std::string* output_str) const {
  constexpr auto kAlgorithm = crypto::aead::AES_128_GCM;
  const auto input = base::as_byte_span(input_str);
  const auto key = base::as_byte_span(key_str);
  const auto nonce = base::as_byte_span(nonce_str);
  constexpr base::span<const uint8_t> kNoAssociatedData{};

  if (direction == Direction::ENCRYPT) {
    std::vector<uint8_t> ciphertext =
        crypto::aead::Seal(kAlgorithm, key, input, nonce, kNoAssociatedData);
    output_str->assign(base::as_string_view(ciphertext));
    return true;
  } else {
    std::optional<std::vector<uint8_t>> plaintext =
        crypto::aead::Open(kAlgorithm, key, input, nonce, kNoAssociatedData);
    if (plaintext.has_value()) {
      output_str->assign(base::as_string_view(*plaintext));
    }
    return plaintext.has_value();
  }
}

std::string GCMMessageCryptographer::DeriveContentEncryptionKey(
    std::string_view recipient_public_key,
    std::string_view sender_public_key,
    std::string_view ecdh_shared_secret,
    std::string_view salt) const {
  std::string content_encryption_key_info =
      encryption_scheme_->GenerateInfoForContentEncoding(
          EncryptionScheme::EncodingType::CONTENT_ENCRYPTION_KEY,
          recipient_public_key, sender_public_key);

  return crypto::HkdfSha256(ecdh_shared_secret, salt,
                            content_encryption_key_info,
                            kContentEncryptionKeySize);
}

std::string GCMMessageCryptographer::DeriveNonce(
    std::string_view recipient_public_key,
    std::string_view sender_public_key,
    std::string_view ecdh_shared_secret,
    std::string_view salt) const {
  std::string nonce_info = encryption_scheme_->GenerateInfoForContentEncoding(
      EncryptionScheme::EncodingType::NONCE, recipient_public_key,
      sender_public_key);

  // https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-02
  // defines that the result should be XOR'ed with the record's sequence number,
  // however, Web Push encryption is limited to a single record per
  // https://tools.ietf.org/html/draft-ietf-webpush-encryption-03.

  return crypto::HkdfSha256(ecdh_shared_secret, salt, nonce_info, kNonceSize);
}

}  // namespace gcm
