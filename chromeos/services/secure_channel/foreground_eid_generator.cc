// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/foreground_eid_generator.h"

#include <cstring>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/secure_channel/raw_eid_generator.h"
#include "chromeos/services/secure_channel/raw_eid_generator_impl.h"

namespace chromeos {

namespace secure_channel {

namespace {
constexpr int64_t kNoTimestamp = 0;
constexpr int64_t kMaxPositiveInt64TValue = 0x7FFFFFFF;
constexpr base::TimeDelta kEidPeriod = base::TimeDelta::FromHours(8);
constexpr base::TimeDelta kBeginningOfEidPeriod = base::TimeDelta::FromHours(2);
}  // namespace

const int8_t ForegroundEidGenerator::kBluetooth4Flag = 0x01;

ForegroundEidGenerator::EidData::EidData(
    const DataWithTimestamp current_data,
    std::unique_ptr<DataWithTimestamp> adjacent_data)
    : current_data(current_data), adjacent_data(std::move(adjacent_data)) {}

ForegroundEidGenerator::EidData::~EidData() {}

ForegroundEidGenerator::EidData::AdjacentDataType
ForegroundEidGenerator::EidData::GetAdjacentDataType() const {
  if (!adjacent_data) {
    return AdjacentDataType::NONE;
  }

  if (adjacent_data->start_timestamp_ms < current_data.start_timestamp_ms) {
    return AdjacentDataType::PAST;
  }

  return AdjacentDataType::FUTURE;
}

std::string ForegroundEidGenerator::EidData::DataInHex() const {
  std::string str = "[" + current_data.DataInHex();

  if (adjacent_data) {
    return str + ", " + adjacent_data->DataInHex() + "]";
  }

  return str + "]";
}

ForegroundEidGenerator::ForegroundEidGenerator()
    : ForegroundEidGenerator(std::make_unique<RawEidGeneratorImpl>(),
                             base::DefaultClock::GetInstance()) {}

ForegroundEidGenerator::ForegroundEidGenerator(
    std::unique_ptr<RawEidGenerator> raw_eid_generator,
    base::Clock* clock)
    : clock_(clock), raw_eid_generator_(std::move(raw_eid_generator)) {}

ForegroundEidGenerator::~ForegroundEidGenerator() {}

std::unique_ptr<ForegroundEidGenerator::EidData>
ForegroundEidGenerator::GenerateBackgroundScanFilter(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  std::unique_ptr<EidPeriodTimestamps> timestamps =
      GetEidPeriodTimestamps(scanning_device_beacon_seeds);
  if (!timestamps) {
    // If the device does not have seeds for the correct period, no EIDs can be
    // generated.
    return nullptr;
  }

  std::unique_ptr<DataWithTimestamp> current_eid = GenerateEidDataWithTimestamp(
      scanning_device_beacon_seeds,
      timestamps->current_period_start_timestamp_ms,
      timestamps->current_period_end_timestamp_ms);
  if (!current_eid) {
    // The current EID could not be generated.
    return nullptr;
  }

  std::unique_ptr<DataWithTimestamp> adjacent_eid =
      GenerateEidDataWithTimestamp(
          scanning_device_beacon_seeds,
          timestamps->adjacent_period_start_timestamp_ms,
          timestamps->adjacent_period_end_timestamp_ms);
  return base::WrapUnique(new EidData(*current_eid, std::move(adjacent_eid)));
}

std::unique_ptr<DataWithTimestamp>
ForegroundEidGenerator::GenerateAdvertisement(
    const std::string& advertising_device_public_key,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  std::unique_ptr<EidPeriodTimestamps> timestamps =
      GetEidPeriodTimestamps(scanning_device_beacon_seeds);
  if (!timestamps) {
    return nullptr;
  }

  return GenerateAdvertisement(advertising_device_public_key,
                               scanning_device_beacon_seeds,
                               timestamps->current_period_start_timestamp_ms,
                               timestamps->current_period_end_timestamp_ms);
}

std::string ForegroundEidGenerator::IdentifyRemoteDeviceByAdvertisement(
    const std::string& advertisement_service_data,
    const std::vector<std::string>& device_ids,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  // Resize the service data to analyze only the first |2 * kNumBytesInEidValue|
  // bytes. The bytes following these are flags, so they are not needed to
  // identify the device which sent a message.
  std::string service_data_without_flags = advertisement_service_data;
  service_data_without_flags.resize(2 * RawEidGenerator::kNumBytesInEidValue);

  for (const auto& device_id : device_ids) {
    std::vector<std::string> possible_advertisements =
        GeneratePossibleAdvertisements(
            multidevice::RemoteDevice::DerivePublicKey(device_id),
            scanning_device_beacon_seeds);
    for (const auto& possible_advertisement : possible_advertisements) {
      if (service_data_without_flags == possible_advertisement) {
        return device_id;
      }
    }
  }

  return std::string();
}

std::vector<std::string> ForegroundEidGenerator::GeneratePossibleAdvertisements(
    const std::string& advertising_device_public_key,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  std::vector<std::string> possible_advertisements;

  std::unique_ptr<EidPeriodTimestamps> timestamps =
      GetEidPeriodTimestamps(scanning_device_beacon_seeds, true);
  if (!timestamps) {
    // If the devices do not have seeds for the correct period, no
    // advertisements can be generated.
    return possible_advertisements;
  }

  if (timestamps->current_period_start_timestamp_ms != kNoTimestamp) {
    std::unique_ptr<DataWithTimestamp> current_advertisement =
        GenerateAdvertisement(advertising_device_public_key,
                              scanning_device_beacon_seeds,
                              timestamps->current_period_start_timestamp_ms,
                              timestamps->current_period_end_timestamp_ms);
    if (current_advertisement) {
      possible_advertisements.push_back(current_advertisement->data);
    }
  }

  std::unique_ptr<DataWithTimestamp> adjacent_advertisement =
      GenerateAdvertisement(advertising_device_public_key,
                            scanning_device_beacon_seeds,
                            timestamps->adjacent_period_start_timestamp_ms,
                            timestamps->adjacent_period_end_timestamp_ms);
  if (adjacent_advertisement) {
    possible_advertisements.push_back(adjacent_advertisement->data);
  }

  return possible_advertisements;
}

std::unique_ptr<DataWithTimestamp>
ForegroundEidGenerator::GenerateAdvertisement(
    const std::string& advertising_device_public_key,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t start_of_period_timestamp_ms,
    const int64_t end_of_period_timestamp_ms) const {
  std::unique_ptr<DataWithTimestamp> advertising_device_identifying_data =
      GenerateEidDataWithTimestamp(
          scanning_device_beacon_seeds, start_of_period_timestamp_ms,
          end_of_period_timestamp_ms, &advertising_device_public_key);
  std::unique_ptr<DataWithTimestamp> scanning_device_identifying_data =
      GenerateEidDataWithTimestamp(scanning_device_beacon_seeds,
                                   start_of_period_timestamp_ms,
                                   end_of_period_timestamp_ms);
  if (!advertising_device_identifying_data ||
      !scanning_device_identifying_data) {
    return nullptr;
  }

  std::string full_advertisement = scanning_device_identifying_data->data +
                                   advertising_device_identifying_data->data;
  return base::WrapUnique(new DataWithTimestamp(full_advertisement,
                                                start_of_period_timestamp_ms,
                                                end_of_period_timestamp_ms));
}

std::unique_ptr<DataWithTimestamp>
ForegroundEidGenerator::GenerateEidDataWithTimestamp(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t start_of_period_timestamp_ms,
    const int64_t end_of_period_timestamp_ms) const {
  return GenerateEidDataWithTimestamp(scanning_device_beacon_seeds,
                                      start_of_period_timestamp_ms,
                                      end_of_period_timestamp_ms, nullptr);
}

std::unique_ptr<DataWithTimestamp>
ForegroundEidGenerator::GenerateEidDataWithTimestamp(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t start_of_period_timestamp_ms,
    const int64_t end_of_period_timestamp_ms,
    std::string const* extra_entropy) const {
  std::unique_ptr<std::string> eid_seed = GetEidSeedForPeriod(
      scanning_device_beacon_seeds, start_of_period_timestamp_ms);
  if (!eid_seed) {
    return nullptr;
  }

  std::string eid_data = raw_eid_generator_->GenerateEid(
      *eid_seed, start_of_period_timestamp_ms, extra_entropy);

  return base::WrapUnique(new DataWithTimestamp(
      eid_data, start_of_period_timestamp_ms, end_of_period_timestamp_ms));
}

std::unique_ptr<std::string> ForegroundEidGenerator::GetEidSeedForPeriod(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t start_of_period_timestamp_ms) const {
  for (auto seed : scanning_device_beacon_seeds) {
    if (seed.start_time_millis() <= start_of_period_timestamp_ms &&
        start_of_period_timestamp_ms < seed.end_time_millis()) {
      return base::WrapUnique(new std::string(seed.data()));
    }
  }

  return nullptr;
}

std::unique_ptr<ForegroundEidGenerator::EidPeriodTimestamps>
ForegroundEidGenerator::GetEidPeriodTimestamps(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  return GetEidPeriodTimestamps(scanning_device_beacon_seeds, false);
}

std::unique_ptr<ForegroundEidGenerator::EidPeriodTimestamps>
ForegroundEidGenerator::GetEidPeriodTimestamps(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const bool allow_non_current_periods) const {
  base::Time now = clock_->Now();
  int64_t current_time_ms = now.ToJavaTime();

  std::unique_ptr<cryptauth::BeaconSeed> current_seed =
      GetBeaconSeedForCurrentPeriod(scanning_device_beacon_seeds,
                                    current_time_ms);
  if (!current_seed) {
    if (!allow_non_current_periods) {
      // No seed existed for the current time.
      return nullptr;
    }

    return GetClosestPeriod(scanning_device_beacon_seeds, current_time_ms);
  }

  // Now that the start of the 14-day EID seed period has been determined, the
  // current EID period must be determined.
  for (int64_t start_of_eid_period_ms = current_seed->start_time_millis();
       start_of_eid_period_ms <= current_seed->end_time_millis();
       start_of_eid_period_ms += kEidPeriod.InMilliseconds()) {
    int64_t end_of_eid_period_ms =
        start_of_eid_period_ms + kEidPeriod.InMilliseconds();
    if (start_of_eid_period_ms <= current_time_ms &&
        current_time_ms < end_of_eid_period_ms) {
      int64_t start_of_adjacent_period_ms;
      int64_t end_of_adjacent_period_ms;
      if (IsCurrentTimeAtStartOfEidPeriod(
              start_of_eid_period_ms, end_of_eid_period_ms, current_time_ms)) {
        // If the current time is at the beginning of the period, the "adjacent
        // period" is the period before this one.
        start_of_adjacent_period_ms =
            start_of_eid_period_ms - kEidPeriod.InMilliseconds();
        end_of_adjacent_period_ms = start_of_eid_period_ms;
      } else {
        // Otherwise, the "adjacent period" is the one after this one.
        start_of_adjacent_period_ms = end_of_eid_period_ms;
        end_of_adjacent_period_ms =
            end_of_eid_period_ms + kEidPeriod.InMilliseconds();
      }

      return base::WrapUnique(new EidPeriodTimestamps{
          start_of_eid_period_ms, end_of_eid_period_ms,
          start_of_adjacent_period_ms, end_of_adjacent_period_ms});
    }
  }

  PA_LOG(ERROR) << "Could not find valid EID period for seed. "
                << "seed.start_timestamp_ms: "
                << current_seed->start_time_millis()
                << ", seed.end_timestamp_ms: "
                << current_seed->end_time_millis();
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<cryptauth::BeaconSeed>
ForegroundEidGenerator::GetBeaconSeedForCurrentPeriod(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t current_time_ms) const {
  for (auto seed : scanning_device_beacon_seeds) {
    int64_t seed_period_length_ms =
        seed.end_time_millis() - seed.start_time_millis();
    if (seed_period_length_ms % kEidPeriod.InMilliseconds() != 0) {
      PA_LOG(WARNING) << "Seed has period length which is not an multiple of "
                      << "the rotation length.";
      continue;
    }

    if (seed.start_time_millis() <= current_time_ms &&
        current_time_ms < seed.end_time_millis()) {
      return base::WrapUnique(new cryptauth::BeaconSeed(seed));
    }
  }

  return nullptr;
}

std::unique_ptr<ForegroundEidGenerator::EidPeriodTimestamps>
ForegroundEidGenerator::GetClosestPeriod(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
    const int64_t current_time_ms) const {
  int64_t smallest_diff_so_far_ms = kMaxPositiveInt64TValue;
  int64_t start_of_period_timestamp_ms = kMaxPositiveInt64TValue;
  int64_t end_of_period_timestamp_ms = 0;

  for (auto seed : scanning_device_beacon_seeds) {
    int64_t seed_period_length_ms =
        seed.end_time_millis() - seed.start_time_millis();
    if (seed_period_length_ms % kEidPeriod.InMilliseconds() != 0) {
      PA_LOG(WARNING) << "Seed has period length which is not an multiple of "
                      << "the rotation length.";
      continue;
    }

    DCHECK(seed.start_time_millis() > current_time_ms ||
           current_time_ms >= seed.end_time_millis());

    if (seed.start_time_millis() > current_time_ms) {
      int64_t diff = seed.start_time_millis() - current_time_ms;
      if (diff < smallest_diff_so_far_ms) {
        smallest_diff_so_far_ms = diff;
        start_of_period_timestamp_ms = seed.start_time_millis();
        end_of_period_timestamp_ms =
            seed.start_time_millis() + kEidPeriod.InMilliseconds();
      }
    } else if (seed.end_time_millis() <= current_time_ms) {
      int64_t diff = current_time_ms - seed.end_time_millis();
      if (diff < smallest_diff_so_far_ms) {
        smallest_diff_so_far_ms = diff;
        end_of_period_timestamp_ms = seed.end_time_millis();
        start_of_period_timestamp_ms =
            end_of_period_timestamp_ms - kEidPeriod.InMilliseconds();
      }
    }
  }

  if (smallest_diff_so_far_ms < kMaxPositiveInt64TValue &&
      smallest_diff_so_far_ms > kEidPeriod.InMilliseconds()) {
    return nullptr;
  }

  return base::WrapUnique(new EidPeriodTimestamps{
      kNoTimestamp,  // current_period_start_timestamp_ms is unused.
      kNoTimestamp,  // current_period_end_timestamp_ms is unused.
      start_of_period_timestamp_ms, end_of_period_timestamp_ms});
}

bool ForegroundEidGenerator::IsCurrentTimeAtStartOfEidPeriod(
    const int64_t start_of_period_timestamp_ms,
    const int64_t end_of_period_timestamp_ms,
    const int64_t current_timestamp_ms) {
  DCHECK(start_of_period_timestamp_ms <= current_timestamp_ms);
  DCHECK(current_timestamp_ms < end_of_period_timestamp_ms);

  return current_timestamp_ms <
         start_of_period_timestamp_ms + kBeginningOfEidPeriod.InMilliseconds();
}

}  // namespace secure_channel

}  // namespace chromeos
