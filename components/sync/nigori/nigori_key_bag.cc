// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_key_bag.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {
namespace {

// Note that |key_name| is redundant but computing the name from |nigori| can be
// expensive.
sync_pb::NigoriKey NigoriToProto(const Nigori& nigori,
                                 const std::string& key_name) {
  DCHECK_EQ(key_name, nigori.GetKeyName());

  sync_pb::NigoriKey proto;
  proto.set_deprecated_name(key_name);
  nigori.ExportKeys(proto.mutable_deprecated_user_key(),
                    proto.mutable_encryption_key(), proto.mutable_mac_key());
  return proto;
}

sync_pb::CrossUserSharingPrivateKey KeyPairToPrivateKeyProto(
    const uint32_t version,
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  auto raw_private_key = key_pair.GetRawPrivateKey();
  sync_pb::CrossUserSharingPrivateKey output;
  output.set_version(version);
  output.set_x25519_private_key(
      std::string(std::begin(raw_private_key), std::end(raw_private_key)));
  return output;
}

std::unique_ptr<Nigori> CloneNigori(const Nigori& nigori) {
  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&user_key, &encryption_key, &mac_key);

  std::unique_ptr<Nigori> nigori_copy =
      Nigori::CreateByImport(user_key, encryption_key, mac_key);
  DCHECK(nigori_copy);
  return nigori_copy;
}

CrossUserSharingPublicPrivateKeyPair CloneKeyPair(
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  const auto raw_private_key = key_pair.GetRawPrivateKey();
  std::vector<uint8_t> key_for_import(raw_private_key.begin(),
                                      raw_private_key.end());
  absl::optional<CrossUserSharingPublicPrivateKeyPair> clone =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(key_for_import);
  CHECK(clone.has_value());
  return std::move(clone.value());
}

}  // namespace

// static
NigoriKeyBag NigoriKeyBag::CreateEmpty() {
  return NigoriKeyBag();
}

// static
NigoriKeyBag NigoriKeyBag::CreateFromProto(const sync_pb::NigoriKeyBag& proto) {
  NigoriKeyBag output;
  for (const sync_pb::NigoriKey& key : proto.key()) {
    if (output.AddKeyFromProto(key).empty()) {
      // TODO(crbug.com/1368018): Consider propagating this error to callers
      // such that they can do smarter handling.
      DLOG(ERROR) << "Invalid NigoriKey protocol buffer message.";
    }
  }
  for (const sync_pb::CrossUserSharingPrivateKey& key :
       proto.cross_user_sharing_private_key()) {
    if (!output.AddKeyPairFromProto(key)) {
      DLOG(WARNING) << "Could not add PrivateKey protocol buffer message.";
    }
  }

  return output;
}

NigoriKeyBag::NigoriKeyBag(NigoriKeyBag&& other) = default;

NigoriKeyBag::~NigoriKeyBag() = default;

void NigoriKeyBag::CopyFrom(const NigoriKeyBag& other) {
  nigori_map_.clear();
  AddAllUnknownKeysFrom(other);
}

sync_pb::NigoriKeyBag NigoriKeyBag::ToProto() const {
  sync_pb::NigoriKeyBag output;
  for (const auto& [key_name, nigori] : nigori_map_) {
    *output.add_key() = NigoriToProto(*nigori, key_name);
  }
  for (const auto& [key_version, key_pair] : key_pairs_map_) {
    *output.add_cross_user_sharing_private_key() =
        KeyPairToPrivateKeyProto(key_version, key_pair);
  }

  return output;
}

NigoriKeyBag NigoriKeyBag::Clone() const {
  NigoriKeyBag copy;
  copy.AddAllUnknownKeysFrom(*this);
  return copy;
}

size_t NigoriKeyBag::size() const {
  return nigori_map_.size();
}

bool NigoriKeyBag::HasKey(const std::string& key_name) const {
  return nigori_map_.count(key_name) != 0;
}

bool NigoriKeyBag::HasKeyPair(uint32_t key_pair_version) const {
  return key_pairs_map_.contains(key_pair_version);
}

sync_pb::NigoriKey NigoriKeyBag::ExportKey(const std::string& key_name) const {
  DCHECK(HasKey(key_name));
  sync_pb::NigoriKey key =
      NigoriToProto(*nigori_map_.find(key_name)->second, key_name);
  // For exported keys, clients never consumed the key name, so it's safe to
  // clear the deprecated field.
  key.clear_deprecated_name();
  return key;
}

std::string NigoriKeyBag::AddKey(std::unique_ptr<Nigori> nigori) {
  DCHECK(nigori);
  const std::string key_name = nigori->GetKeyName();
  if (key_name.empty()) {
    NOTREACHED();
    return key_name;
  }
  nigori_map_.emplace(key_name, std::move(nigori));
  return key_name;
}

std::string NigoriKeyBag::AddKeyFromProto(const sync_pb::NigoriKey& key) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByImport(
      key.deprecated_user_key(), key.encryption_key(), key.mac_key());
  if (!nigori) {
    return std::string();
  }

  const std::string key_name = nigori->GetKeyName();
  if (key_name.empty()) {
    return std::string();
  }

  nigori_map_[key_name] = std::move(nigori);
  return key_name;
}

void NigoriKeyBag::AddAllUnknownKeysFrom(const NigoriKeyBag& other) {
  for (const auto& [key_name, nigori] : other.nigori_map_) {
    // Only use this key if we don't already know about it.
    nigori_map_.emplace(key_name, CloneNigori(*nigori));
  }
  for (const auto& [public_key, key_pair] : other.key_pairs_map_) {
    key_pairs_map_.emplace(public_key, CloneKeyPair(key_pair));
  }
}

bool NigoriKeyBag::AddKeyPairFromProto(
    const sync_pb::CrossUserSharingPrivateKey& key) {
  std::vector<uint8_t> private_key(key.x25519_private_key().begin(),
                                   key.x25519_private_key().end());
  absl::optional<CrossUserSharingPublicPrivateKeyPair> key_pair =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(private_key);

  if (!key_pair.has_value()) {
    return false;
  }

  AddKeyPair(std::move(key_pair.value()), key.version());
  return true;
}

void NigoriKeyBag::AddKeyPair(CrossUserSharingPublicPrivateKeyPair key_pair,
                              uint32_t version) {
  key_pairs_map_.emplace(version, std::move(key_pair));
}

bool NigoriKeyBag::EncryptWithKey(
    const std::string& key_name,
    const std::string& input,
    sync_pb::EncryptedData* encrypted_output) const {
  DCHECK(encrypted_output);
  DCHECK(HasKey(key_name));

  encrypted_output->set_blob(
      nigori_map_.find(key_name)->second->Encrypt(input));
  encrypted_output->set_key_name(key_name);

  // TODO(crbug.com/1368018): returned value is always true, update interface
  // to return void or `encrypted_output`.
  return true;
}

bool NigoriKeyBag::CanDecrypt(
    const sync_pb::EncryptedData& encrypted_input) const {
  return HasKey(encrypted_input.key_name());
}

bool NigoriKeyBag::Decrypt(const sync_pb::EncryptedData& encrypted_input,
                           std::string* decrypted_output) const {
  DCHECK(decrypted_output);

  decrypted_output->clear();

  auto it = nigori_map_.find(encrypted_input.key_name());
  if (it == nigori_map_.end()) {
    // The key used to encrypt the blob is not part of the set of installed
    // nigoris.
    return false;
  }

  return it->second->Decrypt(encrypted_input.blob(), decrypted_output);
}

NigoriKeyBag::NigoriKeyBag() = default;

}  // namespace syncer
