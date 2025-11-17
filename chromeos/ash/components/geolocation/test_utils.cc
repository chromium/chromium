// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/test_utils.h"

#include "base/run_loop.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/system_location_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::geolocation::test_utils {

// This implements fake Google MAPS Geolocation API remote endpoint.
TestGeolocationAPILoaderFactory::TestGeolocationAPILoaderFactory(
    const GURL& url,
    const net::HttpStatusCode http_status,
    const std::string& response,
    const size_t require_retries)
    : url_(url),
      http_status_(http_status),
      response_(response),
      require_retries_(require_retries) {
  SetInterceptor(base::BindRepeating(
      &TestGeolocationAPILoaderFactory::Intercept, base::Unretained(this)));
  AddResponse(url_.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);
}

TestGeolocationAPILoaderFactory::~TestGeolocationAPILoaderFactory() = default;

void TestGeolocationAPILoaderFactory::Configure(
    const GURL& url,
    const net::HttpStatusCode http_status,
    const std::string& response,
    const size_t require_retries) {
  url_ = url;
  http_status_ = http_status;
  response_ = response;
  require_retries_ = require_retries;
}

void TestGeolocationAPILoaderFactory::Intercept(
    const network::ResourceRequest& request) {
  // Drop the query component potentially appended by the
  // `SimpleGeolocationRequest` class.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  EXPECT_EQ(url_.ReplaceComponents(replacements),
            request.url.ReplaceComponents(replacements));

  if (++attempts_ > require_retries_) {
    AddResponse(url_.spec(), response_, http_status_);
  }
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

void FakeNetworkDelegate::AddWifiAP() {
  WifiAccessPoint wifi_ap;
  wifi_ap.channel = 1;
  wifi_ap.mac_address = "01:00:00:00:00:00";
  wifi_ap.signal_strength = 10;
  wifi_ap.signal_to_noise = 0;

  wifi_aps_.emplace_back(std::move(wifi_ap));
}

void FakeNetworkDelegate::AddCellTower() {
  CellTower cell_tower;
  cell_tower.ci = base::NumberToString(1);
  cell_tower.lac = base::NumberToString(3);
  cell_tower.mcc = base::NumberToString(100);
  cell_tower.mnc = base::NumberToString(101);

  cell_towers_.emplace_back(std::move(cell_tower));
}

}  // namespace ash::geolocation::test_utils
