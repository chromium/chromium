// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/rfc8188_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/keypair.h"
#include "crypto/random.h"

namespace gcm {

BASE_FEATURE(kRfc8188StrictCompliance, base::FEATURE_ENABLED_BY_DEFAULT);

base::expected<std::string, Rfc8188EncryptionError> EncryptPayloadWithRfc8188(
    std::string_view message,
    std::string_view p256dh,
    std::string_view auth_secret,
    const crypto::keypair::PrivateKey& sender_private_key) {
  // Validate recipient public key and auth secret sizes to avoid crashing debug
  // builds. Per SEC 1 (ANSI X9.62), an uncompressed P-256 point is 65 bytes
  // starting with 0x04. Per RFC 8291 Section 3.2, auth secret is 16 bytes.
  if (base::FeatureList::IsEnabled(kRfc8188StrictCompliance)) {
    if (p256dh.size() != 65 || p256dh[0] != 0x04) {
      DLOG(ERROR) << "Invalid recipient public key. Size: " << p256dh.size();
      return base::unexpected(Rfc8188EncryptionError::kEncryptionFailed);
    }
    if (auth_secret.size() != 16) {
      DLOG(ERROR) << "Invalid auth secret size: " << auth_secret.size();
      return base::unexpected(Rfc8188EncryptionError::kEncryptionFailed);
    }
  }

  // Creates a cryptographically secure salt of 16 octets in size, and
  // calculates the shared secret for the message.
  std::string salt(16, '\0');
  crypto::RandBytes(base::as_writable_byte_span(salt));

  std::string shared_secret;
  if (!ComputeSharedP256Secret(sender_private_key, p256dh, &shared_secret)) {
    DLOG(ERROR) << "Unable to calculate the shared secret.";
    return base::unexpected(Rfc8188EncryptionError::kKeyDerivationFailed);
  }

  size_t record_size = 0;
  std::string ciphertext;

  GCMMessageCryptographer cryptographer(
      GCMMessageCryptographer::Version::DRAFT_08);

  std::string sender_public_key(
      base::as_string_view(sender_private_key.ToUncompressedX962Point()));
  if (!cryptographer.Encrypt(p256dh, sender_public_key, shared_secret,
                             auth_secret, salt, message, &record_size,
                             &ciphertext)) {
    DLOG(ERROR) << "Encryption failed.";
    return base::unexpected(Rfc8188EncryptionError::kEncryptionFailed);
  }

  // Construct encryption header.
  // GCMMessageCryptographer's advertised record_size does not include the
  // 16-byte AEAD tag, making it smaller than the actual record size for large
  // payloads. To comply with RFC 8188, rs must be >= the size of the record
  // (including tag).
  uint32_t rs = base::FeatureList::IsEnabled(kRfc8188StrictCompliance)
                    ? base::checked_cast<uint32_t>(
                          std::max(record_size, ciphertext.size()))
                    : base::checked_cast<uint32_t>(record_size);
  std::string rs_str(sizeof(rs), 0u);
  base::as_writable_byte_span(rs_str).copy_from(base::U32ToBigEndian(rs));

  uint8_t key_length = base::checked_cast<uint8_t>(sender_public_key.size());
  std::string key_length_str(sizeof(key_length), 0u);
  base::as_writable_byte_span(key_length_str)
      .copy_from(base::U8ToBigEndian(key_length));

  std::string payload = base::StrCat(
      {salt, rs_str, key_length_str, sender_public_key, ciphertext});

  return payload;
}

}  // namespace gcm
