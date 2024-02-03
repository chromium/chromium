// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/background_eid_generator.h"

#include <cstring>
#include <memory>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"
#include "chromeos/ash/services/secure_channel/raw_eid_generator.h"
#include "chromeos/ash/services/secure_channel/raw_eid_generator_impl.h"

namespace ash::secure_channel {

namespace {

// The duration of a EID period in milliseconds.
const int64_t kEidPeriodMs = 15 * 60 * 1000;  // 15 minutes

// The number of periods to look forward and backwards when calculating the
// neartest EIDs.
const int kEidLookAhead = 2;

// Returns the BeaconSeed valid for |timestamp_ms|, or nullptr if none can be
// found.
const cryptauth::BeaconSeed* GetBeaconSeedForTimestamp(
    int64_t timestamp_ms,
    const std::vector<cryptauth::BeaconSeed>& beacon_seeds) {
  for (const cryptauth::BeaconSeed& seed : beacon_seeds) {
    if (timestamp_ms >= seed.start_time_millis() &&
        timestamp_ms <= seed.end_time_millis()) {
      return &seed;
    }
  }
  return nullptr;
}

}  // namespace

BackgroundEidGenerator::BackgroundEidGenerator()
    : BackgroundEidGenerator(std::make_unique<RawEidGeneratorImpl>(),
                             base::DefaultClock::GetInstance()) {}

BackgroundEidGenerator::~BackgroundEidGenerator() {}

BackgroundEidGenerator::BackgroundEidGenerator(
    std::unique_ptr<RawEidGenerator> raw_eid_generator,
    base::Clock* clock)
    : raw_eid_generator_(std::move(raw_eid_generator)), clock_(clock) {}

std::vector<DataWithTimestamp> BackgroundEidGenerator::GenerateNearestEids(
    const std::vector<cryptauth::BeaconSeed>& beacon_seeds) const {
  int64_t now_timestamp_ms = clock_->Now().InMillisecondsSinceUnixEpoch();
  std::vector<DataWithTimestamp> eids;

  for (int i = -kEidLookAhead; i <= kEidLookAhead; ++i) {
    int64_t timestamp_ms = now_timestamp_ms + i * kEidPeriodMs;
    std::unique_ptr<DataWithTimestamp> eid =
        GenerateEid(timestamp_ms, beacon_seeds);
    if (eid)
      eids.push_back(*eid);
  }

  return eids;
}

std::unique_ptr<DataWithTimestamp> BackgroundEidGenerator::GenerateEid(
    int64_t timestamp_ms,
    const std::vector<cryptauth::BeaconSeed>& beacon_seeds) const {
  const cryptauth::BeaconSeed* beacon_seed =
      GetBeaconSeedForTimestamp(timestamp_ms, beacon_seeds);
  if (!beacon_seed) {
    PA_LOG(WARNING) << "  " << timestamp_ms << ": outside of BeaconSeed range.";
    return nullptr;
  }

  int64_t seed_start_time_ms = beacon_seed->start_time_millis();
  int64_t offset_time_ms = timestamp_ms - seed_start_time_ms;
  int64_t start_of_period_ms =
      seed_start_time_ms + (offset_time_ms / kEidPeriodMs) * kEidPeriodMs;

  std::string eid = raw_eid_generator_->GenerateEid(
      beacon_seed->data(), start_of_period_ms, nullptr);

  return std::make_unique<DataWithTimestamp>(eid, start_of_period_ms,
                                             start_of_period_ms + kEidPeriodMs);
}

std::string BackgroundEidGenerator::IdentifyRemoteDeviceByAdvertisement(
    const std::string& advertisement_service_data,
    const multidevice::RemoteDeviceRefList& remote_devices) const {
  // Resize the service data to analyze only the first |kNumBytesInEidValue|
  // bytes. If there are any bytes after those first |kNumBytesInEidValue|
  // bytes, they are flags, so they are not needed to identify the device which
  // sent a message.
  std::string service_data_without_flags = advertisement_service_data;
  service_data_without_flags.resize(RawEidGenerator::kNumBytesInEidValue);

  const auto remote_device_it = base::ranges::find_if(
      remote_devices,
      [this, &service_data_without_flags](const auto& remote_device) {
        std::vector<DataWithTimestamp> eids = GenerateNearestEids(
            multidevice::ToCryptAuthSeedList(remote_device.beacon_seeds()));
        bool success = base::Contains(eids, service_data_without_flags,
                                      &DataWithTimestamp::data);
        std::stringstream ss;
        ss << "BackgroundEidGenerator::IdentifyRemoteDeviceByAdvertisement: "
           << (success ? "Identified " : "Failed to identify ")
           << "the following remote device from advertisement service data 0x"
           << base::HexEncode(service_data_without_flags) << ": "
           << "\n  device_name: " << remote_device.name()
           << "\n  device_id: " << remote_device.GetDeviceId()
           << "\n  beacon seeds: ";
        for (const auto& seed : remote_device.beacon_seeds()) {
          ss << "\n    " << seed;
        }
        ss << "\n  eids: " << DataWithTimestamp::ToDebugString(eids);
        PA_LOG(VERBOSE) << ss.str();

        return success;
      });

  // Return empty string if no matching device is found.
  return remote_device_it != remote_devices.end()
             ? remote_device_it->GetDeviceId()
             : std::string();
}

}  // namespace ash::secure_channel
