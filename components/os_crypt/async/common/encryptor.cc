// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/encryptor.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace os_crypt_async {

Encryptor::Encryptor(Encryptor&& other) = default;
Encryptor& Encryptor::operator=(Encryptor&& other) = default;

Encryptor::Encryptor() = default;

Encryptor::~Encryptor() = default;

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

  std::string string_data(data.begin(), data.end());
  std::string ciphertext;
  if (OSCrypt::EncryptString(string_data, &ciphertext)) {
    return std::vector<uint8_t>(ciphertext.cbegin(), ciphertext.cend());
  }

  return absl::nullopt;
}

absl::optional<std::string> Encryptor::DecryptData(
    base::span<const uint8_t> data) const {
  if (data.empty()) {
    return std::string();
  }

  std::string string_data(data.begin(), data.end());
  std::string plaintext;
  if (OSCrypt::DecryptString(string_data, &plaintext)) {
    return plaintext;
  }

  return absl::nullopt;
}

Encryptor Encryptor::Clone() const {
  return Encryptor();
}

}  // namespace os_crypt_async
