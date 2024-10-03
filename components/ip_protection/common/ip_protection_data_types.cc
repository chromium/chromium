// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_data_types.h"

#include <memory>
#include <optional>
#include <string>

namespace ip_protection {

std::string GetGeoIdFromGeoHint(const std::optional<GeoHint> geo_hint) {
  if (!geo_hint.has_value()) {
    return "";  // If nullopt, return empty string.
  }

  std::string geo_id = geo_hint->country_code;
  if (!geo_hint->iso_region.empty()) {
    geo_id += "," + geo_hint->iso_region;
  }
  if (!geo_hint->city_name.empty()) {
    geo_id += "," + geo_hint->city_name;
  }

  return geo_id;
}

// TODO(crbug.com/40176497): IN-TEST does not work for multi-line declarations.
std::optional<GeoHint> GetGeoHintFromGeoIdForTesting(  // IN-TEST
    const std::string& geo_id) {
  if (geo_id.empty()) {
    return std::nullopt;  // Return nullopt if the geo_id is empty.
  }
  GeoHint geo_hint;
  std::stringstream geo_id_stream(geo_id);
  std::string segment;

  // Extract country code.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.country_code = segment;
  }

  // Extract ISO region.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.iso_region = segment;
  }

  // Extract city name.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint.city_name = segment;
  }

  return geo_hint;
}

}  // namespace ip_protection
