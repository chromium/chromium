// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/common/test_encryptor.h"

#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

TestEncryptor::TestEncryptor(
    KeyRing keys,
    const std::string& provider_for_encryption,
    const std::string& provider_for_os_crypt_sync_compatible_encryption)
    : Encryptor(std::move(keys),
                provider_for_encryption,
                provider_for_os_crypt_sync_compatible_encryption) {}

TestEncryptor TestEncryptor::Clone(Option option) const {
  auto cloned = Encryptor::Clone(option);
  return TestEncryptor(
      std::move(cloned.keys_), cloned.provider_for_encryption_,
      cloned.provider_for_os_crypt_sync_compatible_encryption_);
}

bool TestEncryptor::IsEncryptionAvailable() const {
  return is_encryption_available_.value_or(Encryptor::IsEncryptionAvailable());
}

bool TestEncryptor::IsDecryptionAvailable() const {
  return is_decryption_available_.value_or(Encryptor::IsDecryptionAvailable());
}

}  // namespace os_crypt_async
