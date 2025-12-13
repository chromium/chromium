// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LIVE_LOCATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LIVE_LOCATION_PROVIDER_H_

#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

// This class always delivers real-time (live) location information.
//
// All `RequestLocation()` calls are directly forwarded to the internal
// `LocationFetcher` instance to initiate a new Geolocation API Web Service
// request, ensuring the latest possible reading.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) LiveLocationProvider
    : public LocationProvider {
 public:
  explicit LiveLocationProvider(
      std::unique_ptr<LocationFetcher> location_fetcher);
  void RequestLocation(base::TimeDelta timeout,
                       bool use_wifi,
                       bool use_cell_towers,
                       ResponseCallback callback) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LIVE_LOCATION_PROVIDER_H_
