// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cross_user_sharing_keys.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {
namespace {

sync_pb::CrossUserSharingPrivateKey KeyPairToPrivateKeyProto(
    const uint32_t version,
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  const auto raw_private_key = key_pair.GetRawPrivateKey();
  sync_pb::CrossUserSharingPrivateKey output;
  output.set_version(version);
  output.set_x25519_private_key(
      std::string(std::begin(raw_private_key), std::end(raw_private_key)));
  return output;
}

CrossUserSharingPublicPrivateKeyPair CloneKeyPair(
    const CrossUserSharingPublicPrivateKeyPair& key_pair) {
  return CrossUserSharingPublicPrivateKeyPair(key_pair.GetRawPrivateKey());
}

}  // namespace

// static
CrossUserSharingKeys CrossUserSharingKeys::CreateEmpty() {
  return CrossUserSharingKeys();
}

// static
CrossUserSharingKeys CrossUserSharingKeys::CreateFromProto(
    const sync_pb::CrossUserSharingKeys& proto) {
  CrossUserSharingKeys output;
  for (const sync_pb::CrossUserSharingPrivateKey& key : proto.private_key()) {
    if (!output.AddKeyPairFromProto(key)) {
      // TODO(crbug.com/40267990): consider re-downloading Nigori node in this
      // case.
      LOG(ERROR) << "Could not add PrivateKey protocol buffer message.";
    }
  }

  return output;
}

CrossUserSharingKeys::CrossUserSharingKeys() = default;

CrossUserSharingKeys::CrossUserSharingKeys(CrossUserSharingKeys&& other) =
    default;

CrossUserSharingKeys::~CrossUserSharingKeys() = default;

sync_pb::CrossUserSharingKeys CrossUserSharingKeys::ToProto() const {
  sync_pb::CrossUserSharingKeys output;
  for (const auto& [key_version, key_pair] : key_pairs_map_) {
    *output.add_private_key() = KeyPairToPrivateKeyProto(key_version, key_pair);
  }

  return output;
}

CrossUserSharingKeys CrossUserSharingKeys::Clone() const {
  CrossUserSharingKeys copy;
  for (const auto& [version, key_pair] : key_pairs_map_) {
    copy.SetKeyPair(CloneKeyPair(key_pair), version);
  }
  return copy;
}

size_t CrossUserSharingKeys::size() const {
  return key_pairs_map_.size();
}

bool CrossUserSharingKeys::HasKeyPair(uint32_t key_pair_version) const {
  return key_pairs_map_.contains(key_pair_version);
}

bool CrossUserSharingKeys::AddKeyPairFromProto(
    const sync_pb::CrossUserSharingPrivateKey& key) {
  std::optional<base::span<const uint8_t, X25519_PRIVATE_KEY_LEN>> fixed_key =
      base::as_byte_span(key.x25519_private_key())
          .to_fixed_extent<X25519_PRIVATE_KEY_LEN>();
  if (!fixed_key) {
    return false;
  }
  SetKeyPair(CrossUserSharingPublicPrivateKeyPair(*fixed_key), key.version());
  return true;
}

void CrossUserSharingKeys::SetKeyPair(
    CrossUserSharingPublicPrivateKeyPair key_pair,
    uint32_t version) {
  key_pairs_map_.insert_or_assign(version, std::move(key_pair));
}

const CrossUserSharingPublicPrivateKeyPair& CrossUserSharingKeys::GetKeyPair(
    uint32_t version) const {
  CHECK(HasKeyPair(version));
  return key_pairs_map_.at(version);
}

}  // namespace syncer
