// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_background_eid_generator.h"

#include <memory>

namespace chromeos {

namespace secure_channel {

FakeBackgroundEidGenerator::FakeBackgroundEidGenerator() = default;
FakeBackgroundEidGenerator::~FakeBackgroundEidGenerator() = default;

std::vector<DataWithTimestamp> FakeBackgroundEidGenerator::GenerateNearestEids(
    const std::vector<cryptauth::BeaconSeed>& beacon_seed) const {
  return *nearest_eids_;
}

std::string FakeBackgroundEidGenerator::IdentifyRemoteDeviceByAdvertisement(
    const std::string& advertisement_service_data,
    const multidevice::RemoteDeviceRefList& remote_devices) const {
  // Increment num_identify_calls_. Since this overrides a const method, some
  // hacking is needed to modify the num_identify_calls_ instance variable.
  int* num_identify_calls_ptr = const_cast<int*>(&num_identify_calls_);
  *num_identify_calls_ptr = *num_identify_calls_ptr + 1;

  // If only a specific matching service data should be valid, verify that it
  // matches.
  if (!matching_service_data_.empty() &&
      advertisement_service_data != matching_service_data_) {
    return std::string();
  }

  return identified_device_id_;
}

}  // namespace secure_channel

}  // namespace chromeos