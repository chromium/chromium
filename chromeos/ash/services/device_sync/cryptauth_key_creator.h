// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// The lone method CreateKeys() takes a map from key-bundle name to key-creation
// data and returns a map from key-bundle name to newly created key via the
// callback, |create_keys_callback|.
//
// Requirements:
//   - An instance of this class should only be used once.
//   - The input map, |keys_to_create|, cannot be empty.
//   - Currently, the only supported key types are RAW128 and RAW256 for
//     symmetric keys and P256 for asymmetric keys.
//   - If symmetric keys are being created, the CryptAuth server's
//     Diffie-Hellman public key needs to be passed into |server_ephemeral_dh|
//     of CreateKeys().
//
// Note about symmetric key creation:
//   The CryptAuth v2 Enrollment protocol demands that symmetric keys be derived
//   from a secret key, obtained by performing a Diffie-Hellman handshake with
//   the CryptAuth server. Specifically,
//
//   |derived_key| =
//       Hkdf(|secret|, salt="CryptAuth Enrollment", info=|key_bundle_name|).
//
//   The CryptAuth server's Diffie-Hellman key is passed into CreateKeys(),
//   CreateKeys() generates the client side of the Diffie-Hellman handshake, and
//   this asymmetric key is returned in the callback.
class CryptAuthKeyCreator {
 public:
  struct CreateKeyData {
    CreateKeyData(CryptAuthKey::Status status,
                  cryptauthv2::KeyType type,
                  std::optional<std::string> handle = std::nullopt);

    // Special constructor needed to handle existing user key pair. The input
    // strings cannot be empty.
    CreateKeyData(CryptAuthKey::Status status,
                  cryptauthv2::KeyType type,
                  const std::string& handle,
                  const std::string& public_key,
                  const std::string& private_key);

    ~CreateKeyData();
    CreateKeyData(const CreateKeyData&);

    CryptAuthKey::Status status;
    cryptauthv2::KeyType type;
    std::optional<std::string> handle;
    // Special data needed to handle existing user key pair. If these are both
    // non-empty strings and the key type is asymmetric, then the key creator
    // will bypass the standard key creation and simply return
    // CryptAuthKey(|public_key|, |private_key|, |status|, |type|, |handle|).
    std::optional<std::string> public_key;
    std::optional<std::string> private_key;
  };

  CryptAuthKeyCreator();

  CryptAuthKeyCreator(const CryptAuthKeyCreator&) = delete;
  CryptAuthKeyCreator& operator=(const CryptAuthKeyCreator&) = delete;

  virtual ~CryptAuthKeyCreator();

  // A new key is null if key creation fails.
  using CreateKeysCallback = base::OnceCallback<void(
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& /* new_keys */,
      const std::optional<CryptAuthKey>& /* client_ephemeral_dh */)>;
  virtual void CreateKeys(
      const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
          keys_to_create,
      const std::optional<CryptAuthKey>& server_ephemeral_dh,
      CreateKeysCallback create_keys_callback) = 0;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_CREATOR_H_
