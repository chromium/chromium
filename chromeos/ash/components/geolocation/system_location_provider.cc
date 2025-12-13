// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/system_location_provider.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "ash/constants/geolocation_access_level.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "chromeos/ash/components/network/geolocation_handler_impl.h"
#include "chromeos/ash/components/network/network_handler.h"

namespace ash {

namespace {

SystemLocationProvider* g_geolocation_provider = nullptr;

// Maximum interval (in hours) to record in the request interval histogram.
// We track intervals within a day to identify clients that excessively request
// geolocation updates (e.g., hourly). Longer intervals are considered normal.
constexpr static int kMaxRequestIntervalHistogramHours = 24;

std::string_view GetClientIdUmaName(
    SystemLocationProvider::ClientId client_id) {
  switch (client_id) {
    case SystemLocationProvider::ClientId::kGeolocationController:
      return "SimpleGeolocation.Provider.GeolocationControllerRequestInterval";
    case SystemLocationProvider::ClientId::kWizardController:
      return "SimpleGeolocation.Provider.WizardControllerRequestInterval";
    case SystemLocationProvider::ClientId::kTimezoneResolver:
      return "SimpleGeolocation.Provider.TimezoneResolverRequestInterval";
    case SystemLocationProvider::ClientId::kForTesting:
      // This case is unused but required to avoid a compiler warning about
      // missing 'default' case in the switch.
      break;
  }
  NOTREACHED() << "GetClientIdUmaName: An invalid client_id is passed";
}

}  // namespace

SystemLocationProvider::SystemLocationProvider(
    std::unique_ptr<LocationProvider> location_provider)
    : location_provider_(std::move(location_provider)) {}

SystemLocationProvider::~SystemLocationProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
void SystemLocationProvider::Initialize(
    std::unique_ptr<LocationProvider> location_provider) {
  CHECK_EQ(g_geolocation_provider, nullptr);

  g_geolocation_provider =
      new SystemLocationProvider(std::move(location_provider));
}

// static
SystemLocationProvider* SystemLocationProvider::GetInstance() {
  CHECK_NE(g_geolocation_provider, nullptr);
  return g_geolocation_provider;
}

GeolocationAccessLevel SystemLocationProvider::GetGeolocationAccessLevel()
    const {
  return geolocation_access_level_;
}

void SystemLocationProvider::SetGeolocationAccessLevel(
    GeolocationAccessLevel geolocation_access_level) {
  bool system_geo_usage_allowed = IsGeolocationUsageAllowedForSystem();
  geolocation_access_level_ = geolocation_access_level;

  if (system_geo_usage_allowed != IsGeolocationUsageAllowedForSystem()) {
    NotifyObservers();
  }
}

void SystemLocationProvider::AddObserver(Observer* obs) {
  CHECK(obs);
  CHECK(!observer_list_.HasObserver(obs));
  observer_list_.AddObserver(obs);
}

void SystemLocationProvider::RemoveObserver(Observer* obs) {
  CHECK(obs);
  CHECK(observer_list_.HasObserver(obs));
  observer_list_.RemoveObserver(obs);
}

void SystemLocationProvider::RequestGeolocation(
    base::TimeDelta timeout,
    bool use_wifi_scan,
    bool use_cellular_scan,
    LocationProvider::ResponseCallback callback,
    ClientId client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordClientIdUma(client_id);

  // Drop request if the system geolocation permission is not granted for
  // system services.
  if (!IsGeolocationUsageAllowedForSystem()) {
    return;
  }

  location_provider_->RequestLocation(timeout, use_wifi_scan, use_cellular_scan,
                                      std::move(callback));
}

// static
void SystemLocationProvider::DestroyForTesting() {
  CHECK_IS_TEST();
  CHECK_NE(g_geolocation_provider, nullptr);
  delete g_geolocation_provider;
  g_geolocation_provider = nullptr;
}

bool SystemLocationProvider::IsGeolocationUsageAllowedForSystem() {
  switch (geolocation_access_level_) {
    case GeolocationAccessLevel::kAllowed:
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
      return true;
    case GeolocationAccessLevel::kDisallowed:
      return false;
  }
}

void SystemLocationProvider::OnGeolocationResponse(
    SimpleGeolocationRequest* request,
    LocationProvider::ResponseCallback callback,
    const Geoposition& geoposition,
    bool server_error,
    const base::TimeDelta elapsed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(callback).Run(geoposition, server_error, elapsed);
}

void SystemLocationProvider::NotifyObservers() {
  for (auto& obs : observer_list_) {
    obs.OnGeolocationPermissionChanged(IsGeolocationUsageAllowedForSystem());
  }
}

void SystemLocationProvider::RecordClientIdUma(ClientId client_id) {
  if (client_id == ClientId::kForTesting) {
    // Requests from tests are not relevant for metrics and can be skipped
    return;
  }

  base::UmaHistogramEnumeration("SimpleGeolocation.Provider.ClientId",
                                client_id);
  base::TimeTicks now = base::TimeTicks::Now();
  auto it = last_request_times_.find(client_id);
  if (it != last_request_times_.end()) {
    base::UmaHistogramExactLinear(GetClientIdUmaName(client_id),
                                  (now - it->second).InHours(),
                                  kMaxRequestIntervalHistogramHours);
  }
  last_request_times_[client_id] = now;
}

}  // namespace ash
