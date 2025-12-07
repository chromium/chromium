// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"

namespace ash {

class LocationFetcher;

// Defines the interface for all concrete location resolution strategies.
//
// This interface dictates the contract for how geolocation requests are
// serviced. `SystemLocationProvider` holds ownership of a concrete
// implementation,
//
// Concrete strategy implementations include:
// * LiveLocationProvider: Delivers real-time (live) location data for every
// request.
// * CachedLocationProvider: Serves cached location data when validity criteria
// are met, to prevent excessive Geolocation API usage.
class LocationProvider {
 public:
  // Called when a new geolocation information is available.
  // The second argument indicates whether there was a server error or not.
  // It is true when there was a server or network error - either no response
  // or a 500 error code.
  using ResponseCallback = base::OnceCallback<void(const Geoposition& position,
                                                   bool server_error,
                                                   base::TimeDelta elapsed)>;

  explicit LocationProvider(std::unique_ptr<LocationFetcher> location_fetcher);
  virtual ~LocationProvider();

  // Resolves the device's geographical position.
  //
  // The result is delivered through `callback` either synchronously or
  // asynchronously, depending on the implementation.
  // The resolved location is based on the device's IP address and
  // on network scan data when specified by the flags.
  //
  // Parameters:
  // `timeout`: The maximum duration for the network fetch. If exceeded,
  //            `callback` is invoked with `STATUS_TIMEOUT` status.
  // `use_wifi`: If true, the returned location will [also] be based on Wi-Fi
  //             scan data.
  // `use_cell_towers`: If true, the returned location will [also] be based on
  //                    Cellular scan.
  // `callback`: Location is returned through this callback. Callers must handle
  //             the possibility of synchronous invocation (e.g. on a cache
  //             hit).
  virtual void RequestLocation(base::TimeDelta timeout,
                               bool use_wifi,
                               bool use_cell_towers,
                               ResponseCallback callback) = 0;

  LocationFetcher* GetLocationFetcherForTesting() {
    return location_fetcher_.get();
  }

 protected:
  std::unique_ptr<LocationFetcher> location_fetcher_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_PROVIDER_H_
