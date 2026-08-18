// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_TEST_API_H_
#define COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_TEST_API_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "services/device/public/mojom/geoposition.mojom.h"

class GeolocationHeaderServiceTestApi {
 public:
  explicit GeolocationHeaderServiceTestApi(GeolocationHeaderService* service)
      : service_(service) {}

  void SetLocationAge(base::TimeDelta age) {
    service_->location_age_for_testing_ = age;
  }

  void SetLocation(device::mojom::GeopositionPtr position) {
    service_->last_position_ = std::move(position);
  }

  bool is_geolocation_bound() const {
    return service_->geolocation_.is_bound();
  }

 private:
  raw_ptr<GeolocationHeaderService> service_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_GEOLOCATION_HEADER_SERVICE_TEST_API_H_
