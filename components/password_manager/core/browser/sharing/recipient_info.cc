// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/recipient_info.h"

namespace password_manager {

bool PublicKey::operator==(const PublicKey& other) const {
  return key == other.key && key_version == other.key_version;
}

sync_pb::CrossUserSharingPublicKey PublicKey::ToProto() const {
  sync_pb::CrossUserSharingPublicKey proto_result;
  proto_result.set_x25519_public_key(key);
  proto_result.set_version(key_version);
  return proto_result;
}

// static
PublicKey PublicKey::FromProto(
    const sync_pb::CrossUserSharingPublicKey& proto_public_key) {
  PublicKey result;
  result.key = proto_public_key.x25519_public_key();
  result.key_version = proto_public_key.version();
  return result;
}

RecipientInfo::RecipientInfo() = default;

RecipientInfo::RecipientInfo(const RecipientInfo& other) = default;
RecipientInfo::RecipientInfo(RecipientInfo&& other) = default;
RecipientInfo& RecipientInfo::operator=(const RecipientInfo&) = default;
RecipientInfo& RecipientInfo::operator=(RecipientInfo&&) = default;
RecipientInfo::~RecipientInfo() = default;

bool RecipientInfo::operator==(const RecipientInfo& other) const {
  return user_id == other.user_id && user_name == other.user_name &&
         email == other.email && profile_image_url == other.profile_image_url &&
         public_key == other.public_key;
}

}  // namespace password_manager
