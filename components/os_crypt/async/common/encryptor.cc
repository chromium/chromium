// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/aead.h"
#include "crypto/aes_cbc.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dpapi.h>
#endif

namespace os_crypt_async {

namespace {

constexpr size_t kNonceLength = 96 / 8;  // AES_GCM_NONCE_LENGTH

constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kFixedIvForAes128Cbc{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

}  // namespace

Encryptor::Key::Key(base::span<const uint8_t> key,
                    const mojom::Algorithm& algorithm,
                    bool encrypted)
    : algorithm_(algorithm),
      key_(key.begin(), key.end())
#if BUILDFLAG(IS_WIN)
      ,
      encrypted_(encrypted)
#endif
{
#if BUILDFLAG(IS_WIN)
  if (!encrypted_) {
    encrypted_ = ::CryptProtectMemory(std::data(key_), std::size(key_),
                                      CRYPTPROTECTMEMORY_SAME_PROCESS);
  }
#endif
  CHECK(algorithm_.has_value());

  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM:
      CHECK_EQ(key.size(), Key::kAES256GCMKeySize);
      break;
    case mojom::Algorithm::kAES128CBC:
      CHECK_EQ(key.size(), Key::kAES128CBCKeySize);
      break;
  }
}

Encryptor::Key::Key(base::span<const uint8_t> key,
                    const mojom::Algorithm& algorithm)
    : Key(key, algorithm, /*encrypted=*/false) {}

Encryptor::Key::Key(mojo::DefaultConstruct::Tag) {}

Encryptor::Key::Key(Key&& other) = default;
Encryptor::Key& Encryptor::Key::operator=(Key&& other) = default;

Encryptor::Key::~Key() = default;

Encryptor::Key Encryptor::Key::Clone() const {
#if BUILDFLAG(IS_WIN)
  Encryptor::Key key(key_, *algorithm_, encrypted_);
#else
  Encryptor::Key key(key_, *algorithm_, /*encrypted=*/false);
#endif
  return key;
}

Encryptor::Encryptor() = default;
Encryptor::Encryptor(mojo::DefaultConstruct::Tag) : Encryptor() {}

Encryptor::Encryptor(Encryptor&& other) = default;
Encryptor& Encryptor::operator=(Encryptor&& other) = default;

Encryptor::Encryptor(
    KeyRing keys,
    const std::string& provider_for_encryption,
    const std::string& provider_for_os_crypt_sync_compatible_encryption)
    : keys_(std::move(keys)),
      provider_for_encryption_(provider_for_encryption),
      provider_for_os_crypt_sync_compatible_encryption_(
          provider_for_os_crypt_sync_compatible_encryption) {}
Encryptor::~Encryptor() = default;

std::vector<uint8_t> Encryptor::Key::Encrypt(
    base::span<const uint8_t> plaintext) const {
  CHECK(algorithm_.has_value());

  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM: {
      crypto::Aead aead(crypto::Aead::AES_256_GCM);
      base::span<const uint8_t> key(key_);
#if BUILDFLAG(IS_WIN)
      // Copy. This makes it thread safe. Must outlive aead.
      std::vector<uint8_t> decrypted_key(key_);
      absl::Cleanup zero_memory = [&decrypted_key] {
        ::SecureZeroMemory(decrypted_key.data(), decrypted_key.size());
      };

      if (encrypted_) {
        ::CryptUnprotectMemory(std::data(decrypted_key),
                               std::size(decrypted_key),
                               CRYPTPROTECTMEMORY_SAME_PROCESS);
        key = base::span<const uint8_t>(decrypted_key);
      }
#endif  // BUILDFLAG(IS_WIN)
      aead.Init(key);

      // Note: can only check this once AEAD is initialized.
      DCHECK_EQ(kNonceLength, aead.NonceLength());

      std::array<uint8_t, kNonceLength> nonce = {};
      crypto::RandBytes(nonce);
      std::vector<uint8_t> ciphertext =
          aead.Seal(plaintext, nonce, /*additional_data=*/{});

      // Nonce goes at the front of the ciphertext.
      ciphertext.insert(ciphertext.begin(), nonce.cbegin(), nonce.cend());
      return ciphertext;
    }
    case mojom::Algorithm::kAES128CBC: {
      std::vector<uint8_t> ciphertext = crypto::aes_cbc::Encrypt(
          key_, base::as_byte_span(kFixedIvForAes128Cbc), plaintext);
      return ciphertext;
    }
  }
  LOG(FATAL) << "Unsupported algorithm" << static_cast<int>(*algorithm_);
}

