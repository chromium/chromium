// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/test_utils.h"

#include "base/time/time.h"

namespace language {

MockGeoLocation::MockGeoLocation() = default;
MockGeoLocation::~MockGeoLocation() = default;

void MockGeoLocation::SetHighAccuracy(bool high_accuracy) {}

void MockGeoLocation::QueryNextPosition(QueryNextPositionCallback callback) {
  ++query_next_position_called_times_;
  std::move(callback).Run(result_.Clone());
}

void MockGeoLocation::BindGeoLocation(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockGeoLocation::MoveToLocation(float latitude, float longitude) {
  auto position = device::mojom::Geoposition::New();
  position->latitude = latitude;
  position->longitude = longitude;
  position->accuracy = 100;
  position->timestamp = base::Time::Now();
  result_ = device::mojom::GeopositionResult::NewPosition(std::move(position));
}

MockIpGeoLocationProvider::MockIpGeoLocationProvider(
    MockGeoLocation* mock_geo_location)
    : mock_geo_location_(mock_geo_location) {}

MockIpGeoLocationProvider::~MockIpGeoLocationProvider() = default;

void MockIpGeoLocationProvider::Bind(
    mojo::PendingReceiver<device::mojom::PublicIpAddressGeolocationProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockIpGeoLocationProvider::CreateGeolocation(
    const net::MutablePartialNetworkTrafficAnnotationTag& /* unused */,
    mojo::PendingReceiver<device::mojom::Geolocation> receiver,
    device::mojom::GeolocationClientId client_id) {
  mock_geo_location_->BindGeoLocation(std::move(receiver));
}

}  // namespace language
