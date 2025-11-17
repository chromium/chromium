// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/location_fetcher.h"

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
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

LocationFetcher::LocationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : LocationFetcher(url_loader_factory,
                      GURL(kDefaultGeolocationProviderUrl),
                      nullptr) {}

LocationFetcher::LocationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& location_service_url,
    GeolocationHandler* geolocation_handler)
    : shared_url_loader_factory_(url_loader_factory),
      location_service_url_(location_service_url),
      geolocation_handler_(geolocation_handler) {}

LocationFetcher::~LocationFetcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void LocationFetcher::GetNetworkInformation(
    WifiAccessPointVector* wifi_vector,
    CellTowerVector* cell_vector) const {
  // Mostly necessary for testing and rare cases where NetworkHandler is not
  // initialized: in that case, calls to Get() will fail.
  GeolocationHandler* geolocation_handler =
      geolocation_handler_ ? geolocation_handler_.get()
                           : NetworkHandler::Get()->geolocation_handler();
  geolocation_handler->GetNetworkInformation(wifi_vector, cell_vector);
}

void LocationFetcher::RequestGeolocation(
    base::TimeDelta timeout,
    bool use_wifi_scan,
    bool use_cellular_scan,
    LocationProvider::ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto cell_vector = std::make_unique<CellTowerVector>();
  auto wifi_vector = std::make_unique<WifiAccessPointVector>();

  if (use_wifi_scan || use_cellular_scan) {
    GetNetworkInformation(wifi_vector.get(), cell_vector.get());
  }

  if (!use_wifi_scan || (wifi_vector->size() == 0)) {
    wifi_vector = nullptr;
  }

  if (!use_cellular_scan || (cell_vector->size() == 0)) {
    cell_vector = nullptr;
  }

  SimpleGeolocationRequest* request(new SimpleGeolocationRequest(
      shared_url_loader_factory_, location_service_url_, timeout,
      std::move(wifi_vector), std::move(cell_vector)));
  requests_.push_back(base::WrapUnique(request));

  // LocationFetcher owns all requests. It is safe to pass unretained
  // "this" because destruction of LocationFetcher cancels all
  // requests.
  LocationProvider::ResponseCallback callback_tmp(
      base::BindOnce(&LocationFetcher::OnGeolocationResponse,
                     base::Unretained(this), request, std::move(callback)));
  request->MakeRequest(std::move(callback_tmp));
}

network::SharedURLLoaderFactory*
LocationFetcher::GetSharedURLLoaderFactoryForTesting() {
  CHECK_IS_TEST();
  return shared_url_loader_factory_.get();
}
void LocationFetcher::SetSharedUrlLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  CHECK_IS_TEST();
  shared_url_loader_factory_ = factory;
}

//------------------------------------------------------------------------------
// Private methods

void LocationFetcher::OnGeolocationResponse(
    SimpleGeolocationRequest* request,
    LocationProvider::ResponseCallback callback,
    const Geoposition& geoposition,
    bool server_error,
    const base::TimeDelta elapsed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::move(callback).Run(geoposition, server_error, elapsed);

  std::vector<std::unique_ptr<SimpleGeolocationRequest>>::iterator position =
      std::ranges::find(requests_, request,
                        &std::unique_ptr<SimpleGeolocationRequest>::get);
  DCHECK(position != requests_.end());
  if (position != requests_.end()) {
    std::swap(*position, *requests_.rbegin());
    requests_.resize(requests_.size() - 1);
  }
}

}  // namespace ash
