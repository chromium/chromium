// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"

namespace chromeos {

namespace device_sync {

// Holds information for a device managed by CryptAuth.
class CryptAuthDevice {
 public:
  // Returns null if |dict| cannot be converted into a CryptAuthDevice.
  static base::Optional<CryptAuthDevice> FromDictionary(
      const base::Value& dict);

  // |instance_id|: The Instance ID, used as a unique device identifier. Cannot
  //                be empty.
  explicit CryptAuthDevice(const std::string& instance_id);
  CryptAuthDevice(
      const std::string& instance_id,
      const std::string& device_name,
      const std::string& device_better_together_public_key,
      const base::Time& last_update_time,
      const base::Optional<cryptauthv2::BetterTogetherDeviceMetadata>&
          better_together_device_metadata,
      const std::map<multidevice::SoftwareFeature,
                     multidevice::SoftwareFeatureState>& feature_states);

  CryptAuthDevice(const CryptAuthDevice&);

  ~CryptAuthDevice();

  const std::string& instance_id() const { return instance_id_; }

  // Converts CryptAuthDevice to a dictionary-type Value of the form
  //   {
  //     "instance_id": |instance_id_|,
  //     "device_name": |device_name_|,
  //     "device_better_together_public_key_":
  //         <|device_better_together_public_key_| base64 encoded>,
  //     "last_update_time": <|last_update_time_| converted to string>,
  //     "better_together_device_metadata":
  //         <|better_together_device_metadata_| serialized and base64 encoded>,
  //     "feature_states": <|feature_states_| as dictionary>
  //   }
  base::Value AsDictionary() const;

  bool operator==(const CryptAuthDevice& other) const;
  bool operator!=(const CryptAuthDevice& other) const;

  // The name known to the CryptAuth server or which was assigned by the user to
  // the device. Might contain personally identifiable information.
  std::string device_name;

  // The device's "DeviceSync:BetterTogether" public key that is enrolled with
  // CryptAuth--known as CryptAuthKeyBundle::kDeviceSyncBetterTogether on Chrome
  // OS. This is not to be confused with the shared, unenrolled group public
  // key.
  std::string device_better_together_public_key;

  // The last time the device updated its information with the CryptAuth server.
  base::Time last_update_time;

  // Device metadata relevant to the suite of multi-device ("Better Together")
  // features. Null if metadata could not be decrypted.
  base::Optional<cryptauthv2::BetterTogetherDeviceMetadata>
      better_together_device_metadata;

  // A map from the multi-device feature type (example: kBetterTogetherHost) to
  // feature state (example: kEnabled).
  std::map<multidevice::SoftwareFeature, multidevice::SoftwareFeatureState>
      feature_states;

 private:
  std::string instance_id_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_H_
