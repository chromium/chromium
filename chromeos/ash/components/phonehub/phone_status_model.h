// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_MODEL_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>

namespace ash {
namespace phonehub {

// Contains properties representing a phone's status, including mobile
// connection state and battery/power state.
class PhoneStatusModel {
 public:
  enum class MobileStatus {
    // The phone does not have a physical SIM inserted or an eSIM profile set
    // up.
    kNoSim = 0,

    // The phone has a SIM, but it is not connected to a mobile network.
    kSimButNoReception = 1,

    // The phone has a SIM and is connected to a mobile network using that SIM.
    kSimWithReception = 2
  };

  // Number of "bars" in the connection strength; only applies when the device
  // has reception.
  enum class SignalStrength {
    kZeroBars = 0,
    kOneBar = 1,
    kTwoBars = 2,
    kThreeBars = 3,
    kFourBars = 4
  };

  struct MobileConnectionMetadata {
    bool operator==(const MobileConnectionMetadata& other) const;
    bool operator!=(const MobileConnectionMetadata& other) const;

    SignalStrength signal_strength;

    // Name of the service provider (e.g., "Google Fi").
    std::u16string mobile_provider;
  };

  enum class ChargingState {
    // Not charging (i.e., on battery power).
    kNotCharging = 0,

    // Charging via AC adapter.
    kChargingAc = 1,

    // Charging via a USB connection.
    kChargingUsb = 2
  };

  // Android devices can enable "battery saver" mode, which causes the battery
  // charge to last longer by reducing/eliminating functionality with
  // significant power impact.
  enum class BatterySaverState { kOff = 0, kOn = 1 };

  // Note: If |mobile_status| is not kSimWithReception,
  // |mobile_connection_metadata| should be null.
  PhoneStatusModel(
      MobileStatus mobile_status,
      const std::optional<MobileConnectionMetadata>& mobile_connection_metadata,
      ChargingState charging_state,
      BatterySaverState battery_saver_state,
      uint32_t battery_percentage);
  PhoneStatusModel(const PhoneStatusModel& other);
  ~PhoneStatusModel();

  bool operator==(const PhoneStatusModel& other) const;
  bool operator!=(const PhoneStatusModel& other) const;

  MobileStatus mobile_status() const { return mobile_status_; }

  // Note: Null when mobile_status() is not kSimWithReception.
  const std::optional<MobileConnectionMetadata>& mobile_connection_metadata()
      const {
    return mobile_connection_metadata_;
  }

  ChargingState charging_state() const { return charging_state_; }

  BatterySaverState battery_saver_state() const { return battery_saver_state_; }

  uint32_t battery_percentage() const { return battery_percentage_; }

 private:
  MobileStatus mobile_status_;
  std::optional<MobileConnectionMetadata> mobile_connection_metadata_;
  ChargingState charging_state_;
  BatterySaverState battery_saver_state_;
  uint32_t battery_percentage_;
};

std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::MobileStatus mobile_status);
std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::SignalStrength signal_strength);
std::ostream& operator<<(
    std::ostream& stream,
    PhoneStatusModel::MobileConnectionMetadata mobile_connection_metadata);
std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::ChargingState charging_state);
std::ostream& operator<<(
    std::ostream& stream,
    PhoneStatusModel::BatterySaverState battery_saver_state);

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_STATUS_MODEL_H_
