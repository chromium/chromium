// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/test_utils.h"

namespace language {

MockGeoLocation::MockGeoLocation() {}
MockGeoLocation::~MockGeoLocation() {}

void MockGeoLocation::SetHighAccuracy(bool high_accuracy) {}

void MockGeoLocation::QueryNextPosition(QueryNextPositionCallback callback) {
  ++query_next_position_called_times_;
  std::move(callback).Run(position_.Clone());
}

void MockGeoLocation::BindGeoLocation(
    mojo::PendingReceiver<device::mojom::Geolocation> receiver) {
  receiver_.Bind(std::move(receiver));
}

void MockGeoLocation::MoveToLocation(float latitude, float longitude) {
  position_.latitude = latitude;
  position_.longitude = longitude;
}

MockIpGeoLocationProvider::MockIpGeoLocationProvider(
    MockGeoLocation* mock_geo_location)
    : mock_geo_location_(mock_geo_location) {}

MockIpGeoLocationProvider::~MockIpGeoLocationProvider() {}

void MockIpGeoLocationProvider::Bind(mojo::ScopedMessagePipeHandle handle) {
  receiver_.Bind(
      mojo::PendingReceiver<device::mojom::PublicIpAddressGeolocationProvider>(
          std::move(handle)));
}

void MockIpGeoLocationProvider::CreateGeolocation(
    const net::MutablePartialNetworkTrafficAnnotationTag& /* unused */,
    mojo::PendingReceiver<device::mojom::Geolocation> receiver) {
  mock_geo_location_->BindGeoLocation(std::move(receiver));
}

}  // namespace language