std::optional<std::vector<uint8_t>> Encryptor::Key::Decrypt(
    base::span<const uint8_t> ciphertext) const {
  CHECK(algorithm_.has_value());
  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM: {
      if (ciphertext.size() < kNonceLength) {
        return std::nullopt;
      }
      crypto::Aead aead(crypto::Aead::AES_256_GCM);

      base::span<const uint8_t> key(key_);
#if BUILDFLAG(IS_WIN)
      // Copy. This makes it thread safe. Must outlive aead.
      std::vector<uint8_t> decrypted_key(key_);
      absl::Cleanup zero_memory = [&decrypted_key] {
        ::SecureZeroMemory(decrypted_key.data(), decrypted_key.size());
      };
      if (encrypted_) {
        ::CryptUnprotectMemory(std::data(decrypted_key),
                               std::size(decrypted_key),
                               CRYPTPROTECTMEMORY_SAME_PROCESS);
        key = base::span<const uint8_t>(decrypted_key);
      }
#endif  // BUILDFLAG(IS_WIN)
      aead.Init(key);

      // The nonce is at the start of the ciphertext and must be removed.
      auto nonce = ciphertext.first(kNonceLength);
      auto data = ciphertext.subspan(kNonceLength);

      return aead.Open(data, nonce, /*additional_data=*/{});
    }
    case mojom::Algorithm::kAES128CBC: {
      auto plaintext =
          crypto::aes_cbc::Decrypt(key_, kFixedIvForAes128Cbc, ciphertext);
      if (plaintext.has_value()) {
        return plaintext;
      }
      // Decryption failed - try the empty fallback key. See
      // https://crbug.com/40055416.
      // PBKDF2-HMAC-SHA1(1 iteration, key = "", salt = "saltysalt")
      constexpr auto kEmptyKey = std::to_array<uint8_t>(
          {0xd0, 0xd0, 0xec, 0x9c, 0x7d, 0x77, 0xd4, 0x3a, 0xc5, 0x41, 0x87,
           0xfa, 0x48, 0x18, 0xd1, 0x7f});
      return crypto::aes_cbc::Decrypt(kEmptyKey, kFixedIvForAes128Cbc,
                                      ciphertext);
    }
  }
  LOG(FATAL) << "Unsupported algorithm" << static_cast<int>(*algorithm_);
}

bool Encryptor::EncryptString(const std::string& plaintext,
                              std::string* ciphertext) const {
  auto encrypted = EncryptString(plaintext);

  if (!encrypted.has_value()) {
    return false;
  }

  *ciphertext = std::string(encrypted->begin(), encrypted->end());

  return true;
}

bool Encryptor::DecryptString(const std::string& ciphertext,
                              std::string* plaintext,
                              DecryptFlags* flags) const {
  auto decrypted = DecryptData(base::as_byte_span(ciphertext), flags);

  if (!decrypted.has_value()) {
    return false;
  }

  *plaintext = std::string(decrypted->begin(), decrypted->end());

  return true;
}

std::optional<std::vector<uint8_t>> Encryptor::EncryptString(
    const std::string& data) const {
  if (data.empty()) {
    return std::vector<uint8_t>();
  }

  const auto& it = keys_.find(provider_for_encryption_);

  if (it == keys_.end()) {
    // This can happen if there is no default provider, or `keys_` is empty. In
    // this case, fall back to legacy OSCrypt encryption.
    std::string ciphertext;
    if (OSCrypt::EncryptString(data, &ciphertext)) {
      return std::vector<uint8_t>(ciphertext.cbegin(), ciphertext.cend());
    }
    return std::nullopt;
  }

  const auto& [provider, key] = *it;
  std::vector<uint8_t> ciphertext = key.Encrypt(base::as_byte_span(data));

  // This adds the provider prefix on the start of the data.
  ciphertext.insert(ciphertext.begin(), provider.cbegin(), provider.cend());

  return ciphertext;
}

std::optional<std::string> Encryptor::DecryptData(
    base::span<const uint8_t> data,
    DecryptFlags* flags) const {
  if (flags) {
    flags->should_reencrypt = false;
  }

  if (data.empty()) {
    return std::string();
  }

  for (const auto& [provider, key] : keys_) {
    if (data.size() < provider.size()) {
      continue;
    }
    if (std::ranges::equal(provider, data.first(provider.size()))) {
      // This removes the provider prefix from the front of the data.
      auto ciphertext = data.subspan(provider.size());
      // The Key does the raw decrypt.
      auto plaintext = key.Decrypt(ciphertext);
      if (plaintext) {
        if (flags) {
          flags->should_reencrypt = provider != provider_for_encryption_;
        }
        return std::string(plaintext->begin(), plaintext->end());
      }
    }
  }

  // No keys are loaded, or no suitable provider was found, or decryption
  // failed. Fallback to using legacy OSCrypt to attempt decryption.
  std::string string_data(data.begin(), data.end());
  std::string plaintext;
  if (OSCrypt::DecryptString(string_data, &plaintext)) {
    // If fallback to OSCrypt happened but there is a valid key provider, then
    // recommend re-encryption.
    if (!provider_for_encryption_.empty() && flags) {
      flags->should_reencrypt = true;
    }
    return plaintext;
  }

  return std::nullopt;
}

bool Encryptor::EncryptString16(const std::u16string& plaintext,
                                std::string* ciphertext) const {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool Encryptor::DecryptString16(const std::string& ciphertext,
                                std::u16string* plaintext,
                                DecryptFlags* flags) const {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8, flags)) {
    return false;
  }

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

Encryptor Encryptor::Clone(Option option) const {
  KeyRing keyring;
  for (const auto& [provider, key] : keys_) {
    keyring.emplace(provider, key.Clone());
  }

  switch (option) {
    case Option::kNone:
      return Encryptor(std::move(keyring), provider_for_encryption_,
                       provider_for_os_crypt_sync_compatible_encryption_);
    case Option::kEncryptSyncCompat:
      return Encryptor(std::move(keyring),
                       provider_for_os_crypt_sync_compatible_encryption_,
                       provider_for_os_crypt_sync_compatible_encryption_);
  }
}

bool Encryptor::IsEncryptionAvailable() const {
  if (!provider_for_encryption_.empty() &&
      keys_.contains(provider_for_encryption_)) {
    return true;
  }

  return OSCrypt::IsEncryptionAvailable();
}

bool Encryptor::IsDecryptionAvailable() const {
  if (!keys_.empty()) {
    return true;
  }

  return OSCrypt::IsEncryptionAvailable();
}

}  // namespace os_crypt_async
