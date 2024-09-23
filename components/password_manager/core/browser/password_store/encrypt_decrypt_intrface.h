// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ENCRYPT_DECRYPT_INTRFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ENCRYPT_DECRYPT_INTRFACE_H_

#include <string>

namespace password_manager {

// Result values for encryption/decryption actions.
enum class EncryptionResult {
  // Success.
  kSuccess,
  // Failure for a specific item (e.g., the encrypted value was manually
  // moved from another machine, and can't be decrypted on this machine).
  // This is presumed to be a permanent failure.
  kItemFailure,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  // This is presumed to be a temporary failure.
  kServiceFailure,
};

class EncryptDecryptInterface {
 public:
  // Encrypts plain_text, setting the value of cipher_text and returning true if
  // successful, or returning false and leaving cipher_text unchanged if
  // encryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  [[nodiscard]] virtual EncryptionResult EncryptedString(
      const std::u16string& plain_text,
      std::string* cipher_text) const = 0;

  // Decrypts cipher_text, setting the value of plain_text and returning true if
  // successful, or returning false and leaving plain_text unchanged if
  // decryption fails (e.g., if the underlying OS encryption system is
  // temporarily unavailable).
  [[nodiscard]] virtual EncryptionResult DecryptedString(
      const std::string& cipher_text,
      std::u16string* plain_text) const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_ENCRYPT_DECRYPT_INTRFACE_H_
