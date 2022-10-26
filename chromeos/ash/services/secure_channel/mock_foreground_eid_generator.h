// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_MOCK_FOREGROUND_EID_GENERATOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_MOCK_FOREGROUND_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "chromeos/ash/services/secure_channel/foreground_eid_generator.h"

namespace cryptauth {
class BeaconSeed;
}

namespace ash::secure_channel {

// Mock class for ForegroundEidGenerator. Note that GoogleMock cannot be used to
// mock this class because GoogleMock's mock functions cannot return a
// |std::unique_ptr|.
class MockForegroundEidGenerator : public ForegroundEidGenerator {
 public:
  MockForegroundEidGenerator();
  ~MockForegroundEidGenerator() override;

  // Setters for the return values of the overridden functions below.
  void set_background_scan_filter(
      std::unique_ptr<EidData> background_scan_filter) {
    background_scan_filter_ = std::move(background_scan_filter);
  }

  void set_advertisement(std::unique_ptr<DataWithTimestamp> advertisement) {
    advertisement_ = std::move(advertisement);
  }

  void set_possible_advertisements(
      std::unique_ptr<std::vector<std::string>> possible_advertisements) {
    possible_advertisements_ = std::move(possible_advertisements);
  }

  void set_identified_device_id(const std::string& identified_device_id) {
    identified_device_id_ = identified_device_id;
  }

  // ForegroundEidGenerator:
  std::unique_ptr<EidData> GenerateBackgroundScanFilter(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const override;
  std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const override;
  std::vector<std::string> GeneratePossibleAdvertisements(
      const std::string& advertising_device_public_key,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const override;
  std::string IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const std::vector<std::string>& device_id_list,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const override;

  int num_identify_calls() { return num_identify_calls_; }

 private:
  std::unique_ptr<EidData> background_scan_filter_;
  std::unique_ptr<DataWithTimestamp> advertisement_;
  std::unique_ptr<std::vector<std::string>> possible_advertisements_;
  std::string identified_device_id_;

  int num_identify_calls_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_MOCK_FOREGROUND_EID_GENERATOR_H_
