// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/mock_foreground_eid_generator.h"

#include <memory>

namespace ash::secure_channel {

MockForegroundEidGenerator::MockForegroundEidGenerator()
    : num_identify_calls_(0) {}

MockForegroundEidGenerator::~MockForegroundEidGenerator() {}

std::unique_ptr<ForegroundEidGenerator::EidData>
MockForegroundEidGenerator::GenerateBackgroundScanFilter(
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  if (!background_scan_filter_) {
    return nullptr;
  }

  std::unique_ptr<DataWithTimestamp> adjacent_data;
  if (background_scan_filter_->adjacent_data) {
    adjacent_data = std::make_unique<DataWithTimestamp>(
        background_scan_filter_->adjacent_data->data,
        background_scan_filter_->adjacent_data->start_timestamp_ms,
        background_scan_filter_->adjacent_data->end_timestamp_ms);
  }

  return std::make_unique<EidData>(background_scan_filter_->current_data,
                                   std::move(adjacent_data));
}

std::unique_ptr<DataWithTimestamp>
MockForegroundEidGenerator::GenerateAdvertisement(
    const std::string& advertising_device_public_key,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  if (!advertisement_) {
    return nullptr;
  }

  return std::make_unique<DataWithTimestamp>(advertisement_->data,
                                             advertisement_->start_timestamp_ms,
                                             advertisement_->end_timestamp_ms);
}

std::vector<std::string>
MockForegroundEidGenerator::GeneratePossibleAdvertisements(
    const std::string& advertising_device_public_key,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  if (!possible_advertisements_) {
    return std::vector<std::string>();
  }

  return *possible_advertisements_;
}

std::string MockForegroundEidGenerator::IdentifyRemoteDeviceByAdvertisement(
    const std::string& advertisement_service_data,
    const std::vector<std::string>& device_id_list,
    const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
    const {
  // Increment num_identify_calls_. Since this overrides a const method, some
  // hacking is needed to modify the num_identify_calls_ instance variable.
  int* num_identify_calls_ptr = const_cast<int*>(&num_identify_calls_);
  *num_identify_calls_ptr = *num_identify_calls_ptr + 1;

  return identified_device_id_;
}

}  // namespace ash::secure_channel
