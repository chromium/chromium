// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"

namespace ash {
namespace quick_pair {

NotDiscoverableAdvertisement::NotDiscoverableAdvertisement() = default;

NotDiscoverableAdvertisement::NotDiscoverableAdvertisement(
    std::vector<uint8_t> account_key_filter,
    bool show_ui,
    std::vector<uint8_t> salt,
    std::optional<BatteryNotification> battery_notification)
    : account_key_filter(std::move(account_key_filter)),
      show_ui(show_ui),
      salt(std::move(salt)),
      battery_notification(std::move(battery_notification)) {}

NotDiscoverableAdvertisement::NotDiscoverableAdvertisement(
    const NotDiscoverableAdvertisement&) = default;

NotDiscoverableAdvertisement::NotDiscoverableAdvertisement(
    NotDiscoverableAdvertisement&&) = default;

NotDiscoverableAdvertisement& NotDiscoverableAdvertisement::operator=(
    const NotDiscoverableAdvertisement&) = default;

NotDiscoverableAdvertisement& NotDiscoverableAdvertisement::operator=(
    NotDiscoverableAdvertisement&&) = default;

NotDiscoverableAdvertisement::~NotDiscoverableAdvertisement() = default;

}  // namespace quick_pair
}  // namespace ash
