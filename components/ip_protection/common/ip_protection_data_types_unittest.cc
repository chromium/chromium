// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_data_types.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace ip_protection {

namespace {

// Size of a PRT when TLS serialized, before base64 encoding.
constexpr size_t kPRTSize = 79;
constexpr size_t kPRTPointSize = 33;
constexpr size_t kEpochIdSize = 8;

// Deserialize a given prt serialized using
// `ProbabilisticRevealToken::SerializeAndEncode()`.
bool Deserialize(const std::string& serialized_encoded_prt,
                 ProbabilisticRevealToken& out) {
  std::string serialized_prt;
  if (!base::Base64Decode(serialized_encoded_prt, &serialized_prt)) {
    return false;
  }
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(serialized_prt.data()),
           serialized_prt.size());
  if (CBS_len(&cbs) != kPRTSize) {
    return false;
  }
  uint8_t version;
  uint16_t u_size;
  uint16_t e_size;
  std::string u(kPRTPointSize, '0');
  std::string e(kPRTPointSize, '0');
  std::string epoch_id(kEpochIdSize, '0');
  if (!CBS_get_u8(&cbs, &version) || !CBS_get_u16(&cbs, &u_size) ||
      u_size != kPRTPointSize ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(u.data()), u_size) ||
      !CBS_get_u16(&cbs, &e_size) || e_size != kPRTPointSize ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(e.data()), e_size) ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(epoch_id.data()),
                      kEpochIdSize)) {
    return false;
  }
  out.version = version;
  out.u = std::move(u);
  out.e = std::move(e);
  out.epoch_id = std::move(epoch_id);
  return true;
}

}  // namespace

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

class IpProtectionPRTSerializeTest : public testing::Test {};

TEST_F(IpProtectionPRTSerializeTest, SerializeEmptyPRT) {
  ProbabilisticRevealToken token{};
  std::optional<std::string> res = token.SerializeAndEncode();
  EXPECT_FALSE(res.has_value());
}

TEST_F(IpProtectionPRTSerializeTest, WrongVersion) {
  ProbabilisticRevealToken token{2, std::string(kPRTPointSize, 'u'),
                                 std::string(kPRTPointSize, 'e'),
                                 std::string(8, '0')};
  std::optional<std::string> res = token.SerializeAndEncode();
  EXPECT_FALSE(res.has_value());
}

TEST_F(IpProtectionPRTSerializeTest, WrongUSize) {
  ProbabilisticRevealToken token{1, std::string(kPRTPointSize + 1, 'u'),
                                 std::string(kPRTPointSize, 'e'),
                                 std::string(8, '0')};
  std::optional<std::string> res = token.SerializeAndEncode();
  EXPECT_FALSE(res.has_value());
}

TEST_F(IpProtectionPRTSerializeTest, WrongESize) {
  ProbabilisticRevealToken token{1, std::string(kPRTPointSize, 'u'),
                                 std::string(kPRTPointSize - 1, 'e'),
                                 std::string(8, '0')};
  std::optional<std::string> res = token.SerializeAndEncode();
  EXPECT_FALSE(res.has_value());
}

TEST_F(IpProtectionPRTSerializeTest, WrongEpochIdSize) {
  ProbabilisticRevealToken token{1, std::string(kPRTPointSize, 'u'),
                                 std::string(kPRTPointSize, 'e'),
                                 std::string(9, '0')};
  std::optional<std::string> res = token.SerializeAndEncode();
  EXPECT_FALSE(res.has_value());
}

TEST_F(IpProtectionPRTSerializeTest, Success) {
  ProbabilisticRevealToken expected_token{1, std::string(kPRTPointSize, 'u'),
                                          std::string(kPRTPointSize, 'e'),
                                          std::string(8, '0')};
  std::optional<std::string> res = expected_token.SerializeAndEncode();
  ASSERT_TRUE(res.has_value());
  ProbabilisticRevealToken token;
  ASSERT_TRUE(Deserialize(res.value(), token));
  EXPECT_EQ(token, expected_token);
}

}  // namespace ip_protection
