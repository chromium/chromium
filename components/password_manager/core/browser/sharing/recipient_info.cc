// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/recipient_info.h"

#include "base/base64.h"

namespace password_manager {

bool PublicKey::operator==(const PublicKey& other) const = default;

sync_pb::CrossUserSharingPublicKey PublicKey::ToProto() const {
  sync_pb::CrossUserSharingPublicKey proto_result;
  // Bytes from x25519_public_key proto may not match valid utf-8 characters.
  // Conversion to Base64 prevents further errors while converting string to
  // different types.
  bool success =
      base::Base64Decode(key, proto_result.mutable_x25519_public_key());
  CHECK(success);
  proto_result.set_version(key_version);
  return proto_result;
}

// static
PublicKey PublicKey::FromProto(
    const sync_pb::CrossUserSharingPublicKey& proto_public_key) {
  PublicKey result;
  result.key = base::Base64Encode(proto_public_key.x25519_public_key());
  result.key_version = proto_public_key.version();
  return result;
}

RecipientInfo::RecipientInfo() = default;

RecipientInfo::RecipientInfo(const RecipientInfo& other) = default;
RecipientInfo::RecipientInfo(RecipientInfo&& other) = default;
RecipientInfo& RecipientInfo::operator=(const RecipientInfo&) = default;
RecipientInfo& RecipientInfo::operator=(RecipientInfo&&) = default;
RecipientInfo::~RecipientInfo() = default;

bool RecipientInfo::operator==(const RecipientInfo& other) const = default;

}  // namespace password_manager
