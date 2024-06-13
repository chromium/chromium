// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/destination_set.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::ElementsAre;
using ::testing::Property;

TEST(DestinationSetTest, Parse) {
  EXPECT_THAT(DestinationSet::FromJSON(nullptr),
              ErrorIs(SourceRegistrationError::kDestinationMissing));

  const struct {
    const char* desc;
    const char* json;
    ::testing::Matcher<base::expected<DestinationSet, SourceRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json(null)json",
          ErrorIs(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_wrong_type",
          R"json(0)json",
          ErrorIs(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_untrustworthy",
          R"json("http://d.example")json",
          ErrorIs(SourceRegistrationError::kDestinationUntrustworthy),
      },
      {
          "basic_destination",
          R"json("https://d.example")json",
          ValueIs(Property(&DestinationSet::destinations,
                           ElementsAre(net::SchemefulSite::Deserialize(
                               "https://d.example")))),
      },
      {
          "destination_list_empty",
          R"json([])json",
          ErrorIs(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_in_list_wrong_type",
          R"json([0])json",
          ErrorIs(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_in_list_untrustworthy",
          R"json(["http://d.example"])json",
          ErrorIs(SourceRegistrationError::kDestinationListUntrustworthy),
      },
      {
          "multiple_destinations",
          R"json([
            "https://d.example",
            "https://e.example",
            "https://f.example"
          ])json",
          ValueIs(Property(
              &DestinationSet::destinations,
              ElementsAre(
                  net::SchemefulSite::Deserialize("https://d.example"),
                  net::SchemefulSite::Deserialize("https://e.example"),
                  net::SchemefulSite::Deserialize("https://f.example")))),
      },
      {
          "too_many_destinations",
          R"json([
            "https://d.example",
            "https://e.example",
            "https://f.example",
            "https://g.example"
          ])json",
          ErrorIs(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "size_check_after_transformation_and_deduplication",
          R"json([
            "https://d1.example",
            "https://d2.example",
            "https://d3.example/a",
            "https://d3.example/b"
          ])json",
          ValueIs(Property(
              &DestinationSet::destinations,
              ElementsAre(
                  net::SchemefulSite::Deserialize("https://d1.example"),
                  net::SchemefulSite::Deserialize("https://d2.example"),
                  net::SchemefulSite::Deserialize("https://d3.example")))),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    const base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(DestinationSet::FromJSON(&value), test_case.matches);
  }
}

TEST(DestinationSetTest, ToJson) {
  const struct {
    DestinationSet input;
    const char* expected_json;
  } kTestCases[] = {
      {
          *DestinationSet::Create(
              {net::SchemefulSite::Deserialize("https://d.example")}),
          R"json("https://d.example")json",
      },
      {
          *DestinationSet::Create(
              {net::SchemefulSite::Deserialize("https://d.example"),
               net::SchemefulSite::Deserialize("https://e.example"),
               net::SchemefulSite::Deserialize("https://f.example")}),
          R"json(
            ["https://d.example","https://e.example","https://f.example"]
          )json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
