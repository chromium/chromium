// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_

#include <string>

#include "base/strings/string_piece_forward.h"

namespace password_manager {

// Username hash prefix length in bits.
constexpr size_t kUsernameHashPrefixLength = 24;

// Canonicalizes |username| by lower-casing and and stripping a mail-address
// host in case the username is a mail address. |username| must be a UTF-8
// string.
std::string CanonicalizeUsername(base::StringPiece username);

// Hashes |canonicalized_username| by appending a fixed salt and computing the
// SHA256 hash.
std::string HashUsername(base::StringPiece canonicalized_username);

// Bucketizes |canonicalized_username| by hashing it and returning a prefix of
// |kUsernameHashPrefixLength| bits.
std::string BucketizeUsername(base::StringPiece canonicalized_username);

// Produces the username/password pair hash using scrypt algorithm.
// |canonicalized_username| and |password| are UTF-8 strings.
std::string ScryptHashUsernameAndPassword(
    base::StringPiece canonicalized_username,
    base::StringPiece password);

// Encrypt/decrypt routines.

// Encrypts |plaintext| with a new key. The key is returned via |key|.
// Internally the function does some hashing first and then encrypts the result.
std::string CipherEncrypt(const std::string& plaintext, std::string* key);

// Encrypts |plaintext| with the existing key.
std::string CipherEncryptWithKey(const std::string& plaintext,
                                 const std::string& key);

// |already_encrypted| is an already encrypted string (output of CipherEncrypt).
// Encrypts it again with a new key. The key is returned in |key|.
// The function is different from CipherEncrypt() as it doesn't apply hashing on
// the input.
std::string CipherReEncrypt(const std::string& already_encrypted,
                            std::string* key);

// Decrypts |ciphertext| using |key|. The result isn't the original string but a
// hash of it.
std::string CipherDecrypt(const std::string& ciphertext,
                          const std::string& key);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
