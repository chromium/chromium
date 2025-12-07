// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_

#include <memory>
#include <vector>

#include "ash/constants/geolocation_access_level.h"
#include "base/check_is_test.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {

// Serves as the central authority and access point for all geolocation-related
// matters for ChromeOS system services
//
// All system services MUST use this class for:
// (1) Obtaining geographical coordinates.
// (2) Querying system location permission status and subscribing to updates.
//
// Note: ARC++ and PWAs handle geolocation retrieval separately.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    SystemLocationProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnGeolocationPermissionChanged(bool enabled) = 0;
  };

  enum class ClientId {
    kGeolocationController = 0,
    kWizardController = 1,
    kTimezoneResolver = 2,
    kForTesting = 3,
    kMaxValue = kForTesting,
  };

  SystemLocationProvider(const SystemLocationProvider&) = delete;
  SystemLocationProvider& operator=(const SystemLocationProvider&) = delete;

  virtual ~SystemLocationProvider();

  // Initializes the global singleton instance and injects the core location
  // fetcher implementation.
  // NOTE: Must be called before accessing other members.
  static void Initialize(std::unique_ptr<LocationProvider> location_provider);

  static SystemLocationProvider* GetInstance();

  GeolocationAccessLevel GetGeolocationAccessLevel() const;
  void SetGeolocationAccessLevel(
      GeolocationAccessLevel geolocation_access_level);

  // Convenience method for clients to read underlying `GeolocationAccessLevel`
  // as a boolean value.
  bool IsGeolocationUsageAllowedForSystem();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Asynchronous request for device geolocation.
  //
  // If 'use_wifi_scan' is true, the returned location was based on available
  // WiFi scan data, to improve accuracy.
  // If 'use_cellular_scan' is true, the returned location was based on
  // available Cellular scan data, to improve accuracy.
  //
  // If the location request is not successfully resolved within the `timeout`
  // duration, callback is invoked with `Geoposition::Status::STATUS_TIMEOUT`.
  void RequestGeolocation(base::TimeDelta timeout,
                          bool use_wifi_scan,
                          bool use_cellular_scan,
                          LocationProvider::ResponseCallback callback,
                          ClientId client_id);

  static void DestroyForTesting();
  LocationProvider* GetLocationProviderForTesting() {
    return location_provider_.get();
  }

 private:
  // This class is a singleton.
  explicit SystemLocationProvider(
      std::unique_ptr<LocationProvider> location_provider);

  FRIEND_TEST_ALL_PREFIXES(SystemLocationProviderWirelessTest, CellularExists);
  FRIEND_TEST_ALL_PREFIXES(SystemLocationProviderWirelessTest, WiFiExists);

  // Geolocation response callback. Deletes request from requests_.
  void OnGeolocationResponse(SimpleGeolocationRequest* request,
                             LocationProvider::ResponseCallback callback,
                             const Geoposition& geoposition,
                             bool server_error,
                             const base::TimeDelta elapsed);

  void NotifyObservers();

  // Records UMA metrics related to geolocation requests, including the
  // distribution of requests per ClientId. This function tracks the frequency
  // of requests by measuring the time intervals between consecutive requests
  // and categorizing them into hourly buckets.
  void RecordClientIdUma(ClientId client_id);

  // Encapsulating the location provision strategy.
  std::unique_ptr<LocationProvider> location_provider_;

  // Source of truth for the current geolocation access level.
  // Takes into consideration geolocation policies, log-in and in-session
  // geolocation prefs and is being updated on relevant events.
  GeolocationAccessLevel geolocation_access_level_ =
      GeolocationAccessLevel::kAllowed;

  base::ObserverList<Observer> observer_list_;

  // Stores the time of the last geolocation request for each client ID. This is
  // used to calculate the time gap between requests for metrics reporting.
  base::flat_map<ClientId, base::TimeTicks> last_request_times_;

  // Creation and destruction should happen on the same thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_
