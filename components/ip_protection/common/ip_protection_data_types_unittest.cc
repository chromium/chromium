// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_data_types.h"

#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

class IpProtectionGeoUtilsTest : public testing::Test {};

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_ValidInput) {
  GeoHint geo_hint = {.country_code = "US",
                      .iso_region = "US-CA",
                      .city_name = "MOUNTAIN VIEW"};

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));

  EXPECT_EQ(geo_id, "US,US-CA,MOUNTAIN VIEW");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_CountryCodeOnly) {
  GeoHint geo_hint = {
      .country_code = "US",
  };

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));

  EXPECT_EQ(geo_id, "US");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_EmptyGeoHintPtr) {
  std::optional<GeoHint> geo_hint;

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));

  EXPECT_EQ(geo_id, "");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_NullOptGeoHint) {
  std::string geo_id = GetGeoIdFromGeoHint(std::nullopt);

  EXPECT_EQ(geo_id, "");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoHintFromGeoIdForTesting_CompleteGeoId) {
  std::optional<GeoHint> geo_hint =
      GetGeoHintFromGeoIdForTesting("US,US-CA,MOUNTAIN VIEW");

  GeoHint expected_geo_hint = {.country_code = "US",
                               .iso_region = "US-CA",
                               .city_name = "MOUNTAIN VIEW"};

  EXPECT_TRUE(geo_hint == expected_geo_hint);
}

TEST_F(IpProtectionGeoUtilsTest,
       GetGeoHintFromGeoIdForTesting_CountryOnlyGeoId) {
  std::optional<GeoHint> geo_hint = GetGeoHintFromGeoIdForTesting("US");
  GeoHint expected_geo_hint = {.country_code = "US"};

  EXPECT_TRUE(geo_hint == expected_geo_hint);
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoHintFromGeoIdForTesting_EmptyGeoId) {
  std::optional<GeoHint> geo_hint = GetGeoHintFromGeoIdForTesting("");

  EXPECT_TRUE(!geo_hint.has_value());
}

}  // namespace ip_protection
