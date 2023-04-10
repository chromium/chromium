// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace os_crypt_async {

namespace {

constexpr size_t kNonceLength = 96 / 8;  // AES_GCM_NONCE_LENGTH

}  // namespace

Encryptor::Key::Key(base::span<const uint8_t> key,
                    const mojom::Algorithm& algorithm)
    : algorithm_(algorithm), key_(key.begin(), key.end()) {
  if (!algorithm_.has_value()) {
    NOTREACHED_NORETURN();
  }

  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM:
      CHECK_EQ(key.size(), Key::kAES256GCMKeySize);
      break;
  }
}

Encryptor::Key::Key(mojo::DefaultConstruct::Tag) {}

Encryptor::Key::Key(Key&& other) = default;
Encryptor::Key& Encryptor::Key::operator=(Key&& other) = default;

Encryptor::Key::~Key() = default;

Encryptor::Key Encryptor::Key::Clone() const {
  return Key(key_, *algorithm_);
}

Encryptor::Encryptor() = default;
Encryptor::Encryptor(mojo::DefaultConstruct::Tag) : Encryptor() {}

Encryptor::Encryptor(Encryptor&& other) = default;
Encryptor& Encryptor::operator=(Encryptor&& other) = default;

Encryptor::Encryptor(KeyRing keys, const std::string& provider_for_encryption)
    : keys_(std::move(keys)),
      provider_for_encryption_(provider_for_encryption) {}
Encryptor::~Encryptor() = default;

std::vector<uint8_t> Encryptor::Key::Encrypt(
    base::span<const uint8_t> plaintext) const {
  if (!algorithm_.has_value()) {
    NOTREACHED_NORETURN();
  }

  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM: {
      crypto::Aead aead(crypto::Aead::AES_256_GCM);
      aead.Init(key_);

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

absl::optional<std::vector<uint8_t>> Encryptor::Key::Decrypt(
    base::span<const uint8_t> ciphertext) const {
  if (!algorithm_.has_value()) {
    NOTREACHED_NORETURN();
  }
  switch (*algorithm_) {
    case mojom::Algorithm::kAES256GCM: {
      if (ciphertext.size() < kNonceLength) {
        return absl::nullopt;
      }
      crypto::Aead aead(crypto::Aead::AES_256_GCM);
      aead.Init(key_);

      // The nonce is at the start of the ciphertext and must be removed.
      auto nonce = ciphertext.subspan(0, kNonceLength);
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
                              std::string* plaintext) const {
  auto decrypted = DecryptData(base::as_bytes(base::make_span(ciphertext)));

  if (!decrypted.has_value()) {
    return false;
  }

  *plaintext = std::string(decrypted->begin(), decrypted->end());

  return true;
}

absl::optional<std::vector<uint8_t>> Encryptor::EncryptString(
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
    return absl::nullopt;
  }

  const auto& [provider, key] = *it;
  std::vector<uint8_t> ciphertext =
      key.Encrypt(base::as_bytes(base::make_span(data)));

  // This adds the provider prefix on the start of the data.
  ciphertext.insert(ciphertext.begin(), provider.cbegin(), provider.cend());

  return ciphertext;
}

absl::optional<std::string> Encryptor::DecryptData(
    base::span<const uint8_t> data) const {
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
        return std::string(plaintext->begin(), plaintext->end());
      }
    }
  }

  // No keys are loaded, or no suitable provider was found, or decryption
  // failed. Fallback to using legacy OSCrypt to attempt decryption.
  std::string string_data(data.begin(), data.end());
  std::string plaintext;
  if (OSCrypt::DecryptString(string_data, &plaintext)) {
    return plaintext;
  }

  return absl::nullopt;
}

Encryptor Encryptor::Clone() const {
  KeyRing keyring;
  for (const auto& [provider, key] : keys_) {
    keyring.emplace(provider, key.Clone());
  }

  return Encryptor(std::move(keyring), provider_for_encryption_);
}

}  // namespace os_crypt_async
