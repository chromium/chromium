// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_COMMON_TEST_ENCRYPTOR_H_
#define COMPONENTS_OS_CRYPT_ASYNC_COMMON_TEST_ENCRYPTOR_H_

#include <optional>

#include "components/os_crypt/async/common/encryptor.h"

namespace os_crypt_async {

// An instance of this class can be obtained by calling
// GetTestEncryptorForTesting. It can be used to provide control over whether or
// not encryption services are available to higher level tests.
class TestEncryptor : public Encryptor {
 public:
  TestEncryptor() = delete;

  // Encryptor overrides.
  bool IsEncryptionAvailable() const override;
  bool IsDecryptionAvailable() const override;

  // Override whether `IsEncryptionAvailable` returns true or false for the use
  // of this Encryptor in the current process.
  void set_encryption_available_for_testing(std::optional<bool> available) {
    is_encryption_available_ = available;
  }

  // Override whether `IsDecryptionAvailable` returns true or false for the use
  // of this Encryptor in the current process.
  void set_decryption_available_for_testing(std::optional<bool> available) {
    is_decryption_available_ = available;
  }

 private:
  friend class TestOSCryptAsync;

  TestEncryptor(
      KeyRing keys,
      const std::string& provider_for_encryption,
      const std::string& provider_for_os_crypt_sync_compatible_encryption);

  TestEncryptor Clone(Option option) const;

  std::optional<bool> is_encryption_available_;
  std::optional<bool> is_decryption_available_;
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_COMMON_TEST_ENCRYPTOR_H_
