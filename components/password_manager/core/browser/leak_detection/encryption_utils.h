// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

namespace password_manager {

// Username hash prefix length in bits.
inline constexpr size_t kUsernameHashPrefixLength = 26;

// Canonicalizes |username| by lower-casing and and stripping a mail-address
// host in case the username is a mail address. |username| must be a UTF-8
// string.
std::string CanonicalizeUsername(std::string_view username);
std::u16string CanonicalizeUsername(std::u16string_view username);

// Hashes |canonicalized_username| by appending a fixed salt and computing the
// SHA256 hash.
std::string HashUsername(std::string_view canonicalized_username);

// Bucketizes |canonicalized_username| by hashing it and returning a prefix of
// |kUsernameHashPrefixLength| bits.
std::string BucketizeUsername(std::string_view canonicalized_username);

// Produces the username/password pair hash using scrypt algorithm.
// |canonicalized_username| and |password| are UTF-8 strings.
// Returns nullopt in case of encryption failure.
std::optional<std::string> ScryptHashUsernameAndPassword(
    std::string_view canonicalized_username,
    std::string_view password);

// Encrypt/decrypt routines.

// Encrypts |plaintext| with a new key. The key is returned via |key|.
// Internally the function does some hashing first and then encrypts the result.
// In case of an encryption failure this returns nullopt and does not modify
// |key|.
std::optional<std::string> CipherEncrypt(const std::string& plaintext,
                                         std::string* key);

// Encrypts |plaintext| with the existing key.
// Returns nullopt in case of encryption failure.
std::optional<std::string> CipherEncryptWithKey(const std::string& plaintext,
                                                const std::string& key);

// |already_encrypted| is an already encrypted string (output of CipherEncrypt).
// Encrypts it again with a new key. The key is returned in |key|.
// The function is different from CipherEncrypt() as it doesn't apply hashing on
// the input.
// In case of an encryption failure this returns nullopt and does not modify
// |key|.
std::optional<std::string> CipherReEncrypt(const std::string& already_encrypted,
                                           std::string* key);

// Decrypts |ciphertext| using |key|. The result isn't the original string but a
// hash of it.
// Returns nullopt in case of decryption failure.
std::optional<std::string> CipherDecrypt(const std::string& ciphertext,
                                         const std::string& key);

// Returns a new key suitable for the encryption functions above, or nullopt if
// the operation failed.
std::optional<std::string> CreateNewKey();

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
