// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dpapi.h>

#include "components/os_crypt/async/common/encryptor_features.h"
#endif

namespace os_crypt_async {

namespace {

constexpr size_t kNonceLength = 96 / 8;  // AES_GCM_NONCE_LENGTH

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
  if (base::FeatureList::IsEnabled(features::kProtectEncryptionKey) &&
      !encrypted_) {
    encrypted_ = ::CryptProtectMemory(std::data(key_), std::size(key_),
                                      CRYPTPROTECTMEMORY_SAME_PROCESS);
  }
#endif
  CHECK(algorithm_.has_value());

  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM:
      CHECK_EQ(key.size(), Key::kAES256GCMKeySize);
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
  key.is_os_crypt_sync_compatible_ = is_os_crypt_sync_compatible_;
  return key;
}

Encryptor::Encryptor() = default;
Encryptor::Encryptor(mojo::DefaultConstruct::Tag) : Encryptor() {}

Encryptor::Encryptor(Encryptor&& other) = default;
Encryptor& Encryptor::operator=(Encryptor&& other) = default;

Encryptor::Encryptor(KeyRing keys, const std::string& provider_for_encryption)
    : keys_(std::move(keys)),
      provider_for_encryption_(provider_for_encryption) {
  // It is not permitted to have multiple keys that mark themselves as OSCrypt
  // sync compatible.
  bool already_found_os_crypt_compatible = false;
  for (const auto& key : keys_) {
    if (key.second.is_os_crypt_sync_compatible_) {
      CHECK(!already_found_os_crypt_compatible)
          << "Cannot have more than one key marked OSCrypt sync compatible.";
      already_found_os_crypt_compatible = true;
    }
  }
}
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
  auto decrypted =
      DecryptData(base::as_bytes(base::make_span(ciphertext)), flags);

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
  std::vector<uint8_t> ciphertext =
      key.Encrypt(base::as_bytes(base::make_span(data)));

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
    if (base::ranges::equal(provider, data.first(provider.size()))) {
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
    // If OSCrypt is using os_crypt_posix.cc and it's passed invalid data to
    // decrypt, it simply returns the data. This is a quirk of
    // os_crypt_posix.cc. In this case, it's not really possible to tell whether
    // or not encryption worked or not, and certainly not advisable to recommend
    // a re-encryption of this potentially invalid data.
    // TODO(crbug.com/365712505): Remove this fallback.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) &&         \
        !(BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_FUCHSIA)
    if (plaintext == string_data) {
      return plaintext;
    }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !(BUILDFLAG(IS_LINUX)
        // && !BUILDFLAG(IS_CASTOS)) || BUILDFLAG(IS_FUCHSIA)
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

  std::string provider_for_encryption;

  switch (option) {
    case Option::kNone:
      provider_for_encryption = provider_for_encryption_;
      break;
    case Option::kEncryptSyncCompat:
      for (const auto& [provider, key] : keyring) {
        if (key.is_os_crypt_sync_compatible_) {
          provider_for_encryption = provider;
          break;
        }
      }
      break;
  }

  // Can be empty provider, if no suitable provider is available.
  return Encryptor(std::move(keyring), provider_for_encryption);
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
