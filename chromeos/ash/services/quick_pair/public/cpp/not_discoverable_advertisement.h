// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_
#define CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "chromeos/ash/services/quick_pair/public/cpp/battery_notification.h"

namespace ash {
namespace quick_pair {

// Fast Pair 'Not Discoverable' advertisement. See
// https://developers.google.com/nearby/fast-pair/spec#AdvertisingWhenNotDiscoverable
struct NotDiscoverableAdvertisement {
  NotDiscoverableAdvertisement();
  NotDiscoverableAdvertisement(
      std::vector<uint8_t> account_key_filter,
      bool show_ui,
      std::vector<uint8_t> salt,
      std::optional<BatteryNotification> battery_notification);
  NotDiscoverableAdvertisement(const NotDiscoverableAdvertisement&);
  NotDiscoverableAdvertisement(NotDiscoverableAdvertisement&&);
  NotDiscoverableAdvertisement& operator=(const NotDiscoverableAdvertisement&);
  NotDiscoverableAdvertisement& operator=(NotDiscoverableAdvertisement&&);
  ~NotDiscoverableAdvertisement();

  std::vector<uint8_t> account_key_filter;
  bool show_ui = false;
  std::vector<uint8_t> salt;
  std::optional<BatteryNotification> battery_notification;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_QUICK_PAIR_PUBLIC_CPP_NOT_DISCOVERABLE_ADVERTISEMENT_H_
