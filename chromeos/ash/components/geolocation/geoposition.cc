// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/geoposition.h"

#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
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
  static constexpr auto kStatusStrings = std::to_array<const char*>(
      {"NONE", "OK", "SERVER_ERROR", "NETWORK_ERROR", "TIMEOUT"});

  // Construct a span from the fixed-size array.
  base::span<const char* const> status_span(kStatusStrings);

  const char* status_str = "unknown";
  // Ensure the enum value is within the valid range of the span.
  if (status >= 0 && static_cast<size_t>(status) < status_span.size()) {
    // SAFETY: The if-condition guarantees that the index is within bounds.
    status_str = status_span[status];
  }

  return base::StringPrintf(
      "latitude=%f, longitude=%f, accuracy=%f, error_code=%u, "
      "error_message='%s', status=%u (%s)",
      latitude, longitude, accuracy, error_code, error_message.c_str(),
      static_cast<unsigned>(status), status_str);
}

}  // namespace ash
