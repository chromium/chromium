// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FOREGROUND_EID_GENERATOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FOREGROUND_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"

namespace base {
class Clock;
}

namespace cryptauth {
class BeaconSeed;
}

namespace ash::secure_channel {

class RawEidGenerator;

// Generates ephemeral ID (EID) values that are broadcast for foreground BLE
// advertisements in the ProximityAuth protocol.
//
// When advertising in foreground mode, we don't care about battery consumption
// while advertising. We assume, however, that the scanning side is
// battery-conscious, and is using hardware-based scanning.
//
// For the inverse of this model, in which advertising is battery-sensitive, see
// BackgroundEidGenerator.
//
// A peripheral-role device advertises a 4-byte advertisement with two parts: a
// 2-byte EID which is specific to the central-role device with which it intends
// to communicate, and a 2-byte EID which is specific to the peripheral-role
// device.
//
// This class uses EID seed values synced from the back-end to generate these
// EIDs.
//
// See go/proximity-auth-ble-advertising.
class ForegroundEidGenerator {
 public:
  // Data for both a current and adjacent EID. The current EID *must* be
  // supplied, but adjacent data may be null. Each EID consists of a 2-byte EID
  // value paired with the timestamp at which time this value becomes active or
  // inactive.
  struct EidData {
    enum AdjacentDataType { NONE, PAST, FUTURE };

    EidData(const DataWithTimestamp current_data,
            std::unique_ptr<DataWithTimestamp> adjacent_data);
    ~EidData();

    AdjacentDataType GetAdjacentDataType() const;
    std::string DataInHex() const;

    const DataWithTimestamp current_data;
    const std::unique_ptr<DataWithTimestamp> adjacent_data;
  };

  // The flag used to denote that a Bluetooth 4.0 device has sent an
  // advertisement. This flag indicates to the recipient that the sender cannot
  // act as both a central- and peripheral-role device simultaneously, so the
  // recipient should advertise back instead of initializing a connection.
  static const int8_t kBluetooth4Flag;

  ForegroundEidGenerator();

  ForegroundEidGenerator(const ForegroundEidGenerator&) = delete;
  ForegroundEidGenerator& operator=(const ForegroundEidGenerator&) = delete;

  virtual ~ForegroundEidGenerator();

  // Generates EID data for the given EID seeds to be used as a background scan
  // filter. In the normal case, two DataWithTimestamp values are returned, one
  // for each EID seed rotation period. If data has not been synced from the
  // backend recently and EID seeds are unavailable, nullptr is returned.
  virtual std::unique_ptr<EidData> GenerateBackgroundScanFilter(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const;

  // Generates advertisement data for the given EID seeds. If data has not been
  // synced from the back-end recently and EID seeds are unavailable, nullptr is
  // returned.
  virtual std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const;

  // Generates all possible advertisements that could be created by a device
  // given that device's public key and the beacon seeds of the device which is
  // intended to scan for the advertisement.
  virtual std::vector<std::string> GeneratePossibleAdvertisements(
      const std::string& advertising_device_public_key,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const;

  // Given a list of device IDs, returns the device ID which could have
  // produced the supplied advertisement service data. If none of the provided
  // device IDs could have produced the advertisement, an empty string is
  // returned.
  virtual std::string IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const std::vector<std::string>& device_ids,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const;

 private:
  struct EidPeriodTimestamps {
    int64_t current_period_start_timestamp_ms;
    int64_t current_period_end_timestamp_ms;
    int64_t adjacent_period_start_timestamp_ms;
    int64_t adjacent_period_end_timestamp_ms;
  };

  ForegroundEidGenerator(std::unique_ptr<RawEidGenerator> raw_eid_generator,
                         base::Clock* clock);

  std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms) const;

  std::unique_ptr<DataWithTimestamp> GenerateEidDataWithTimestamp(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms) const;

  std::unique_ptr<DataWithTimestamp> GenerateEidDataWithTimestamp(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms,
      std::string const* extra_entropy) const;

  std::unique_ptr<std::string> GetEidSeedForPeriod(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms) const;

  std::unique_ptr<EidPeriodTimestamps> GetEidPeriodTimestamps(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds)
      const;

  std::unique_ptr<EidPeriodTimestamps> GetEidPeriodTimestamps(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const bool allow_non_current_periods) const;

  std::unique_ptr<cryptauth::BeaconSeed> GetBeaconSeedForCurrentPeriod(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t current_time_ms) const;

  std::unique_ptr<EidPeriodTimestamps> GetClosestPeriod(
      const std::vector<cryptauth::BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t current_time_ms) const;

  static bool IsCurrentTimeAtStartOfEidPeriod(
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms,
      const int64_t current_timestamp_ms);

  raw_ptr<base::Clock> clock_;

  std::unique_ptr<RawEidGenerator> raw_eid_generator_;

  friend class SecureChannelForegroundEidGeneratorTest;
  FRIEND_TEST_ALL_PREFIXES(SecureChannelForegroundEidGeneratorTest,
                           GenerateBackgroundScanFilter_UsingRealEids);
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelForegroundEidGeneratorTest,
      GeneratePossibleAdvertisements_CurrentAndPastAdjacentPeriods);
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelForegroundEidGeneratorTest,
      testGeneratePossibleAdvertisements_CurrentAndFutureAdjacentPeriods);
  FRIEND_TEST_ALL_PREFIXES(SecureChannelForegroundEidGeneratorTest,
                           GeneratePossibleAdvertisements_OnlyCurrentPeriod);
  FRIEND_TEST_ALL_PREFIXES(SecureChannelForegroundEidGeneratorTest,
                           GeneratePossibleAdvertisements_OnlyFuturePeriod);
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelForegroundEidGeneratorTest,
      GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInFuture);
  FRIEND_TEST_ALL_PREFIXES(SecureChannelForegroundEidGeneratorTest,
                           GeneratePossibleAdvertisements_OnlyPastPeriod);
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelForegroundEidGeneratorTest,
      GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInPast);
  FRIEND_TEST_ALL_PREFIXES(
      SecureChannelForegroundEidGeneratorTest,
      GeneratePossibleAdvertisements_NoAdvertisements_EmptySeeds);
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FOREGROUND_EID_GENERATOR_H_
