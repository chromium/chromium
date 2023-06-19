// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_KEY_BAG_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_KEY_BAG_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace sync_pb {
class EncryptedData;
class NigoriKey;
class NigoriKeyBag;
}  // namespace sync_pb

namespace syncer {

class Nigori;

// A set of Nigori keys, aka keybag. Note that there is no notion of default
// key.
class NigoriKeyBag {
 public:
  static NigoriKeyBag CreateEmpty();
  // Deserialization from proto.
  static NigoriKeyBag CreateFromProto(const sync_pb::NigoriKeyBag& key_bag);

  NigoriKeyBag(NigoriKeyBag&& other);
  ~NigoriKeyBag();

  NigoriKeyBag& operator=(NigoriKeyBag&&) = default;

  void CopyFrom(const NigoriKeyBag& other);

  // Serialization to proto.
  sync_pb::NigoriKeyBag ToProto() const;

  // Makes a deep copy of |*this|.
  NigoriKeyBag Clone() const;

  size_t size() const;
  bool HasKey(const std::string& key_name) const;
  bool HasKeyPair(const uint32_t key_version) const;

  // |key_name| must exist in this keybag.
  sync_pb::NigoriKey ExportKey(const std::string& key_name) const;

  // Adds a new key to the keybag. Returns the name of the key or an empty
  // string in case of failure.
  std::string AddKey(std::unique_ptr<Nigori> nigori);

  // Similar to AddKey(), but reads the key material from a proto. The |name|
  // field is ignored since it's redundant.
  std::string AddKeyFromProto(const sync_pb::NigoriKey& key);

  // Merges all keys from another keybag, which means adding all keys that we
  // don't know about.
  void AddAllUnknownKeysFrom(const NigoriKeyBag& other);

  // Adds a Public-private key-pair to the keybag associated with |version|.
  void AddKeyPair(CrossUserSharingPublicPrivateKeyPair key_pair,
                  uint32_t version);

  // Similar to AddKeyPair, but reads the private-key material from a proto and
  // derives the public-key from the private-key.
  bool AddKeyPairFromProto(const sync_pb::CrossUserSharingPrivateKey& key);

  // Encryption of strings (possibly binary). Returns true if success.
  // |key_name| must be known. |encrypted_output| must not be null.
  bool EncryptWithKey(const std::string& key_name,
                      const std::string& input,
                      sync_pb::EncryptedData* encrypted_output) const;

  // Returns whether the key required to decrypt |encrypted_input| is known.
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted_input) const;

  // Decryption of strings (possibly binary). Returns true if success.
  // |decrypted_output| must not be null.
  bool Decrypt(const sync_pb::EncryptedData& encrypted_input,
               std::string* decrypted_output) const;

 private:
  NigoriKeyBag();

  // The Nigoris we know about, mapped by key name.
  std::map<std::string, std::unique_ptr<const Nigori>> nigori_map_;

  // Public-private key-pairs we know about, mapped by version.
  std::map<uint32_t, const CrossUserSharingPublicPrivateKeyPair> key_pairs_map_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_KEY_BAG_H_
