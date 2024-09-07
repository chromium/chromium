// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parsing_utils.h"

#include <optional>

#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

using ::testing::ElementsAre;
using ::testing::Optional;

// PlusAddressParsing tests validate the ParsePlusAddressFrom* methods
// Returns empty when the DataDecoder fails to parse the JSON.
TEST(PlusAddressParsing, NotValidJson) {
  EXPECT_EQ(ParsePlusProfileFromV1Create(base::unexpected("error!")),
            std::nullopt);
}

TEST(PlusAddressParsing, FromV1Create_ParsesSuccessfully) {
  const std::string kProfileId = "123";
  const affiliations::FacetURI kFacet =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          "https://www.apple.com");
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
          {kProfileId, kFacet.canonical_spec(), kPlusAddress},
          /*offsets=*/nullptr));

  ASSERT_TRUE(valid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError value = std::move(valid_mode.value());

  std::optional<PlusProfile> valid_result =
      ParsePlusProfileFromV1Create(std::move(value));
  ASSERT_TRUE(valid_result.has_value());
  EXPECT_EQ(valid_result->profile_id, kProfileId);
  EXPECT_EQ(valid_result->facet, kFacet);
  EXPECT_EQ(valid_result->plus_address, PlusAddress(kPlusAddress));
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
          {kProfileId, kFacet.canonical_spec(), kPlusAddress},
          /*offsets=*/nullptr));
  ASSERT_TRUE(invalid_mode.has_value());
  data_decoder::DataDecoder::ValueOrError decoded =
      std::move(invalid_mode.value());

  std::optional<PlusProfile> invalid_result =
      ParsePlusProfileFromV1Create(std::move(decoded));
  ASSERT_TRUE(invalid_result.has_value());
  EXPECT_EQ(invalid_result->profile_id, kProfileId);
  EXPECT_EQ(invalid_result->facet, kFacet);
  EXPECT_EQ(invalid_result->plus_address, PlusAddress(kPlusAddress));
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

TEST(PlusAddressParsing, ParsePreallocatedPlusAddresses) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
    "emailAddresses": [
      {
        "emailAddress": "foo@foo.com",
        "reservationLifetime": "123s"
      },
      {
        "emailAddress": "foo@bar.com",
        "reservationLifetime": "15552000s"
      }
    ]
  }
  )");
  ASSERT_TRUE(json);

  std::optional<std::vector<PreallocatedPlusAddress>> addresses =
      ParsePreallocatedPlusAddresses(std::move(*json));
  EXPECT_THAT(
      addresses,
      Optional(ElementsAre(
          PreallocatedPlusAddress(PlusAddress("foo@foo.com"),
                                  /*lifetime=*/base::Seconds(123)),
          PreallocatedPlusAddress(PlusAddress("foo@bar.com"),
                                  /*lifetime=*/base::Seconds(15552000)))));
}

TEST(PlusAddressParsing, ParsePreallocatedPlusAddressesWithInvalidJSON) {
  EXPECT_EQ(
      ParsePreallocatedPlusAddresses(base::unexpected("An error occurred")),
      std::nullopt);
}

// Tests that `ParsePreallocatedPlusAddresses` ignores malformed entries.
TEST(PlusAddressParsing, ParsePreallocatedPlusAddressesWithMalformedEntries) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
    "emailAddresses": [
      {
        "emailAddress": "foo@foo.com",
        "reservationLifetime": "123s"
      },
      {
        "emailAddress1": "foo@bar.com",
        "reservationLifetime": "15552000s"
      },
      {
        "emailAddress": "foo@bar.com",
        "reservationLifetime1": "15552000s"
      },
      {
        "emailAddress": "foo@bar.com",
        "reservationLifetime": "15552000"
      },
      {
        "emailAddress": "foo@bar.com",
        "reservationLifetime": ""
      },
      {
        "emailAddress": "foo@bar.com",
        "reservationLifetime": "as"
      },
      "asd",
      {
        "emailAddress": "foo@goo.com",
        "reservationLifetime": "312s"
      }
    ]
  }
  )");
  ASSERT_TRUE(json);

  std::optional<std::vector<PreallocatedPlusAddress>> addresses =
      ParsePreallocatedPlusAddresses(std::move(*json));
  EXPECT_THAT(addresses,
              Optional(ElementsAre(
                  PreallocatedPlusAddress(PlusAddress("foo@foo.com"),
                                          /*lifetime=*/base::Seconds(123)),
                  PreallocatedPlusAddress(PlusAddress("foo@goo.com"),
                                          /*lifetime=*/base::Seconds(312)))));
}

// Tests that `ParsePreallocatedPlusAddresses` returns `std::nullopt` if the
// top-level entry does not have the expected format.
TEST(PlusAddressParsing,
     ParsePreallocatedPlusAddressesWithMalformedTopLevelEntry) {
  std::optional<base::Value> json = base::JSONReader::Read(R"(
  {
    "Addresses": [
      {
        "emailAddress": "foo@foo.com",
        "reservationLifetime": "123s"
      }
    ],
    "emailAddresses": "asd"
  }
  )");
  ASSERT_TRUE(json);

  std::optional<std::vector<PreallocatedPlusAddress>> addresses =
      ParsePreallocatedPlusAddresses(std::move(*json));
  EXPECT_THAT(addresses, std::nullopt);
}

}  // namespace plus_addresses
