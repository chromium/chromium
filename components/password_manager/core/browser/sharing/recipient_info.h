// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENT_INFO_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENT_INFO_H_

#include <string>

#include "components/sync/protocol/nigori_specifics.pb.h"

namespace password_manager {

// Represents a Public Key for a user including its version.
struct PublicKey {
  // Base64 representation of 32 byte X25519 Public key.
  std::string key;
  // Key version.
  uint32_t key_version = 0;

  bool operator==(const PublicKey& other) const;

  sync_pb::CrossUserSharingPublicKey ToProto() const;

  static PublicKey FromProto(
      const sync_pb::CrossUserSharingPublicKey& proto_public_key);
};

// The RecipientInfo struct represents a recipient with whom the user can share
// a password.
struct RecipientInfo {
  RecipientInfo();
  RecipientInfo(const RecipientInfo&);
  RecipientInfo(RecipientInfo&&);
  RecipientInfo& operator=(const RecipientInfo&);
  RecipientInfo& operator=(RecipientInfo&&);
  ~RecipientInfo();

  bool operator==(const RecipientInfo& other) const;

  // Recipient's user identifier (obfuscated Gaia ID).
  std::string user_id;
  // Recipients's user name for display in the UI.
  std::string user_name;
  // The email address of the recipients account for display in the UI.
  std::string email;
  // URL to the profile picture of the recipient for display in the UI.
  std::string profile_image_url;
  // Recipient's Public key.
  PublicKey public_key;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENT_INFO_H_
