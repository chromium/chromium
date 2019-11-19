// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace sync_pb {

class EncryptedData;
class NigoriKey;

}  // namespace sync_pb

namespace syncer {

class CryptographerImpl;

// Wrapper of CryptographerImpl, which contains only keystore keys and uses the
// last one as the default encryption key.
class KeystoreKeysCryptographer {
 public:
  // Factory methods.
  static std::unique_ptr<KeystoreKeysCryptographer> CreateEmpty();
  // Returns null if crypto error occurs.
  static std::unique_ptr<KeystoreKeysCryptographer> FromKeystoreKeys(
      const std::vector<std::string>& keystore_keys);

  ~KeystoreKeysCryptographer();

  const std::vector<std::string>& keystore_keys() const {
    return keystore_keys_;
  }

  // Returns name of Nigori key derived from last keystore key if !IsEmpty()
  // and empty string otherwise.
  std::string GetLastKeystoreKeyName() const;

  bool IsEmpty() const;

  std::unique_ptr<KeystoreKeysCryptographer> Clone() const;

  // Returns CryptographerImpl, which contains all keystore keys and uses the
  // last one as the default encryption key.
  std::unique_ptr<CryptographerImpl> ToCryptographerImpl() const;

  // Encrypts |keystore_decryptor_key| into |keystore_decryptor_token|.
  // |keystore_decryptor_token| must be not null. Returns false if there is no
  // keystore keys or crypto error occurs.
  bool EncryptKeystoreDecryptorToken(
      const sync_pb::NigoriKey& keystore_decryptor_key,
      sync_pb::EncryptedData* keystore_decryptor_token) const;

  // Decrypts |keystore_decryptor_token| into |keystore_decryptor_key|.
  // |keystore_decryptor_key| must be not null. Returns false if can't decrypt
  // or crypto error occurs.
  bool DecryptKeystoreDecryptorToken(
      const sync_pb::EncryptedData& keystore_decryptor_token,
      sync_pb::NigoriKey* keystore_decryptor_key) const;

 private:
  KeystoreKeysCryptographer(std::unique_ptr<CryptographerImpl> cryptographer,
                            const std::vector<std::string>& keystore_keys);

  std::unique_ptr<CryptographerImpl> cryptographer_;
  std::vector<std::string> keystore_keys_;

  DISALLOW_COPY_AND_ASSIGN(KeystoreKeysCryptographer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_
