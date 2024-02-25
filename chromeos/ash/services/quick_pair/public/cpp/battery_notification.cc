// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/battery_notification.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/logging.h"

namespace ash {
namespace quick_pair {

constexpr int kBatteryChargingMask = 0b10000000;
constexpr int kBatteryPercentageMask = 0b01111111;

BatteryInfo::BatteryInfo() = default;

BatteryInfo::BatteryInfo(bool is_charging)
    : is_charging(is_charging), percentage(std::nullopt) {}

BatteryInfo::BatteryInfo(bool is_charging, int8_t percentage)
    : is_charging(is_charging), percentage(percentage) {}

BatteryInfo::BatteryInfo(const BatteryInfo&) = default;

BatteryInfo::BatteryInfo(BatteryInfo&&) = default;

BatteryInfo& BatteryInfo::operator=(const BatteryInfo&) = default;

BatteryInfo& BatteryInfo::operator=(BatteryInfo&&) = default;

BatteryInfo::~BatteryInfo() = default;

// static
BatteryInfo BatteryInfo::FromByte(uint8_t byte) {
  // Battery value is in the form 0bSVVVVVVV.
  // S = charinging (0b1) or not (0b0).
  // V = value, Ranges from 0-100, or 0bS1111111 if unknown.
  bool is_charging = byte & kBatteryChargingMask;
  uint8_t percentage = byte & kBatteryPercentageMask;
  int8_t percentage_signed = static_cast<int8_t>(percentage);

  if (percentage == kBatteryPercentageMask || percentage_signed < 0 ||
      percentage_signed > 100) {
    LOG(WARNING) << "Invalid battery percentage.";
    return BatteryInfo(is_charging);
  } else {
    return BatteryInfo(is_charging, percentage_signed);
  }
}

uint8_t BatteryInfo::ToByte() const {
  // Battery value is in the form 0bSVVVVVVV.
  // S = charinging (0b1) or not (0b0).
  // V = value, Ranges from 0-100, or 1111111 if unknown.
  if (!percentage) {
    return is_charging ? 0b11111111 : 0b01111111;
  } else {
    return percentage.value() | (is_charging ? 0b10000000 : 0);
  }
}

BatteryNotification::BatteryNotification() = default;

BatteryNotification::BatteryNotification(bool show_ui,
                                         BatteryInfo left_bud_info,
                                         BatteryInfo right_bud_info,
                                         BatteryInfo case_info)
    : show_ui(show_ui),
      left_bud_info(std::move(left_bud_info)),
      right_bud_info(std::move(right_bud_info)),
      case_info(std::move(case_info)) {}

BatteryNotification::BatteryNotification(const BatteryNotification&) = default;

BatteryNotification::BatteryNotification(BatteryNotification&&) = default;

BatteryNotification& BatteryNotification::operator=(
    const BatteryNotification&) = default;

BatteryNotification& BatteryNotification::operator=(BatteryNotification&&) =
    default;

BatteryNotification::~BatteryNotification() = default;

// static
std::optional<BatteryNotification> BatteryNotification::FromBytes(
    const std::vector<uint8_t>& bytes,
    bool show_ui) {
  // Expecting 3 bytes - Left bud, Right bud and case.
  if (bytes.size() != 3) {
    LOG(WARNING) << __func__
                 << ": Invalid bytes length. Expected 3, got: " << bytes.size();
    return std::nullopt;
  }

  BatteryInfo left_bud_info = BatteryInfo::FromByte(bytes[0]);
  BatteryInfo right_bud_info = BatteryInfo::FromByte(bytes[1]);
  BatteryInfo case_info = BatteryInfo::FromByte(bytes[2]);

  return std::make_optional<BatteryNotification>(show_ui, left_bud_info,
                                                 right_bud_info, case_info);
}

}  // namespace quick_pair
}  // namespace ash
