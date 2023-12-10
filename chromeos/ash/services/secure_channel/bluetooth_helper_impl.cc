// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/bluetooth_helper_impl.h"

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/background_eid_generator.h"
#include "chromeos/ash/services/secure_channel/ble_advertisement_generator.h"
#include "chromeos/ash/services/secure_channel/foreground_eid_generator.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"

namespace ash::secure_channel {

namespace {

// Valid advertisement service data must be at least 2 bytes.
// As of June 2018, valid background advertisement service data is exactly 2
// bytes, which identify the advertising device to the scanning device.
// Valid foreground advertisement service data must include at least 4 bytes:
// 2 bytes associated with the scanning device (used as a scan filter) and 2
// bytes which identify the advertising device to the scanning device.
const size_t kMinNumBytesInServiceData = 2;
const size_t kMaxNumBytesInBackgroundServiceData = 3;
const size_t kMinNumBytesInForegroundServiceData = 4;

}  // namespace

// static
BluetoothHelperImpl::Factory* BluetoothHelperImpl::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<BluetoothHelper> BluetoothHelperImpl::Factory::Create(
    multidevice::RemoteDeviceCache* remote_device_cache) {
  if (test_factory_)
    return test_factory_->CreateInstance(remote_device_cache);

  return base::WrapUnique(new BluetoothHelperImpl(remote_device_cache));
}

// static
void BluetoothHelperImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

BluetoothHelperImpl::Factory::~Factory() = default;

BluetoothHelperImpl::BluetoothHelperImpl(
    multidevice::RemoteDeviceCache* remote_device_cache)
    : remote_device_cache_(remote_device_cache),
      background_eid_generator_(std::make_unique<BackgroundEidGenerator>()),
      foreground_eid_generator_(std::make_unique<ForegroundEidGenerator>()) {}

BluetoothHelperImpl::~BluetoothHelperImpl() = default;

std::unique_ptr<DataWithTimestamp>
BluetoothHelperImpl::GenerateForegroundAdvertisement(
    const DeviceIdPair& device_id_pair) {
  std::optional<multidevice::RemoteDeviceRef> local_device =
      remote_device_cache_->GetRemoteDevice(
          std::nullopt /* instance_id */,
          device_id_pair.local_device_id() /* legacy_device_id */);
  if (!local_device) {
    PA_LOG(ERROR) << "Requested local device does not exist: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pair.local_device_id());
    return nullptr;
  }

  std::optional<multidevice::RemoteDeviceRef> remote_device =
      remote_device_cache_->GetRemoteDevice(
          std::nullopt /* instance_id */,
          device_id_pair.remote_device_id() /* legacy_device_id */);
  if (!remote_device) {
    PA_LOG(ERROR) << "Requested remote device does not exist: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pair.remote_device_id());
    return nullptr;
  }

  return BleAdvertisementGenerator::GenerateBleAdvertisement(
      *remote_device, local_device->public_key());
}

std::optional<BluetoothHelper::DeviceWithBackgroundBool>
BluetoothHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  base::flat_map<std::string, std::vector<std::string>>
      local_device_id_to_remote_device_ids_map;
  for (const auto& device_id_pair : device_id_pair_set) {
    if (!remote_device_cache_->GetRemoteDevice(
            std::nullopt /* instance_id */,
            device_id_pair.local_device_id() /* legacy_device_id */)) {
      PA_LOG(ERROR) << "Requested local device does not exist"
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id_pair.local_device_id());
      continue;
    }

    if (!remote_device_cache_->GetRemoteDevice(
            std::nullopt /* instance_id */,
            device_id_pair.remote_device_id() /* legacy_device_id */)) {
      PA_LOG(ERROR) << "Requested remote device does not exist"
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id_pair.remote_device_id());
      continue;
    }

    local_device_id_to_remote_device_ids_map[device_id_pair.local_device_id()]
        .push_back(device_id_pair.remote_device_id());
  }

  for (const auto& map_entry : local_device_id_to_remote_device_ids_map) {
    auto device_with_background_bool = PerformIdentifyRemoteDevice(
        service_data, map_entry.first, map_entry.second);
    if (device_with_background_bool)
      return device_with_background_bool;
  }

  return std::nullopt;
}

