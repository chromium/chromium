// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_TEST_UTILS_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/geolocation.mojom.h"
#include "services/device/public/mojom/geolocation_client_id.mojom.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/device/public/mojom/public_ip_address_geolocation_provider.mojom.h"

namespace language {

// Mock impl of mojom::Geolocation that allows tests to control the returned
// location.
class MockGeoLocation : public device::mojom::Geolocation {
 public:
  MockGeoLocation();
  ~MockGeoLocation() override;

  // device::mojom::Geolocation implementation:
  void SetHighAccuracy(bool high_accuracy) override;
  void QueryNextPosition(QueryNextPositionCallback callback) override;

  void BindGeoLocation(
      mojo::PendingReceiver<device::mojom::Geolocation> receiver);
  void MoveToLocation(float latitude, float longitude);

  int query_next_position_called_times() const {
    return query_next_position_called_times_;
  }

 private:
  int query_next_position_called_times_ = 0;
  device::mojom::GeopositionResultPtr result_;
  mojo::Receiver<device::mojom::Geolocation> receiver_{this};
};

// Mock impl of mojom::PublicIpAddressGeolocationProvider that binds Geolocation
// to testing impl.
class MockIpGeoLocationProvider
    : public device::mojom::PublicIpAddressGeolocationProvider {
 public:
  explicit MockIpGeoLocationProvider(MockGeoLocation* mock_geo_location);
  ~MockIpGeoLocationProvider() override;

  void Bind(
      mojo::PendingReceiver<device::mojom::PublicIpAddressGeolocationProvider>);

  void CreateGeolocation(
      const net::MutablePartialNetworkTrafficAnnotationTag& /* unused */,
      mojo::PendingReceiver<device::mojom::Geolocation> receiver,
      device::mojom::GeolocationClientId client_id) override;

 private:
  raw_ptr<MockGeoLocation> mock_geo_location_;
  mojo::Receiver<device::mojom::PublicIpAddressGeolocationProvider> receiver_{
      this};
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_TEST_UTILS_H_
