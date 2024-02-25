// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_status_model.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {
namespace phonehub {

bool PhoneStatusModel::MobileConnectionMetadata::operator==(
    const MobileConnectionMetadata& other) const {
  return signal_strength == other.signal_strength &&
         mobile_provider == other.mobile_provider;
}

bool PhoneStatusModel::MobileConnectionMetadata::operator!=(
    const MobileConnectionMetadata& other) const {
  return !(*this == other);
}

PhoneStatusModel::PhoneStatusModel(
    MobileStatus mobile_status,
    const std::optional<MobileConnectionMetadata>& mobile_connection_metadata,
    ChargingState charging_state,
    BatterySaverState battery_saver_state,
    uint32_t battery_percentage)
    : mobile_status_(mobile_status),
      mobile_connection_metadata_(mobile_connection_metadata),
      charging_state_(charging_state),
      battery_saver_state_(battery_saver_state),
      battery_percentage_(battery_percentage) {
  if (battery_percentage_ > 100) {
    PA_LOG(WARNING) << "Provided battery percentage (" << battery_percentage_
                    << "%) is >100%; setting to 100%.";
    battery_percentage_ = 100;
  }

  if (mobile_status_ != MobileStatus::kSimWithReception &&
      mobile_connection_metadata_.has_value()) {
    if (!mobile_connection_metadata_->mobile_provider.empty()) {
      // If the phone reports 0/4 bars but still reports a mobile provider, it
      // may be temporarily under-reporting the real signal strength shown on
      // the phone. See https://crbug.com/1163266 for details.

      PA_LOG(VERBOSE) << "Provided MobileStatus " << mobile_status_ << " "
                      << "indicates no reception, but it is connected to a "
                      << "mobile provider. Updating to one-bar reception.";
      mobile_status_ = MobileStatus::kSimWithReception;
      mobile_connection_metadata_->signal_strength = SignalStrength::kOneBar;
    } else {
      PA_LOG(WARNING) << "Provided MobileStatus " << mobile_status_ << " "
                      << "indicates no reception, but MobileConnectionMetadata "
                      << *mobile_connection_metadata_
                      << " was provided. Clearing "
                      << "metadata.";
      mobile_connection_metadata_.reset();
    }
  } else if (mobile_status_ == MobileStatus::kSimWithReception &&
             !mobile_connection_metadata_.has_value()) {
    PA_LOG(WARNING) << "MobileStatus is " << mobile_status_ << ", but no "
                    << "connection metadata was provided. Changing status to "
                    << MobileStatus::kSimButNoReception << ".";
    mobile_status_ = MobileStatus::kSimButNoReception;
  }
}

PhoneStatusModel::PhoneStatusModel(const PhoneStatusModel& other) = default;

PhoneStatusModel::~PhoneStatusModel() = default;

bool PhoneStatusModel::operator==(const PhoneStatusModel& other) const {
  return mobile_status_ == other.mobile_status_ &&
         mobile_connection_metadata_ == other.mobile_connection_metadata_ &&
         charging_state_ == other.charging_state_ &&
         battery_saver_state_ == other.battery_saver_state_ &&
         battery_percentage_ == other.battery_percentage_;
}

bool PhoneStatusModel::operator!=(const PhoneStatusModel& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::MobileStatus mobile_status) {
  switch (mobile_status) {
    case PhoneStatusModel::MobileStatus::kNoSim:
      stream << "[No SIM]";
      break;
    case PhoneStatusModel::MobileStatus::kSimButNoReception:
      stream << "[SIM present; no reception]";
      break;
    case PhoneStatusModel::MobileStatus::kSimWithReception:
      stream << "[SIM present with reception]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::SignalStrength signal_strength) {
  switch (signal_strength) {
    case PhoneStatusModel::SignalStrength::kZeroBars:
      stream << "[0 bars / 4]";
      break;
    case PhoneStatusModel::SignalStrength::kOneBar:
      stream << "[1 bar / 4]";
      break;
    case PhoneStatusModel::SignalStrength::kTwoBars:
      stream << "[2 bars / 4]";
      break;
    case PhoneStatusModel::SignalStrength::kThreeBars:
      stream << "[3 bars / 4]";
      break;
    case PhoneStatusModel::SignalStrength::kFourBars:
      stream << "[4 bars / 4]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    PhoneStatusModel::MobileConnectionMetadata mobile_connection_metadata) {
  stream << "{SignalStrength: " << mobile_connection_metadata.signal_strength
         << ", MobileProvider: \"" << mobile_connection_metadata.mobile_provider
         << "\"}";
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         PhoneStatusModel::ChargingState charging_state) {
  switch (charging_state) {
    case PhoneStatusModel::ChargingState::kNotCharging:
      stream << "[Not charging]";
      break;
    case PhoneStatusModel::ChargingState::kChargingAc:
      stream << "[Charging via an AC adapter]";
      break;
    case PhoneStatusModel::ChargingState::kChargingUsb:
      stream << "[Charging via USB]";
      break;
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    PhoneStatusModel::BatterySaverState battery_saver_state) {
  switch (battery_saver_state) {
    case PhoneStatusModel::BatterySaverState::kOff:
      stream << "[Battery Saver off]";
      break;
    case PhoneStatusModel::BatterySaverState::kOn:
      stream << "[Battery Saver on]";
      break;
  }
  return stream;
}

}  // namespace phonehub
}  // namespace ash
