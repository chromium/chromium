// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"

#include <iterator>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

const char kDefaultGeolocationProviderUrl[] =
    "https://www.googleapis.com/geolocation/v1/geolocate?";

}  // namespace

SimpleGeolocationProvider::SimpleGeolocationProvider(
    const Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& url)
    : delegate_(delegate),
      shared_url_loader_factory_(std::move(factory)),
      url_(url) {}

SimpleGeolocationProvider::~SimpleGeolocationProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SimpleGeolocationProvider::RequestGeolocation(
    base::TimeDelta timeout,
    bool send_wifi_access_points,
    bool send_cell_towers,
    SimpleGeolocationRequest::ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto cell_vector = std::make_unique<CellTowerVector>();
  auto wifi_vector = std::make_unique<WifiAccessPointVector>();

  // Geolocation permission is required to access Wi-Fi and cellular signals.
  if ((send_wifi_access_points || send_cell_towers) &&
      delegate_->IsPreciseGeolocationAllowed()) {
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
      shared_url_loader_factory_, url_, timeout, std::move(wifi_vector),
      std::move(cell_vector)));
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
GURL SimpleGeolocationProvider::DefaultGeolocationProviderURL() {
  return GURL(kDefaultGeolocationProviderUrl);
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

}  // namespace ash