std::string BluetoothHelperImpl::GetBluetoothPublicAddress(
    const std::string& device_id) {
  std::optional<multidevice::RemoteDeviceRef> device =
      remote_device_cache_->GetRemoteDevice(std::nullopt /* instance_id */,
                                            device_id /* legacy_device_id */);
  if (device)
    return device->bluetooth_public_address();
  return std::string();
}

std::string BluetoothHelperImpl::ExpectedServiceDataToString(
    const DeviceIdPairSet& device_id_pair_set) {
  std::stringstream ss;

  for (const DeviceIdPair& pair : device_id_pair_set) {
    ss << "{Device ID: "
       << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
              pair.remote_device_id())
       << " - ";

    std::optional<multidevice::RemoteDeviceRef> device =
        remote_device_cache_->GetRemoteDevice(
            std::nullopt /* instance_id */,
            pair.remote_device_id() /* legacy_device_id */);

    if (!device) {
      ss << "<missing metadata>},";
      continue;
    }

    std::vector<DataWithTimestamp> data_for_device =
        background_eid_generator_->GenerateNearestEids(
            multidevice::ToCryptAuthSeedList(device->beacon_seeds()));
    if (data_for_device.empty()) {
      ss << "[]},";
      continue;
    }

    ss << DataWithTimestamp::ToDebugString(data_for_device) << "},";
  }

  return ss.str();
}

std::optional<BluetoothHelper::DeviceWithBackgroundBool>
BluetoothHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const std::string& local_device_id,
    const std::vector<std::string>& remote_device_ids) {
  std::string identified_device_id;
  bool is_background_advertisement = false;

  // First try, identifying |service_data| as a foreground advertisement.
  if (service_data.size() >= kMinNumBytesInForegroundServiceData) {
    std::vector<cryptauth::BeaconSeed> beacon_seeds =
        multidevice::ToCryptAuthSeedList(
            remote_device_cache_
                ->GetRemoteDevice(std::nullopt /* instance_id */,
                                  local_device_id /* legacy_device_id */)
                ->beacon_seeds());

    identified_device_id =
        foreground_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
            service_data, remote_device_ids, beacon_seeds);
  }

  // If the device has not yet been identified, try identifying |service_data|
  // as a background advertisement.
  if (features::IsInstantTetheringBackgroundAdvertisingSupported() &&
      identified_device_id.empty() &&
      service_data.size() >= kMinNumBytesInServiceData &&
      service_data.size() <= kMaxNumBytesInBackgroundServiceData) {
    multidevice::RemoteDeviceRefList remote_devices;
    base::ranges::transform(
        remote_device_ids, std::back_inserter(remote_devices),
        [this](auto device_id) {
          return *remote_device_cache_->GetRemoteDevice(
              std::nullopt /* instance_id */, device_id /* legacy_device_id */);
        });

    identified_device_id =
        background_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
            service_data, remote_devices);
    is_background_advertisement = true;
  }

  // If the service data does not correspond to an advertisement from a device
  // on this account, ignore it.
  if (identified_device_id.empty())
    return std::nullopt;

  return BluetoothHelper::DeviceWithBackgroundBool(
      *remote_device_cache_->GetRemoteDevice(
          std::nullopt /* instance_id */,
          identified_device_id /* legacy_device_id */),
      is_background_advertisement);
}

void BluetoothHelperImpl::SetTestDoubles(
    std::unique_ptr<BackgroundEidGenerator> background_eid_generator,
    std::unique_ptr<ForegroundEidGenerator> foreground_eid_generator) {
  background_eid_generator_ = std::move(background_eid_generator);
  foreground_eid_generator_ = std::move(foreground_eid_generator);
}

}  // namespace ash::secure_channel
