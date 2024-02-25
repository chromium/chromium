// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

namespace ash::device_sync {

// A group of related CryptAuthKeys, uniquely identified by their handles.
//
// No more than one key in the bundle can be active at a time, and only the
// active key should be used for encryption, signing, etc. Inactive keys are
// retained and can be activated in the future, for example, due to a
// SyncSingleKeyResponse::KeyAction sent by CryptAuth.
//
// All key bundles used in Chrome OS are enumerated in the Name enum class. The
// name string corresponding to each enum value can be retrieved via
// KeyBundleNameEnumToString(). For key bundles that enroll with CryptAuth, this
// string is used to populate the SyncSingleKeysRequest::key_name protobuf
// field.
class CryptAuthKeyBundle {
 public:
  // Names that uniquely define a CryptAuthKeyBundle.
  enum class Name {
    // A non-rotated asymmetric key associated with a user on the device. It is
    // used for encrypting device-to-device communications, for example, and it
    // has historically been used as a device identifier.
    kUserKeyPair,
    // Currently unused but required for CryptAuth v2 Enrollment.
    kLegacyAuthzenKey,
    // Enrolling this asymmetric key adds the device to the user's DeviceSync v2
    // "DeviceSync:BetterTogether" group. This key is not to be confused with
    // the unenrolled kDeviceSyncBetterTogetherGroupKey.
    kDeviceSyncBetterTogether,
    // A key pair that does *not* enroll with CryptAuth, used to encrypt and
    // decrypt the metadata of all devices in the user's
    // "DeviceSync:BetterTogether" group. This metadata is passed in an
    // end-to-end encrypted fashion via DeviceSync v2 SyncMetadata calls .
    kDeviceSyncBetterTogetherGroupKey
  };

  // Returns all Name enum values as a set.
  static const base::flat_set<CryptAuthKeyBundle::Name>& AllNames();

  // Returns the Name enum value of all key bundles that enroll with CryptAuth.
  static const base::flat_set<CryptAuthKeyBundle::Name>& AllEnrollableNames();

  static std::string KeyBundleNameEnumToString(CryptAuthKeyBundle::Name name);
  static std::optional<CryptAuthKeyBundle::Name> KeyBundleNameStringToEnum(
      const std::string& name);

  static std::optional<CryptAuthKeyBundle> FromDictionary(
      const base::Value::Dict& dict);

  explicit CryptAuthKeyBundle(Name name);

  CryptAuthKeyBundle(const CryptAuthKeyBundle&);

  ~CryptAuthKeyBundle();

  Name name() const { return name_; }

  const base::flat_map<std::string, CryptAuthKey>& handle_to_key_map() const {
    return handle_to_key_map_;
  }

  const std::optional<cryptauthv2::KeyDirective>& key_directive() const {
    return key_directive_;
  }

  void set_key_directive(const cryptauthv2::KeyDirective& key_directive) {
    key_directive_ = key_directive;
  }

  // Returns nullptr if there is no active key.
  const CryptAuthKey* GetActiveKey() const;

  // If the key being added is active, all other keys in the bundle will be
  // deactivated. If the handle of the input key matches one in the bundle, the
  // existing key will be overwritten.
  // Note: All keys added to the bundle kUserKeyPair must have the handle
  // kCryptAuthFixedUserKeyPairHandle.
  void AddKey(const CryptAuthKey& key);

  // Activates the key corresponding to |handle| in the bundle and deactivates
  // the other keys.
  void SetActiveKey(const std::string& handle);

  // Sets all key statuses to kInactive.
  void DeactivateKeys();

  // Remove the key corresponding to |handle| from the bundle.
  void DeleteKey(const std::string& handle);

  base::Value::Dict AsDictionary() const;

  bool operator==(const CryptAuthKeyBundle& other) const;
  bool operator!=(const CryptAuthKeyBundle& other) const;

 private:
  using HandleToKeyMap = base::flat_map<std::string, CryptAuthKey>;
  Name name_;
  HandleToKeyMap handle_to_key_map_;
  std::optional<cryptauthv2::KeyDirective> key_directive_;
};

}  // namespace ash::device_sync

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_BUNDLE_H_
