// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BACKGROUND_EID_GENERATOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BACKGROUND_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "chromeos/services/secure_channel/background_eid_generator.h"

namespace cryptauth {
class BeaconSeed;
}  // namespace cryptauth

namespace chromeos {

namespace secure_channel {

// Test double class for BackgroundEidGenerator.
class FakeBackgroundEidGenerator : public BackgroundEidGenerator {
 public:
  FakeBackgroundEidGenerator();
  ~FakeBackgroundEidGenerator() override;

  // BackgroundEidGenerator:
  std::vector<DataWithTimestamp> GenerateNearestEids(
      const std::vector<cryptauth::BeaconSeed>& beacon_seed) const override;
  std::string IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const multidevice::RemoteDeviceRefList& remote_devices) const override;

  void set_nearest_eids_(
      std::unique_ptr<std::vector<DataWithTimestamp>> nearest_eids) {
    nearest_eids_ = std::move(nearest_eids);
  }

  void set_identified_device_id(const std::string& identified_device_id) {
    identified_device_id_ = identified_device_id;
  }

  void set_matching_service_data(const std::string& matching_service_data) {
    matching_service_data_ = matching_service_data;
  }

  int num_identify_calls() { return num_identify_calls_; }

 private:
  std::unique_ptr<std::vector<DataWithTimestamp>> nearest_eids_;
  std::string identified_device_id_;
  std::string matching_service_data_;

  int num_identify_calls_ = 0;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BACKGROUND_EID_GENERATOR_H_
