// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/sync/os_crypt.h"

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/sync/os_crypt_metrics.h"
#include "crypto/aes_cbc.h"

namespace {

// clang-format off
// PBKDF2-HMAC-SHA1(1 iteration, key = "peanuts", salt = "saltysalt")
constexpr auto kV10Key = std::to_array<uint8_t>({
    0xfd, 0x62, 0x1f, 0xe5, 0xa2, 0xb4, 0x02, 0x53,
    0x9d, 0xfa, 0x14, 0x7c, 0xa9, 0x27, 0x27, 0x78,
});

constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kIv{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

std::optional<bool> g_encryption_available_for_testing;

// clang-format on

// Prefix for cypher text returned by obfuscation version.  We prefix the
// cyphertext with this string so that future data migration can detect
// this and migrate to full encryption without data loss.
constexpr char kObfuscationPrefix[] = "v10";

}  // namespace

namespace OSCrypt {

void SetEncryptionAvailableForTesting(std::optional<bool> available) {
  g_encryption_available_for_testing = available;
}

}  // namespace OSCrypt

bool OSCrypt::EncryptString16(const std::u16string& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              std::u16string* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

// This is an obfuscation layer that does not provide any genuine
// confidentiality. It is used on OSes that already provide protection for data
// at rest some other way (Android, CrOS, and Fuchsia) or where there's no
// implementation available of platform secret store integration in Chromium
// (FreeBSD, others).
bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (!IsEncryptionAvailable()) {
    return false;
  }

  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  *ciphertext = kObfuscationPrefix;
  ciphertext->append(base::as_string_view(
      crypto::aes_cbc::Encrypt(kV10Key, kIv, base::as_byte_span(plaintext))));

  return true;
}

bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (!IsEncryptionAvailable()) {
    return false;
  }

  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // The incoming ciphertext was either obfuscated by OSCrypt::EncryptString()
  // as above, or is regular unobfuscated plaintext if it was imported from an
  // old version. Check for the obfuscation prefix to detect obfuscated text.
  const bool no_prefix_found = !base::StartsWith(ciphertext, kObfuscationPrefix,
                                                 base::CompareCase::SENSITIVE);

  os_crypt::LogEncryptionVersion(
      no_prefix_found ? os_crypt::EncryptionPrefixVersion::kNoVersion
                      : os_crypt::EncryptionPrefixVersion::kVersion10);

  if (no_prefix_found) {
    return false;
  }

  // Strip off the versioning prefix before decrypting.
  const std::string raw_ciphertext =
      ciphertext.substr(strlen(kObfuscationPrefix));

  std::optional<std::vector<uint8_t>> maybe_plain = crypto::aes_cbc::Decrypt(
      kV10Key, kIv, base::as_byte_span(raw_ciphertext));

  if (maybe_plain) {
    plaintext->assign(base::as_string_view(*maybe_plain));
  }

  return maybe_plain.has_value();
}

// static
bool OSCrypt::IsEncryptionAvailable() {
  return g_encryption_available_for_testing.value_or(true);
}

// static
void OSCrypt::SetRawEncryptionKey(const std::string& raw_key) {
  DCHECK(raw_key.empty());
}

// static
std::string OSCrypt::GetRawEncryptionKey() {
  return "";
}
