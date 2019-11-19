// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_service_data_helper_impl.h"

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/beacon_seed.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_cache.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/secure_channel/background_eid_generator.h"
#include "chromeos/services/secure_channel/ble_advertisement_generator.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/foreground_eid_generator.h"

namespace chromeos {

namespace secure_channel {

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
BleServiceDataHelperImpl::Factory*
    BleServiceDataHelperImpl::Factory::test_factory_ = nullptr;

// static
BleServiceDataHelperImpl::Factory* BleServiceDataHelperImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleServiceDataHelperImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleServiceDataHelperImpl::Factory::~Factory() = default;

std::unique_ptr<BleServiceDataHelper>
BleServiceDataHelperImpl::Factory::BuildInstance(
    multidevice::RemoteDeviceCache* remote_device_cache) {
  return base::WrapUnique(new BleServiceDataHelperImpl(remote_device_cache));
}

BleServiceDataHelperImpl::BleServiceDataHelperImpl(
    multidevice::RemoteDeviceCache* remote_device_cache)
    : remote_device_cache_(remote_device_cache),
      background_eid_generator_(std::make_unique<BackgroundEidGenerator>()),
      foreground_eid_generator_(std::make_unique<ForegroundEidGenerator>()) {}

BleServiceDataHelperImpl::~BleServiceDataHelperImpl() = default;

std::unique_ptr<DataWithTimestamp>
BleServiceDataHelperImpl::GenerateForegroundAdvertisement(
    const DeviceIdPair& device_id_pair) {
  base::Optional<multidevice::RemoteDeviceRef> local_device =
      remote_device_cache_->GetRemoteDevice(device_id_pair.local_device_id());
  if (!local_device) {
    PA_LOG(ERROR) << "Requested local device does not exist: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pair.local_device_id());
    return nullptr;
  }

  base::Optional<multidevice::RemoteDeviceRef> remote_device =
      remote_device_cache_->GetRemoteDevice(device_id_pair.remote_device_id());
  if (!remote_device) {
    PA_LOG(ERROR) << "Requested remote device does not exist: "
                  << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         device_id_pair.remote_device_id());
    return nullptr;
  }

  return BleAdvertisementGenerator::GenerateBleAdvertisement(
      *remote_device, local_device->public_key());
}

base::Optional<BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  base::flat_map<std::string, std::vector<std::string>>
      local_device_id_to_remote_device_ids_map;
  for (const auto& device_id_pair : device_id_pair_set) {
    if (!remote_device_cache_->GetRemoteDevice(
            device_id_pair.local_device_id())) {
      PA_LOG(ERROR) << "Requested local device does not exist"
                    << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id_pair.local_device_id());
      continue;
    }

    if (!remote_device_cache_->GetRemoteDevice(
            device_id_pair.remote_device_id())) {
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

  return base::nullopt;
}

base::Optional<BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const std::string& local_device_id,
    const std::vector<std::string>& remote_device_ids) {
  std::string identified_device_id;
  bool is_background_advertisement = false;

  // First try, identifying |service_data| as a foreground advertisement.
  if (service_data.size() >= kMinNumBytesInForegroundServiceData) {
    std::vector<cryptauth::BeaconSeed> beacon_seeds =
        multidevice::ToCryptAuthSeedList(
            remote_device_cache_->GetRemoteDevice(local_device_id)
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
    std::transform(remote_device_ids.begin(), remote_device_ids.end(),
                   std::back_inserter(remote_devices), [this](auto device_id) {
                     return *remote_device_cache_->GetRemoteDevice(device_id);
                   });

    identified_device_id =
        background_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
            service_data, remote_devices);
    is_background_advertisement = true;
  }

  // If the service data does not correspond to an advertisement from a device
  // on this account, ignore it.
  if (identified_device_id.empty())
    return base::nullopt;

  return BleServiceDataHelper::DeviceWithBackgroundBool(
      *remote_device_cache_->GetRemoteDevice(identified_device_id),
      is_background_advertisement);
}

void BleServiceDataHelperImpl::SetTestDoubles(
    std::unique_ptr<BackgroundEidGenerator> background_eid_generator,
    std::unique_ptr<ForegroundEidGenerator> foreground_eid_generator) {
  background_eid_generator_ = std::move(background_eid_generator);
  foreground_eid_generator_ = std::move(foreground_eid_generator);
}

}  // namespace secure_channel

}  // namespace chromeos
