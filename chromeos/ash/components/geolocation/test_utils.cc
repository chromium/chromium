// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/test_utils.h"

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "chromeos/ash/components/network/network_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::geolocation::test_utils {
namespace {

// Serializes the Geoposition object into the Geolocation API response
// format:
// {
//   "location": {
//     "lat": <value>
//     "lng": <value>
//   },
//   "accuracy": <value>
// }
std::string GeopositionToApiResponseFormat(const Geoposition& position) {
  // 1. Build the nested "location" dictionary
  base::Value::Dict location_dict;
  location_dict.Set("lat", position.latitude);
  location_dict.Set("lng", position.longitude);

  // 2. Build the root dictionary
  base::Value::Dict root_dict;
  root_dict.Set("location", std::move(location_dict));
  root_dict.Set("accuracy", position.accuracy);

  // 3. Serialize the dictionary to a string
  std::string json_output;
  base::JSONWriter::Write(root_dict, &json_output);

  return json_output;
}

}  // namespace

GeolocationApiInterceptor::GeolocationApiInterceptor(
    network::TestURLLoaderFactory* url_loader,
    const net::HttpStatusCode status_code,
    const std::string& response_body,
    const size_t require_retries)
    : url_loader_(url_loader),
      status_code_(status_code),
      response_body_(response_body),
      require_retries_(require_retries) {}
GeolocationApiInterceptor::~GeolocationApiInterceptor() = default;

void GeolocationApiInterceptor::Intercept(
    const network::ResourceRequest& request) {
  // First `require_retries_` requests should fail.
  if (++attempts_ <= require_retries_) {
    url_loader_->AddResponse(request.url.spec(), "",
                             net::HTTP_INTERNAL_SERVER_ERROR);
    return;
  }

  // If a specific response is configured, use it for the response.
  // Otherwise return the default response.
  auto query = network::GetUploadData(request);
  auto it = responses_.find(query);
  if (it != responses_.end()) {
    url_loader_->AddResponse(request.url.spec(),
                             GeopositionToApiResponseFormat(it->second),
                             net::HTTP_OK);
  } else {
    url_loader_->AddResponse(request.url.spec(), response_body_, status_code_);
  }
}
void GeolocationApiInterceptor::RegisterResponseForQuery(
    std::string query,
    const Geoposition position) {
  responses_.insert_or_assign(std::move(query), position);
}

GeolocationReceiver::GeolocationReceiver() : server_error_(false) {}
GeolocationReceiver::~GeolocationReceiver() = default;

void GeolocationReceiver::OnRequestDone(const Geoposition& position,
                                        bool server_error,
                                        const base::TimeDelta elapsed) {
  position_ = position;
  server_error_ = server_error;
  elapsed_ = elapsed;

  message_loop_runner_.Quit();
}

void GeolocationReceiver::WaitUntilRequestDone() {
  message_loop_runner_.Run();
}

void RequestTestMonitor::OnRequestCreated(SimpleGeolocationRequest* request) {}
void RequestTestMonitor::OnStart(SimpleGeolocationRequest* request) {
  last_request_body_ = request->FormatRequestBodyForTesting();
  last_request_url_ = request->GetServiceURLForTesting();
  ++requests_count_;
}

FakeNetworkDelegate::FakeNetworkDelegate() = default;
FakeNetworkDelegate::~FakeNetworkDelegate() = default;

bool FakeNetworkDelegate::GetNetworkInformation(
    WifiAccessPointVector* access_points,
    CellTowerVector* cell_towers) {
  *access_points = wifi_aps_;
  *cell_towers = cell_towers_;
  return true;
}

bool FakeNetworkDelegate::wifi_enabled() const {
  return true;
}

bool FakeNetworkDelegate::GetWifiAccessPoints(
    WifiAccessPointVector* access_points,
    int64_t* age_ms) {
  *access_points = wifi_aps_;
  *age_ms = 0;
  return true;
}

void FakeNetworkDelegate::AddWifiAP(const WifiAccessPoint wifi_ap) {
  wifi_aps_.emplace_back(std::move(wifi_ap));
}
bool FakeNetworkDelegate::RemoveWifiAP(const WifiAccessPoint wifi_ap) {
  return std::erase(wifi_aps_, wifi_ap);
}

void FakeNetworkDelegate::AddCellTower(const CellTower cell_tower) {
  cell_towers_.emplace_back(std::move(cell_tower));
}
bool FakeNetworkDelegate::RemoveCellTower(const CellTower cell_tower) {
  return std::erase(cell_towers_, cell_tower);
}

}  // namespace ash::geolocation::test_utils
