// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parser.h"

#include <optional>

#include "base/json/json_reader.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

// PlusAddressParsing tests validate the ParsePlusAddressFrom* methods
// Returns empty when the DataDecoder fails to parse the JSON.
TEST(PlusAddressParsing, NotValidJson) {
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(
                base::unexpected("error!")),
            std::nullopt);
  EXPECT_EQ(PlusAddressParser::ParsePlusAddressMapFromV1List(
                base::unexpected("error!")),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_ParsesSuccessfully) {
  std::string facet = "apple.com";
  std::string plus_address = "fubar@plus.com";

  // Test when the plusMode should set is_confirmed to true.
  std::optional<base::Value> valid_mode =
      base::JSONReader::Read(base::ReplaceStringPlaceholders(
          R"(
    {
      "plusProfile":  {
        "unwanted": 123,
        "facet": "$1",
        "plusEmail" : {
          "plusAddress": "$2",
          "plusMode": "validMode"
        }
      },
      "unwanted": "abc"
    }
    )",
          {facet, plus_address}, /*offsets=*/nullptr));

  ASSERT_TRUE(valid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(valid_mode.value());

  std::optional<PlusProfile> valid_result =
      PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value));
  ASSERT_TRUE(valid_result.has_value());
  EXPECT_EQ(valid_result->facet, facet);
  EXPECT_EQ(valid_result->plus_address, plus_address);
  EXPECT_EQ(valid_result->is_confirmed, true);

  // Test when the plusMode should set is_confirmed to false.
  std::optional<base::Value> invalid_mode =
      base::JSONReader::Read(base::ReplaceStringPlaceholders(
          R"(
    {
      "plusProfile":  {
        "unwanted": 123,
        "facet": "$1",
        "plusEmail" : {
          "plusAddress": "$2",
          "plusMode": "MODE_UNSPECIFIED"
        }
      },
      "unwanted": "abc"
    }
    )",
          {facet, plus_address}, /*offsets=*/nullptr));
  ASSERT_TRUE(invalid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError decoded =
      std::move(invalid_mode.value());

  std::optional<PlusProfile> invalid_result =
      PlusAddressParser::ParsePlusProfileFromV1Create(std::move(decoded));
  ASSERT_TRUE(invalid_result.has_value());
  EXPECT_EQ(invalid_result->facet, facet);
  EXPECT_EQ(invalid_result->plus_address, plus_address);
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
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
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
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
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
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsForEmptyDict) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
    {
      "plusProfile": {}
    }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsWithoutPlusProfileKey) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
      {
        "plusAddress": "wouldnt this be nice?"
      }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_FailsIfPlusProfileIsNotDict) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
      {
        "plusProfile": "not a dict"
      }
    )");
  ASSERT_TRUE(json.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(json.value());
  EXPECT_EQ(PlusAddressParser::ParsePlusProfileFromV1Create(std::move(value)),
            std::nullopt);
}

// Success case - Returns the plus address map.
TEST(PlusAddressParsing, FromV1List_ParsesSuccessfully) {
  std::optional<base::Value> perfect = base::JSONReader::Read(R"(
    {
      "plusProfiles": [
        {
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(
          std::move(perfect.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"},
                                            {"netflix.com", "bar@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithFacets) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithPlusAddresses) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), PlusAddressMap({{"google.com", "foo@plus.com"}}));
}

TEST(PlusAddressParsing, FromV1List_OnlyParsesProfilesWithPlusModes) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
      "plusProfiles": [
        {
          "facet": "google.com",
          "plusEmail" : {
            "plusAddress": "foo@plus.com",
            "plusMode": "validMode"
          }
        },
        {
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
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
      PlusAddressParser::ParsePlusAddressMapFromV1List(std::move(json.value()));
  EXPECT_FALSE(result.has_value());
}

}  // namespace plus_addresses
