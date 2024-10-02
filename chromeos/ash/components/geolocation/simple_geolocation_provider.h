// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_

#include <memory>
#include <vector>

#include "ash/constants/geolocation_access_level.h"
#include "base/check_is_test.h"
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
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class GeolocationHandler;

// `SimpleGeolocationProvider` watches geolocation permissions and serves
// geolocation requests to its clients by implementing Google Maps Geolocation
// API. All system services need to use this class to get geolocation data and
// subscribe to it for permission updates.
// Note: Arc++ and PWAs have different pipelines for retrieving geolocation.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    SimpleGeolocationProvider {
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

  SimpleGeolocationProvider(const SimpleGeolocationProvider&) = delete;
  SimpleGeolocationProvider& operator=(const SimpleGeolocationProvider&) =
      delete;

  virtual ~SimpleGeolocationProvider();

  // This function has to be called first thing before using other members.
  static void Initialize(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  static SimpleGeolocationProvider* GetInstance();

  static GURL DefaultGeolocationProviderURL() {
    return GURL(kGeolocationProviderUrl);
  }

  GeolocationAccessLevel GetGeolocationAccessLevel() const;
  void SetGeolocationAccessLevel(
      GeolocationAccessLevel geolocation_access_level);

  // Convenience method for clients to read underlying `GeolocationAccessLevel`
  // as a boolean value.
  bool IsGeolocationUsageAllowedForSystem();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Initiates new request. If |send_wifi_access_points|, WiFi AP information
  // will be added to the request, similarly for |send_cell_towers| and Cell
  // Tower information. See `SimpleGeolocationRequest` for the description of
  // the other parameters.
  void RequestGeolocation(base::TimeDelta timeout,
                          bool send_wifi_access_points,
                          bool send_cell_towers,
                          SimpleGeolocationRequest::ResponseCallback callback,
                          ClientId client_id);

  network::SharedURLLoaderFactory* GetSharedURLLoaderFactoryForTesting() {
    return shared_url_loader_factory_.get();
  }

  static void DestroyForTesting();
  void SetSharedUrlLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  void SetGeolocationProviderUrlForTesting(const char* url);

 private:
  static constexpr char kGeolocationProviderUrl[] =
      "https://www.googleapis.com/geolocation/v1/geolocate?";

  // This class is a singleton.
  explicit SimpleGeolocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> factory);

  friend class TestGeolocationAPILoaderFactory;
  FRIEND_TEST_ALL_PREFIXES(SimpleGeolocationWirelessTest, CellularExists);
  FRIEND_TEST_ALL_PREFIXES(SimpleGeolocationWirelessTest, WiFiExists);

  // Geolocation response callback. Deletes request from requests_.
  void OnGeolocationResponse(
      SimpleGeolocationRequest* request,
      SimpleGeolocationRequest::ResponseCallback callback,
      const Geoposition& geoposition,
      bool server_error,
      const base::TimeDelta elapsed);

  // Returns `DefaultGeolocaitonProivdeURL()` for production. Can be
  // overridden in tests.
  std::string GetGeolocationProviderUrl() const;

  void set_geolocation_handler(GeolocationHandler* geolocation_handler) {
    geolocation_handler_ = geolocation_handler;
  }

  void NotifyObservers();

  // Records UMA metrics related to geolocation requests, including the
  // distribution of requests per ClientId. This function tracks the frequency
  // of requests by measuring the time intervals between consecutive requests
  // and categorizing them into hourly buckets.
  void RecordClientIdUma(ClientId client_id);

  // Source of truth for the current geolocation access level.
  // Takes into consideration geolocation policies, log-in and in-session
  // geolocation prefs and is being updated on relevant events.
  GeolocationAccessLevel geolocation_access_level_ =
      GeolocationAccessLevel::kAllowed;

  base::ObserverList<Observer> observer_list_;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Requests in progress.
  // `SimpleGeolocationProvider` owns all requests, so this vector is deleted
  // on destroy.
  std::vector<std::unique_ptr<SimpleGeolocationRequest>> requests_;

  raw_ptr<GeolocationHandler> geolocation_handler_ = nullptr;

  std::string url_for_testing_;

  // Stores the time of the last geolocation request for each client ID. This is
  // used to calculate the time gap between requests for metrics reporting.
  base::flat_map<ClientId, base::TimeTicks> last_request_times_;

  // Creation and destruction should happen on the same thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
