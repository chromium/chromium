// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/nigori/nigori_key_bag.h"

namespace sync_pb {

class EncryptedData;
class NigoriKey;
}  // namespace sync_pb

namespace syncer {

// Manages keystore keys and provides operations to encrypt and decrypt the
// keystore decryptor token using the last keystore key.
class KeystoreKeysCryptographer {
 public:
  // Factory methods. Never return null.
  static std::unique_ptr<KeystoreKeysCryptographer> CreateEmpty();
  static std::unique_ptr<KeystoreKeysCryptographer> FromKeystoreKeys(
      const std::vector<std::string>& keystore_keys);

  KeystoreKeysCryptographer(const KeystoreKeysCryptographer&) = delete;
  KeystoreKeysCryptographer& operator=(const KeystoreKeysCryptographer&) =
      delete;

  ~KeystoreKeysCryptographer();

  const std::vector<std::string>& keystore_keys() const {
    return keystore_keys_;
  }

  bool IsEmpty() const;

  std::unique_ptr<KeystoreKeysCryptographer> Clone() const;

  // Returns the key name of the last keystore key, or an empty string if
  // `IsEmpty()` is true.
  const std::string& GetLastKeystoreKeyName() const;

  // Encrypts `keystore_decryptor_key` into `keystore_decryptor_token`.
  // `keystore_decryptor_token` must be not null. Returns false if there is no
  // keystore keys or crypto error occurs.
  bool EncryptKeystoreDecryptorToken(
      const sync_pb::NigoriKey& keystore_decryptor_key,
      sync_pb::EncryptedData* keystore_decryptor_token) const;

  // Decrypts `keystore_decryptor_token` into `keystore_decryptor_key`.
  // `keystore_decryptor_key` must be not null. Returns false if can't decrypt
  // or crypto error occurs.
  bool DecryptKeystoreDecryptorToken(
      const sync_pb::EncryptedData& keystore_decryptor_token,
      sync_pb::NigoriKey* keystore_decryptor_key) const;

  // Returns NigoriKeyBag that contains keys derived from all keystore keys.
  const NigoriKeyBag& GetKeystoreKeybag() const;

 private:
  KeystoreKeysCryptographer(NigoriKeyBag key_bag,
                            const std::string& last_keystore_key_name,
                            const std::vector<std::string>& keystore_keys);

  // Contains all keys derived from `keystore_keys_`.
  NigoriKeyBag key_bag_;

  // Used to EncryptKeystoreDecryptorToken(). Empty if there is no
  // `keystore_keys_`.
  std::string last_keystore_key_name_;

  std::vector<std::string> keystore_keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_KEYSTORE_KEYS_CRYPTOGRAPHER_H_
