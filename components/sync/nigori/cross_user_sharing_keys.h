// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_CROSS_USER_SHARING_KEYS_H_
#define COMPONENTS_SYNC_NIGORI_CROSS_USER_SHARING_KEYS_H_

#include <map>
#include <memory>
#include <string>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

namespace sync_pb {
class CrossUserSharingKeys;
class CrossUserSharingPrivateKey;
}  // namespace sync_pb

namespace syncer {

// A set of Public-private key pairs. Note that there is no notion of default
// key.
class CrossUserSharingKeys {
 public:
  static CrossUserSharingKeys CreateEmpty();
  // Deserialization from proto.
  static CrossUserSharingKeys CreateFromProto(
      const sync_pb::CrossUserSharingKeys& key_bag);

  CrossUserSharingKeys(CrossUserSharingKeys&& other);
  ~CrossUserSharingKeys();

  CrossUserSharingKeys& operator=(CrossUserSharingKeys&&) = default;

  // Serialization to proto.
  sync_pb::CrossUserSharingKeys ToProto() const;

  // Makes a deep copy of |*this|.
  CrossUserSharingKeys Clone() const;

  size_t size() const;
  bool HasKeyPair(const uint32_t key_version) const;

  // Sets a Public-private key-pair associated with `version`. Replaces any
  // pre-existing key pair for the given `version`.
  void SetKeyPair(CrossUserSharingPublicPrivateKeyPair key_pair,
                  uint32_t version);

  // Similar to AddKeyPair, but reads the private-key material from a proto and
  // derives the public-key from the private-key.
  bool AddKeyPairFromProto(const sync_pb::CrossUserSharingPrivateKey& key);

  // Returns the Public-private key-pair associated with `version`.
  const CrossUserSharingPublicPrivateKeyPair& GetKeyPair(
      uint32_t version) const;

 private:
  CrossUserSharingKeys();

  // Public-private key-pairs we know about, mapped by version.
  std::map<uint32_t, CrossUserSharingPublicPrivateKeyPair> key_pairs_map_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_CROSS_USER_SHARING_KEYS_H_
