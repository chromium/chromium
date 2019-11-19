// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/geolocation/simple_geolocation_provider.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/geolocation/geoposition.h"
#include "chromeos/network/geolocation_handler.h"
#include "chromeos/network/network_handler.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

const char kDefaultGeolocationProviderUrl[] =
    "https://www.googleapis.com/geolocation/v1/geolocate?";

}  // namespace

SimpleGeolocationProvider::SimpleGeolocationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const GURL& url)
    : shared_url_loader_factory_(std::move(factory)), url_(url) {}

SimpleGeolocationProvider::~SimpleGeolocationProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SimpleGeolocationProvider::RequestGeolocation(
    base::TimeDelta timeout,
    bool send_wifi_access_points,
    bool send_cell_towers,
    SimpleGeolocationRequest::ResponseCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto cell_vector = std::make_unique<chromeos::CellTowerVector>();
  auto wifi_vector = std::make_unique<chromeos::WifiAccessPointVector>();

  // Mostly necessary for testing and rare cases where NetworkHandler is not
  // initialized: in that case, calls to Get() will fail.
  if (send_wifi_access_points || send_cell_towers) {
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
  DCHECK(thread_checker_.CalledOnValidThread());

  std::move(callback).Run(geoposition, server_error, elapsed);

  std::vector<std::unique_ptr<SimpleGeolocationRequest>>::iterator position =
      std::find_if(
          requests_.begin(), requests_.end(),
          [request](const std::unique_ptr<SimpleGeolocationRequest>& req) {
            return req.get() == request;
          });
  DCHECK(position != requests_.end());
  if (position != requests_.end()) {
    std::swap(*position, *requests_.rbegin());
    requests_.resize(requests_.size() - 1);
  }
}

}  // namespace chromeos
