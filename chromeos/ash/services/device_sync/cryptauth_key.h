// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_H_

#include <optional>

#include "base/values.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// Holds information for a key managed by CryptAuth or an ephemeral key, such as
// a Diffie-Hellman key-pair.
class CryptAuthKey {
 public:
  // Specified by CryptAuth in the Enrollment protocol by
  // SyncKeysResponse::SyncSingleKeyResponse::KeyAction or
  // SyncKeysResponse::SyncSingleKeyResponse::KeyCreation. kActive denotes that
  // the key should be used. kInactive denotes that the key should be retained
  // for possible future activation but not used. For ephemeral keys not managed
  // by CryptAuth but used locally for intermediate cryptographic operations,
  // this status is meaningless.
  enum Status { kActive, kInactive };

  static std::optional<CryptAuthKey> FromDictionary(
      const base::Value::Dict& value);

  // Constructor for symmetric keys.
  CryptAuthKey(const std::string& symmetric_key,
               Status status,
               cryptauthv2::KeyType type,
               const std::optional<std::string>& handle = std::nullopt);

  // Constructor for asymmetric keys. Note that |public_key| should be a
  // serialized securemessage::GenericPublicKey proto.
  CryptAuthKey(const std::string& public_key,
               const std::string& private_key,
               Status status,
               cryptauthv2::KeyType type,
               const std::optional<std::string>& handle = std::nullopt);

  CryptAuthKey(const CryptAuthKey&);

  ~CryptAuthKey();

  bool IsSymmetricKey() const;
  bool IsAsymmetricKey() const;

  Status status() const { return status_; }
  cryptauthv2::KeyType type() const { return type_; }
  const std::string& symmetric_key() const {
    DCHECK(IsSymmetricKey());
    return symmetric_key_;
  }
  const std::string& public_key() const {
    DCHECK(IsAsymmetricKey());
    return public_key_;
  }
  const std::string& private_key() const {
    DCHECK(IsAsymmetricKey());
    return private_key_;
  }
  const std::string& handle() const { return handle_; }

  void set_status(Status status) { status_ = status; }

  // Converts CryptAuthKey to a Value::Dict of the form
  //   {
  //     "handle": <handle_>
  //     "status": <status_ as int>
  //     "symmetric_key": <symmetric_key_>
  //     "type": <type_ as int>
  //   }
  base::Value::Dict AsSymmetricKeyDictionary() const;

  // Converts CryptAuthKey to a Value::Dict of the form
  //   {
  //     "handle": <handle_>
  //     "private_key" : <private_key_>
  //     "public_key" : <public_key_>
  //     "status": <status_ as int>
  //     "type": <type_ as int>
  //   }
  base::Value::Dict AsAsymmetricKeyDictionary() const;

  bool operator==(const CryptAuthKey& other) const;
  bool operator!=(const CryptAuthKey& other) const;

 private:
  std::string symmetric_key_;
  std::string public_key_;
  std::string private_key_;
  Status status_;
  cryptauthv2::KeyType type_;
  std::string handle_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_H_
