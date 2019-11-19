// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_DIRECTORY_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_SYNCABLE_DIRECTORY_CRYPTOGRAPHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace sync_pb {
class NigoriKeyBag;
}  // namespace sync_pb

namespace syncer {

class Encryptor;

// The parameters used to initialize a Nigori instance.
// TODO(davidovic): Stop relying on KeyParams and inline it, because it's now
// just a pair of KeyDerivationParams and passphrase.
struct KeyParams {
  KeyParams(KeyDerivationParams derivation_params, const std::string& password);
  KeyParams(const KeyParams& other);
  KeyParams(KeyParams&& other);
  ~KeyParams();

  KeyDerivationParams derivation_params;
  std::string password;
};

struct CryptographerDataWithPendingKeys {
  CryptographerDataWithPendingKeys();
  CryptographerDataWithPendingKeys(CryptographerDataWithPendingKeys&& other);
  ~CryptographerDataWithPendingKeys();

  sync_pb::CryptographerData cryptographer_data;
  base::Optional<sync_pb::EncryptedData> pending_keys;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptographerDataWithPendingKeys);
};

// This class manages the Nigori objects used to encrypt and decrypt sensitive
// sync data (eg. passwords). Each Nigori object knows how to handle data
// protected with a particular passphrase.
//
// Whenever an update to the Nigori sync node is received from the server,
// SetPendingKeys should be called with the encrypted contents of that node.
// Most likely, an updated Nigori node means that a new passphrase has been set
// and that future node updates won't be decryptable. To remedy this, the user
// should be prompted for the new passphrase and DecryptPendingKeys be called.
//
// Whenever a update to an encrypted node is received from the server,
// CanDecrypt should be used to verify whether the Cryptographer can decrypt
// that node. If it cannot, then the application of that update should be
// delayed until after it can be decrypted.
class DirectoryCryptographer : public Cryptographer {
 public:
  DirectoryCryptographer();
  ~DirectoryCryptographer() override;

  void CopyFrom(const DirectoryCryptographer& other);

  // Deserialization.
  void InitFromCryptographerDataWithPendingKeys(
      const CryptographerDataWithPendingKeys& serialized_state);

  // Serialization.
  CryptographerDataWithPendingKeys ToCryptographerDataWithPendingKeys() const;

  // |restored_bootstrap_token| can be provided via this method to bootstrap
  // Cryptographer instance into the ready state (is_ready will be true).
  // It must be a string that was previously built by the
  // GetSerializedBootstrapToken function.  It is possible that the token is no
  // longer valid (due to server key change), in which case the normal
  // decryption code paths will fail and the user will need to provide a new
  // passphrase.
  // It is an error to call this if is_ready() == true, though it is fair to
  // never call Bootstrap at all.
  void Bootstrap(const Encryptor& encryptor,
                 const std::string& restored_bootstrap_token);

  // Returns whether |encrypted| can be decrypted using the default encryption
  // key.
  bool CanDecryptUsingDefaultKey(const sync_pb::EncryptedData& encrypted) const;

  // Encrypts the set of currently known keys into |encrypted|. Returns true if
  // successful.
  bool GetKeys(sync_pb::EncryptedData* encrypted) const;

  // Creates a new Nigori instance using |params|. If successful, |params| will
  // become the default encryption key and be used for all future calls to
  // Encrypt.
  // Will decrypt the pending keys and install them if possible (pending key
  // will not overwrite default).
  bool AddKey(const KeyParams& params);

  // Same as AddKey(..), but builds the new Nigori from a previously persisted
  // bootstrap token. This can be useful when consuming a bootstrap token
  // with a cryptographer that has already been initialized.
  // Updates the default key.
  // Will decrypt the pending keys and install them if possible (pending key
  // will not overwrite default).
  bool AddKeyFromBootstrapToken(const Encryptor& encryptor,
                                const std::string& restored_bootstrap_token);

  // Creates a new Nigori instance using |params|. If successful, |params|
  // will be added to the nigori keybag, but will not be the default encryption
  // key (default_nigori_ will remain the same).
  // Prereq: is_initialized() must be true.
  // Will decrypt the pending keys and install them if possible (pending key
  // will become the new default).
  bool AddNonDefaultKey(const KeyParams& params);

