// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_FETCHER_H_

#include <memory>
#include <vector>

#include "ash/constants/geolocation_access_level.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class GeolocationHandler;

// This class implements the Google Maps Geolocation API, abstracting the
// network details away from `SystemLocationProvider` component.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) LocationFetcher {
 public:
  static constexpr char kDefaultGeolocationProviderUrl[] =
      "https://www.googleapis.com/geolocation/v1/geolocate?";

  explicit LocationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // NOTE: Test-only, DO NOT use in production code.
  // Prefer this over *SetForTesting() methods.
  LocationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& location_service_url,
      GeolocationHandler* geolocation_handler);

  LocationFetcher(const LocationFetcher&) = delete;
  LocationFetcher& operator=(const LocationFetcher&) = delete;

  ~LocationFetcher();

  // Retrieves available WiFi and Cellular scan data from the
  // `GeolocationHandler`.
  void GetNetworkInformation(WifiAccessPointVector* wifi_vector,
                             CellTowerVector* cell_vector) const;

  // Initiates an asynchronous request to the Geolocation API Web Service.
  //
  // The request payload is augmented based on the provided flags:
  // * If 'use_wifi_scan' is true, available WiFi Access Point scan data is
  // included.
  // * If 'use_cellular_scan' is true, available Cell Tower scan data is
  // included.
  //
  // If the location request is not successfully resolved within the `timeout`
  // duration, callback is invoked with `Geoposition::Status::STATUS_TIMEOUT`.
  void RequestGeolocation(base::TimeDelta timeout,
                          bool use_wifi_scan,
                          bool use_cellular_scan,
                          LocationProvider::ResponseCallback callback);

  network::SharedURLLoaderFactory* GetSharedURLLoaderFactoryForTesting();
  void SetSharedUrlLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

 private:
  friend class LocationFetcherTest;

  // Geolocation response callback. Deletes request from requests_.
  void OnGeolocationResponse(SimpleGeolocationRequest* request,
                             LocationProvider::ResponseCallback callback,
                             const Geoposition& geoposition,
                             bool server_error,
                             const base::TimeDelta elapsed);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Requests in progress.
  // `LocationFetcher` owns all requests, so this vector is deleted
  // on destroy.
  std::vector<std::unique_ptr<SimpleGeolocationRequest>> requests_;

  // The Geolocation API service URL used for requests.
  // Defaults to `kDefaultGeolocationProviderUrl` but can be overridden for
  // tests.
  const GURL location_service_url_;

  raw_ptr<GeolocationHandler> geolocation_handler_ = nullptr;

  // Creation and destruction should happen on the same thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_LOCATION_FETCHER_H_
