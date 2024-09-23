// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/geolocation/geoposition.h"

#include "base/strings/stringprintf.h"

namespace {

// Sentinel values to mark invalid data.
const double kBadLatitudeLongitude = 200;
const int kBadAccuracy = -1;  // Accuracy must be non-negative.

}  // namespace

namespace ash {

Geoposition::Geoposition()
    : latitude(kBadLatitudeLongitude),
      longitude(kBadLatitudeLongitude),
      accuracy(kBadAccuracy),
      error_code(0),
      status(STATUS_NONE) {}

bool Geoposition::Valid() const {
  return latitude >= -90. && latitude <= 90. && longitude >= -180. &&
         longitude <= 180. && accuracy >= 0. && !timestamp.is_null() &&
         status == STATUS_OK;
}

std::string Geoposition::ToString() const {
  static const char* const status2string[] = {"NONE", "OK", "SERVER_ERROR",
                                              "NETWORK_ERROR", "TIMEOUT"};

  return base::StringPrintf(
      "latitude=%f, longitude=%f, accuracy=%f, error_code=%u, "
      "error_message='%s', status=%u (%s)",
      latitude, longitude, accuracy, error_code, error_message.c_str(),
      (unsigned)status,
      (status < std::size(status2string) ? status2string[status] : "unknown"));
}

}  // namespace ash