  // Decrypts |encrypted| and uses its contents to initialize Nigori instances.
  // Returns true unless decryption of |encrypted| fails. The caller is
  // responsible for checking that CanDecrypt(encrypted) == true.
  // Does not modify the default key.
  void InstallKeys(const sync_pb::EncryptedData& encrypted);

  // Makes a local copy of |encrypted| to later be decrypted by
  // DecryptPendingKeys. This should only be used if CanDecrypt(encrypted) ==
  // false.
  void SetPendingKeys(const sync_pb::EncryptedData& encrypted);

  // Makes |pending_keys_| available to callers that may want to cache its
  // value for later use on the UI thread. It is illegal to call this if the
  // cryptographer has no pending keys. Like other calls that access the
  // cryptographer, this method must be called from within a transaction.
  const sync_pb::EncryptedData& GetPendingKeys() const;

  // Attempts to decrypt the set of keys that was copied in the previous call to
  // SetPendingKeys using |params|. Returns true if the pending keys were
  // successfully decrypted and installed. If successful, the default key
  // is updated.
  bool DecryptPendingKeys(const KeyParams& params);

  // Sets the default key to the nigori with name |key_name|. |key_name| must
  // correspond to a nigori that has already been installed into the keybag.
  void SetDefaultKey(const std::string& key_name);

  bool is_initialized() const;

  // Obtain a token that can be provided on construction to a future
  // Cryptographer instance to bootstrap itself.  Returns false if such a token
  // can't be created (i.e. if this Cryptograhper doesn't have valid keys).
  bool GetBootstrapToken(const Encryptor& encryptor, std::string* token) const;

  // Returns true if |keybag| is decryptable and either is a subset of
  // |key_bag_| and/or has a different default key.
  bool KeybagIsStale(const sync_pb::EncryptedData& keybag) const;

  // Returns the name of the Nigori key currently used for encryption.
  std::string GetDefaultEncryptionKeyName() const override;

  // Returns a serialized sync_pb::NigoriKey version of current default
  // encryption key. Returns empty string if Cryptographer is not initialized
  // or protobuf serialization error occurs.
  std::string GetDefaultNigoriKeyData() const;

  // Generates a new Nigori from |serialized_nigori_key|, and if successful
  // installs the new nigori as the default key.
  bool ImportNigoriKey(const std::string& serialized_nigori_key);

  bool has_pending_keys() const;

  // Cryptographer overrides.
  std::unique_ptr<Cryptographer> Clone() const override;
  bool CanEncrypt() const override;
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const override;
  bool EncryptString(const std::string& serialized,
                     sync_pb::EncryptedData* encrypted) const override;
  bool DecryptToString(const sync_pb::EncryptedData& encrypted,
                       std::string* decrypted) const override;

 private:
  // Initializes cryptographer with completely provided state.
  DirectoryCryptographer(NigoriKeyBag key_bag,
                         const std::string& default_nigori_name,
                         std::unique_ptr<sync_pb::EncryptedData> pending_keys);

  // Helper method to instantiate Nigori instances for each set of key
  // parameters in |bag|.
  // Does not update the default nigori.
  void InstallKeyBag(const sync_pb::NigoriKeyBag& bag);

  // Helper method to add a nigori to the keybag, optionally making it the
  // default as well.
  bool AddKeyImpl(std::unique_ptr<Nigori> nigori, bool set_as_default);

  // Helper to unencrypt a bootstrap token into a serialized sync_pb::NigoriKey.
  std::string UnpackBootstrapToken(const Encryptor& encryptor,
                                   const std::string& token) const;

  // The actual keys we know about.
  NigoriKeyBag key_bag_;

  // The key name associated with the default nigori. If non-empty, must
  // correspond to a nigori within |key_bag_|.
  std::string default_nigori_name_;

  std::unique_ptr<sync_pb::EncryptedData> pending_keys_;

  DISALLOW_ASSIGN(DirectoryCryptographer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_DIRECTORY_CRYPTOGRAPHER_H_
