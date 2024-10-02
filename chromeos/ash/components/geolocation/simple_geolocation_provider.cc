// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"

#include <iterator>
#include <memory>

#include "ash/constants/geolocation_access_level.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

SimpleGeolocationProvider* g_geolocation_provider = nullptr;

// Maximum interval (in hours) to record in the request interval histogram.
// We track intervals within a day to identify clients that excessively request
// geolocation updates (e.g., hourly). Longer intervals are considered normal.
constexpr static int kMaxRequestIntervalHistogramHours = 24;

std::string_view GetClientIdUmaName(
    SimpleGeolocationProvider::ClientId client_id) {
  switch (client_id) {
    case SimpleGeolocationProvider::ClientId::kGeolocationController:
      return "SimpleGeolocation.Provider.GeolocationControllerRequestInterval";
    case SimpleGeolocationProvider::ClientId::kWizardController:
      return "SimpleGeolocation.Provider.WizardControllerRequestInterval";
    case SimpleGeolocationProvider::ClientId::kTimezoneResolver:
      return "SimpleGeolocation.Provider.TimezoneResolverRequestInterval";
    case SimpleGeolocationProvider::ClientId::kForTesting:
      // This case is unused but required to avoid a compiler warning about
      // missing 'default' case in the switch.
      break;
  }
  NOTREACHED() << "GetClientIdUmaName: An invalid client_id is passed";
}

}  // namespace

SimpleGeolocationProvider::SimpleGeolocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : shared_url_loader_factory_(factory) {}

SimpleGeolocationProvider::~SimpleGeolocationProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
void SimpleGeolocationProvider::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  CHECK_EQ(g_geolocation_provider, nullptr);

  g_geolocation_provider = new SimpleGeolocationProvider(factory);
}

// static
SimpleGeolocationProvider* SimpleGeolocationProvider::GetInstance() {
  CHECK_NE(g_geolocation_provider, nullptr);
  return g_geolocation_provider;
}

GeolocationAccessLevel SimpleGeolocationProvider::GetGeolocationAccessLevel()
    const {
  return geolocation_access_level_;
}

void SimpleGeolocationProvider::SetGeolocationAccessLevel(
    GeolocationAccessLevel geolocation_access_level) {
  bool system_geo_usage_allowed = IsGeolocationUsageAllowedForSystem();
  geolocation_access_level_ = geolocation_access_level;

  if (system_geo_usage_allowed != IsGeolocationUsageAllowedForSystem()) {
    NotifyObservers();
  }
}

void SimpleGeolocationProvider::AddObserver(Observer* obs) {
  CHECK(obs);
  CHECK(!observer_list_.HasObserver(obs));
  observer_list_.AddObserver(obs);
}

void SimpleGeolocationProvider::RemoveObserver(Observer* obs) {
  CHECK(obs);
  CHECK(observer_list_.HasObserver(obs));
  observer_list_.RemoveObserver(obs);
}

void SimpleGeolocationProvider::RequestGeolocation(
    base::TimeDelta timeout,
    bool send_wifi_access_points,
    bool send_cell_towers,
    SimpleGeolocationRequest::ResponseCallback callback,
    ClientId client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordClientIdUma(client_id);

  // Drop request if the system geolocation permission is not granted for
  // system services.
  if (!IsGeolocationUsageAllowedForSystem()) {
    return;
  }

  // System permission is granted:
  auto cell_vector = std::make_unique<CellTowerVector>();
  auto wifi_vector = std::make_unique<WifiAccessPointVector>();

  if (send_wifi_access_points || send_cell_towers) {
    // Mostly necessary for testing and rare cases where NetworkHandler is not
    // initialized: in that case, calls to Get() will fail.
    GeolocationHandler* geolocation_handler = geolocation_handler_;
    if (!geolocation_handler)
      geolocation_handler = NetworkHandler::Get()->geolocation_handler();
    geolocation_handler->GetNetworkInformation(wifi_vector.get(),
                                               cell_vector.get());
  }

  if (!send_wifi_access_points || (wifi_vector->size() == 0))
    wifi_vector = nullptr;

  if (!send_cell_towers || (cell_vector->size() == 0))
    cell_vector = nullptr;

  SimpleGeolocationRequest* request(new SimpleGeolocationRequest(
      shared_url_loader_factory_, GURL(GetGeolocationProviderUrl()), timeout,
      std::move(wifi_vector), std::move(cell_vector)));
  requests_.push_back(base::WrapUnique(request));

  // SimpleGeolocationProvider owns all requests. It is safe to pass unretained
  // "this" because destruction of SimpleGeolocationProvider cancels all
  // requests.
  SimpleGeolocationRequest::ResponseCallback callback_tmp(
      base::BindOnce(&SimpleGeolocationProvider::OnGeolocationResponse,
                     base::Unretained(this), request, std::move(callback)));
  request->MakeRequest(std::move(callback_tmp));
}

// static
void SimpleGeolocationProvider::DestroyForTesting() {
  CHECK_IS_TEST();
  CHECK_NE(g_geolocation_provider, nullptr);
  delete g_geolocation_provider;
  g_geolocation_provider = nullptr;
}

void SimpleGeolocationProvider::SetSharedUrlLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  CHECK_IS_TEST();
  shared_url_loader_factory_ = factory;
}

void SimpleGeolocationProvider::SetGeolocationProviderUrlForTesting(
    const char* url) {
  CHECK_IS_TEST();
  url_for_testing_ = url;
}

bool SimpleGeolocationProvider::IsGeolocationUsageAllowedForSystem() {
  switch (geolocation_access_level_) {
    case GeolocationAccessLevel::kAllowed:
    case GeolocationAccessLevel::kOnlyAllowedForSystem:
      return true;
    case GeolocationAccessLevel::kDisallowed:
      return false;
  }
}

void SimpleGeolocationProvider::OnGeolocationResponse(
    SimpleGeolocationRequest* request,
    SimpleGeolocationRequest::ResponseCallback callback,
    const Geoposition& geoposition,
    bool server_error,
    const base::TimeDelta elapsed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(callback).Run(geoposition, server_error, elapsed);

  std::vector<std::unique_ptr<SimpleGeolocationRequest>>::iterator position =
      base::ranges::find(requests_, request,
                         &std::unique_ptr<SimpleGeolocationRequest>::get);
  DCHECK(position != requests_.end());
  if (position != requests_.end()) {
    std::swap(*position, *requests_.rbegin());
    requests_.resize(requests_.size() - 1);
  }
}

std::string SimpleGeolocationProvider::GetGeolocationProviderUrl() const {
  // URL provider is overridden in tests.
  if (!url_for_testing_.empty()) {
    CHECK_IS_TEST();
    return url_for_testing_;
  }
  return kGeolocationProviderUrl;
}

void SimpleGeolocationProvider::NotifyObservers() {
  for (auto& obs : observer_list_) {
    obs.OnGeolocationPermissionChanged(IsGeolocationUsageAllowedForSystem());
  }
}

void SimpleGeolocationProvider::RecordClientIdUma(ClientId client_id) {
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
