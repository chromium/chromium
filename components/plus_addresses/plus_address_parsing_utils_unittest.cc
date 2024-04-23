// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parsing_utils.h"

#include <optional>

#include "base/json/json_reader.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

// PlusAddressParsing tests validate the ParsePlusAddressFrom* methods
// Returns empty when the DataDecoder fails to parse the JSON.
TEST(PlusAddressParsing, NotValidJson) {
  EXPECT_EQ(ParsePlusProfileFromV1Create(base::unexpected("error!")),
            std::nullopt);
  EXPECT_EQ(ParsePlusAddressMapFromV1List(base::unexpected("error!")),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_ParsesSuccessfully) {
  const std::string kProfileId = "123";
  const std::string kFacet = "apple.com";
  const std::string kPlusAddress = "fubar@plus.com";

  // Test when the plusMode should set is_confirmed to true.
  std::optional<base::Value> valid_mode =
      base::JSONReader::Read(base::ReplaceStringPlaceholders(
          R"(
    {
      "plusProfile":  {
        "unwanted": 123,
        "ProfileId": "$1",
        "facet": "$2",
        "plusEmail" : {
          "plusAddress": "$3",
          "plusMode": "validMode"
        }
      },
      "unwanted": "abc"
    }
    )",
          {kProfileId, kFacet, kPlusAddress},
          /*offsets=*/nullptr));

  ASSERT_TRUE(valid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(valid_mode.value());

  std::optional<PlusProfile> valid_result =
      ParsePlusProfileFromV1Create(std::move(value));
  ASSERT_TRUE(valid_result.has_value());
  EXPECT_EQ(valid_result->profile_id, kProfileId);
  EXPECT_EQ(absl::get<std::string>(valid_result->facet), kFacet);
  EXPECT_EQ(valid_result->plus_address, kPlusAddress);
  EXPECT_EQ(valid_result->is_confirmed, true);

  // Test when the plusMode should set is_confirmed to false.
  std::optional<base::Value> invalid_mode =
      base::JSONReader::Read(base::ReplaceStringPlaceholders(
          R"(
    {
      "plusProfile":  {
        "unwanted": 123,
        "ProfileId": "$1",
        "facet": "$2",
        "plusEmail" : {
          "plusAddress": "$3",
          "plusMode": "MODE_UNSPECIFIED"
        }
      },
      "unwanted": "abc"
    }
    )",
          {kProfileId, kFacet, kPlusAddress},
          /*offsets=*/nullptr));
  ASSERT_TRUE(invalid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError decoded =
      std::move(invalid_mode.value());

  std::optional<PlusProfile> invalid_result =
      ParsePlusProfileFromV1Create(std::move(decoded));
  ASSERT_TRUE(invalid_result.has_value());
  EXPECT_EQ(invalid_result->profile_id, kProfileId);
  EXPECT_EQ(absl::get<std::string>(invalid_result->facet), kFacet);
  EXPECT_EQ(invalid_result->plus_address, kPlusAddress);
  EXPECT_EQ(invalid_result->is_confirmed, false);
}

// Validate that there is a plusAddress field in the plusEmail object.
TEST(PlusAddressParsing, FromV1Create_FailsWithoutPlusAddress) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile":  {
        "plusEmail" : {
          "plusMode": "validMode"
        }
      }
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

// Validate that there is a plusMode field in the plusEmail object.
TEST(PlusAddressParsing, FromV1Create_FailsWithoutPlusMode) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile":  {
        "plusEmail" : {
          "plusAddress": "plus@plus.plus"
        }
      }
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

// Validate that there is a plusEmail object.
TEST(PlusAddressParsing, FromV1Create_FailsWithoutEmailObject) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile":  {
        "address": "foobar"
      }
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsForEmptyDict) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": {}
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsWithoutPlusProfileKey) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
      {
        "plusAddress": "wouldnt this be nice?"
      }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsIfPlusProfileIsNotDict) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
      {
        "plusProfile": "not a dict"
      }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(ParsePlusProfileFromV1Create(std::move(value)), std::nullopt);
}

// Success case - Returns the plus address map.
TEST(PlusAddressParsing, FromV1List_ParsesSuccessfully) {
  std::optional<base::Value> perfect = base::JSONReader::Read(R"(
    {
      "plusProfiles": [
        {
          "ProfileId": "123",
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
          "ProfileId": "234",
          "facet": "netflix.com",
          "plusEmail" : {
            "plusAddress": "bar@plus.com",
            "plusMode": "validMode"
          }
        }
      ],
      "unwanted": "abc"
    }
    )");
  ASSERT_TRUE(perfect.has_value());

  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(perfect.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"},
                                            {"netflix.com", "bar@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithFacets) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "ProfileId": "123",
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
          "ProfileId": "234",
          "plusEmail" : {
            "plusAddress": "bar@plus.com",
            "plusMode": "validMode"
          }
        }
      ]
    }
    )");
  ASSERT_TRUE(json.has_value());

  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithPlusAddresses) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "ProfileId": "123",
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
          "ProfileId": "234",
          "facet": "netflix.com",
          "plusEmail" : {
            "plusMode": "validMode"
          }
        }
      ]
    }
    )");
  ASSERT_TRUE(json.has_value());

  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithPlusModes) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "ProfileId": "123",
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
          "ProfileId": "234",
          "facet": "netflix.com",
          "plusEmail" : {
            "plusAddress": "bar@plus.com"
          }
        }
      ]
    }
    )");
  ASSERT_TRUE(json.has_value());

  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_ReturnsEmptyMapForEmptyProfileList) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfiles": []
    }
    )");
  ASSERT_TRUE(json.has_value());
  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap());
}

TEST(PlusAddressParsing, FromV1List_FailsIfPlusProfilesIsNotList) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfiles": 123
    }
    )");
  ASSERT_TRUE(json.has_value());
  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  EXPECT_FALSE(result.has_value());
}

TEST(PlusAddressParsing, FromV1List_FailsIfMissingPlusProfilesKey) {
  // Note the slight difference in syntax ("plusProfiles" vs "plusProfile").
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": [],
      "otherKey": 123
    }
    )");
  ASSERT_TRUE(json.has_value());
  std::optional<PlusAddressMap> result =
      ParsePlusAddressMapFromV1List(std::move(json.value()));
  EXPECT_FALSE(result.has_value());
}

}  // namespace plus_addresses
