
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/live_location_provider.h"

#include "chromeos/ash/components/geolocation/location_fetcher.h"

namespace ash {

LiveLocationProvider::LiveLocationProvider(
    std::unique_ptr<LocationFetcher> location_fetcher)
    : LocationProvider(std::move(location_fetcher)) {}

void LiveLocationProvider::RequestLocation(base::TimeDelta timeout,
                                           bool use_wifi,
                                           bool use_cell_towers,
                                           ResponseCallback callback) {
  location_fetcher_->RequestGeolocation(timeout, use_wifi, use_cell_towers,
                                        std::move(callback));
}

}  // namespace ash
