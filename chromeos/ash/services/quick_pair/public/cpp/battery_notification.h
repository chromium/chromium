// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_BATTERY_NOTIFICATION_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_BATTERY_NOTIFICATION_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace ash {
namespace quick_pair {

// Fast Pair battery information from notification. See
// https://developers.google.com/nearby/fast-pair/spec#BatteryNotification
struct BatteryInfo {
  BatteryInfo();
  explicit BatteryInfo(bool is_charging);
  BatteryInfo(bool is_charging, int8_t percentage);

  BatteryInfo(const BatteryInfo&);
  BatteryInfo(BatteryInfo&&);
  BatteryInfo& operator=(const BatteryInfo&);
  BatteryInfo& operator=(BatteryInfo&&);
  ~BatteryInfo();

  static BatteryInfo FromByte(uint8_t byte);

  uint8_t ToByte() const;

  bool is_charging = false;
  std::optional<int8_t> percentage;
};

// Fast Pair battery notification. See
// https://developers.google.com/nearby/fast-pair/spec#BatteryNotification
struct BatteryNotification {
  BatteryNotification();
  BatteryNotification(bool show_ui,
                      BatteryInfo left_bud_info,
                      BatteryInfo right_bud_info,
                      BatteryInfo case_info);
  BatteryNotification(const BatteryNotification&);
  BatteryNotification(BatteryNotification&&);
  BatteryNotification& operator=(const BatteryNotification&);
  BatteryNotification& operator=(BatteryNotification&&);
  ~BatteryNotification();

  static std::optional<BatteryNotification> FromBytes(
      const std::vector<uint8_t>& bytes,
      bool show_ui);

  bool show_ui = false;
  BatteryInfo left_bud_info;
  BatteryInfo right_bud_info;
  BatteryInfo case_info;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_BATTERY_NOTIFICATION_H_
