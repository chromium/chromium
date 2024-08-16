// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cross_user_sharing_keys.h"
#include "components/sync/nigori/nigori_key_bag.h"

namespace sync_pb {
class CryptographerData;
class EncryptedData;
class NigoriKey;
}  // namespace sync_pb

namespace syncer {

// This class manages the Nigori objects used to encrypt and decrypt sensitive
// sync data (eg. passwords). Each Nigori object knows how to handle data
// protected with a particular encryption key.
//
// The implementation consists of a keybag representing all known keys and
// optionally the notion of a default encryption key which, if it exists, is
// present in the keybag.
class CryptographerImpl : public Cryptographer {
 public:
  // Factory methods.
  static std::unique_ptr<CryptographerImpl> CreateEmpty();
  static std::unique_ptr<CryptographerImpl> FromSingleKeyForTesting(
      const std::string& passphrase,
      const KeyDerivationParams& derivation_params =
          KeyDerivationParams::CreateForPbkdf2());
  // Returns null in case of error (e.g. default key not present in keybag).
  static std::unique_ptr<CryptographerImpl> FromProto(
      const sync_pb::CryptographerData& proto);

  CryptographerImpl& operator=(const CryptographerImpl&) = delete;

  ~CryptographerImpl() override;

  // Serialization.
  sync_pb::CryptographerData ToProto() const;

  // Creates and registers a new key after deriving Nigori keys. Returns the
  // name of the key, or an empty string in case of error. Note that emplacing
  // an already-known key is not considered an error (just a no-op).
  //
  // Does NOT set or change the default encryption key.
  std::string EmplaceKey(const std::string& passphrase,
                         const KeyDerivationParams& derivation_params);

  // Adds all keys from |key_bag| that weren't previously known.
  //
  // Does NOT set or change the default encryption key.
  void EmplaceKeysFrom(const NigoriKeyBag& key_bag);

  // Drops any pre-existing key pairs and adds all keys from |keys|.
  void ReplaceCrossUserSharingKeys(CrossUserSharingKeys keys);

  // Adds the given Public-private key-pair associated with |version|. Replaces
  // any pre-existing key pair for the given version if exists.
  void SetKeyPair(CrossUserSharingPublicPrivateKeyPair key_pair,
                  uint32_t version);

  // Sets or changes the default encryption key, which causes CanEncrypt() to
  // return true. |key_name| must not be empty and must represent a known key.
  void SelectDefaultEncryptionKey(const std::string& key_name);

  // Adds all Nigori keys in |other| that weren't previously known.
  void EmplaceAllNigoriKeysFrom(const CryptographerImpl& other);

  // Clears the default encryption key, which causes CanEncrypt() to return
  // false.
  void ClearDefaultEncryptionKey();

  // Reverts the cryptographer to an empty one, i.e. what would be returned by
  // CreateEmpty(). The default key is also cleared.
  // The set of known encryption keys shouldn't decrease in general, since this
  // may lead to data becoming undecryptable. This method can be called a) if
  // sync is disabled, or b) if sync finds an error that can be solved by
  // resetting the encryption state.
  void ClearAllKeys();

  // Determines whether |key_name| represents a known key.
  bool HasKey(const std::string& key_name) const;

  // Determines whether |key_pair_version| represents a known Public-private
  // key-pair.
  bool HasKeyPair(uint32_t key_pair_version) const;

  // Returns the number of generated key pairs.
  size_t KeyPairSizeForMetrics() const;

  // Returns a key pair for a given `version`. The key pair with the given
  // `version` must exist.
  const CrossUserSharingPublicPrivateKeyPair& GetCrossUserSharingKeyPair(
      uint32_t version) const;

  // Sets or changes the version of the default cross user sharing key.
  void SelectDefaultCrossUserSharingKey(const uint32_t version);

  // Returns a proto representation of the default encryption key. |*this| must
  // have a default encryption key set, as reflected by CanEncrypt().
  sync_pb::NigoriKey ExportDefaultKey() const;

  std::unique_ptr<CryptographerImpl> Clone() const;

  size_t KeyBagSizeForTesting() const;

  // Cryptographer overrides.
  bool CanEncrypt() const override;
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const override;
  std::string GetDefaultEncryptionKeyName() const override;
  bool EncryptString(const std::string& decrypted,
                     sync_pb::EncryptedData* encrypted) const override;
  bool DecryptToString(const sync_pb::EncryptedData& encrypted,
                       std::string* decrypted) const override;
  std::optional<std::vector<uint8_t>> AuthEncryptForCrossUserSharing(
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> recipient_public_key) const override;
  std::optional<std::vector<uint8_t>> AuthDecryptForCrossUserSharing(
      base::span<const uint8_t> encrypted_data,
      base::span<const uint8_t> sender_public_key,
      const uint32_t recipient_key_version) const override;

 private:
  CryptographerImpl(NigoriKeyBag key_bag,
                    std::string default_encryption_key_name,
                    CrossUserSharingKeys cross_user_sharing_keys);

  // The actual keys we know about.
  NigoriKeyBag key_bag_;

  // The key name associated with the default encryption key. If non-empty, it
  // must correspond to a key within |key_bag_|. May be empty even if |key_bag_|
  // is not.
  std::string default_encryption_key_name_;

  // The version of the default cross user sharing key to be used for
  // encryption.
  std::optional<uint32_t> default_cross_user_sharing_key_version_;

  // Cross user sharing keys we know about.
  CrossUserSharingKeys cross_user_sharing_keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_IMPL_H_
