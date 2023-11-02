// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"

namespace ash {

// This structure represents Google Maps Geolocation response.
// Based on device/geolocation/geoposition.h .
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) Geoposition {
  // Geolocation API client status.
  // (Server status is reported in "error_code" field.)
  enum Status {
    STATUS_NONE,
    STATUS_OK,             // Response successful.
    STATUS_SERVER_ERROR,   // Received error object.
    STATUS_NETWORK_ERROR,  // Received bad or no response.
    STATUS_TIMEOUT,        // Request stopped because of timeout.
    STATUS_LAST = STATUS_TIMEOUT
  };

  // All fields are initialized to sentinel values marking them as invalid. The
  // status is set to STATUS_NONE.
  Geoposition();

  // A valid fix has a valid latitude, longitude, accuracy and timestamp.
  bool Valid() const;

  // Serialize to string.
  std::string ToString() const;

  // Latitude in decimal degrees north.
  double latitude;

  // Longitude in decimal degrees west.
  double longitude;

  // Accuracy of horizontal position in meters.
  double accuracy;

  // Error object data:
  // Value of "error.code".
  int error_code;

  // Human-readable error message.
  std::string error_message;

  // Absolute time, when this position was acquired. This is
  // taken from the host computer's system clock (i.e. from Time::Now(), not the
  // source device's clock).
  base::Time timestamp;

  // See enum above.
  Status status;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_GEOPOSITION_H_
