// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/proto/cryptauth_enrollment.pb.h"

namespace chromeos {

namespace device_sync {

// Stores key bundles used in CryptAuth v2 protocols.
//
// Note: Not all key bundles in the registry are enrolled with CryptAuth, only
// those bundles contained in CryptAuthKeyBundle::AllEnrollableNames().
class CryptAuthKeyRegistry {
 public:
  using KeyBundleMap =
      base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyBundle>;

  virtual ~CryptAuthKeyRegistry();

  // Returns the underlying map from the key-bundle name to the key bundle.
  virtual const KeyBundleMap& key_bundles() const;

  // Returns the key bundle with name |name| if it exists in the key registry,
  // and returns null if it cannot be found.
  virtual const CryptAuthKeyBundle* GetKeyBundle(
      CryptAuthKeyBundle::Name name) const;

  // Returns the key with status kActive if one exists in the key bundle with
  // name |name|, and returns null if one cannot be found.
  virtual const CryptAuthKey* GetActiveKey(CryptAuthKeyBundle::Name name) const;

  // Adds |key| to the key bundle with |name|. If the key being added is active,
  // all other keys in the bundle will be deactivated. If the handle of the
  // input key matches one in the bundle, the existing key will be overwritten.
  // Note: All keys added to the bundle kUserKeyPair must have the handle
  // kCryptAuthFixedUserKeyPairHandle.
  virtual void AddKey(CryptAuthKeyBundle::Name name, const CryptAuthKey& key);

  // Activates the key corresponding to |handle| in the key bundle with |name|
  // and deactivates the other keys the bundle.
  virtual void SetActiveKey(CryptAuthKeyBundle::Name name,
                            const std::string& handle);

  // Sets all key statuses to kInactive in the key bundle with |name|.
  virtual void DeactivateKeys(CryptAuthKeyBundle::Name name);

  // Remove the key corresponding to |handle| from the key bundle with |name|.
  virtual void DeleteKey(CryptAuthKeyBundle::Name name,
                         const std::string& handle);

  // Set the key directive for the key bundle with |name|.
  virtual void SetKeyDirective(CryptAuthKeyBundle::Name name,
                               const cryptauthv2::KeyDirective& key_directive);

 protected:
  CryptAuthKeyRegistry();

  // Invoked when the key bundle map changes.
  virtual void OnKeyRegistryUpdated() = 0;

  KeyBundleMap key_bundles_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyRegistry);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_H_
