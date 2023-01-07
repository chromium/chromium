// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_MOJOM_MULTIDEVICE_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_MOJOM_MULTIDEVICE_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/mojom/multidevice_types.mojom-shared.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/multidevice/software_feature_state.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<ash::multidevice::mojom::BeaconSeedDataView,
                   ash::multidevice::BeaconSeed> {
 public:
  static const std::string& data(
      const ash::multidevice::BeaconSeed& beacon_seed);
  static base::Time start_time(const ash::multidevice::BeaconSeed& beacon_seed);
  static base::Time end_time(const ash::multidevice::BeaconSeed& beacon_seed);

  static bool Read(ash::multidevice::mojom::BeaconSeedDataView in,
                   ash::multidevice::BeaconSeed* out);
};

template <>
class StructTraits<ash::multidevice::mojom::RemoteDeviceDataView,
                   ash::multidevice::RemoteDevice> {
 public:
  static std::string device_id(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& user_email(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& instance_id(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& device_name(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& pii_free_device_name(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& persistent_symmetric_key(
      const ash::multidevice::RemoteDevice& remote_device);
  static base::Time last_update_time(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::map<ash::multidevice::SoftwareFeature,
                        ash::multidevice::SoftwareFeatureState>&
  software_features(const ash::multidevice::RemoteDevice& remote_device);
  static const std::vector<ash::multidevice::BeaconSeed>& beacon_seeds(
      const ash::multidevice::RemoteDevice& remote_device);
  static const std::string& bluetooth_public_address(
      const ash::multidevice::RemoteDevice& remote_device);

  static bool Read(ash::multidevice::mojom::RemoteDeviceDataView in,
                   ash::multidevice::RemoteDevice* out);
};

template <>
class EnumTraits<ash::multidevice::mojom::SoftwareFeature,
                 ash::multidevice::SoftwareFeature> {
 public:
  static ash::multidevice::mojom::SoftwareFeature ToMojom(
      ash::multidevice::SoftwareFeature input);
  static bool FromMojom(ash::multidevice::mojom::SoftwareFeature input,
                        ash::multidevice::SoftwareFeature* out);
};

template <>
class EnumTraits<ash::multidevice::mojom::SoftwareFeatureState,
                 ash::multidevice::SoftwareFeatureState> {
 public:
  static ash::multidevice::mojom::SoftwareFeatureState ToMojom(
      ash::multidevice::SoftwareFeatureState input);
  static bool FromMojom(ash::multidevice::mojom::SoftwareFeatureState input,
                        ash::multidevice::SoftwareFeatureState* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_MOJOM_MULTIDEVICE_MOJOM_TRAITS_H_
