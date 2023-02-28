// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/destination_set.h"

#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "net/base/schemeful_site.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

TEST(DestinationSetTest, Parse) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});
  const DestinationSet destinations = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example"),
       net::SchemefulSite::Deserialize("https://e.example"),
       net::SchemefulSite::Deserialize("https://f.example")});

  const struct {
    const char* desc;
    base::Value json;
    base::expected<DestinationSet, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "empty",
          base::Value(),
          base::unexpected(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_wrong_type",
          base::test::ParseJson(R"json(0)json"),
          base::unexpected(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_untrustworthy",
          base::test::ParseJson(R"json("http://d.example")json"),
          base::unexpected(SourceRegistrationError::kDestinationUntrustworthy),
      },
      {
          "basic_destination",
          base::test::ParseJson(R"json("https://d.example")json"),
          destination,
      },
      {
          "destination_list_empty",
          base::test::ParseJson(R"json([])json"),
          base::unexpected(SourceRegistrationError::kDestinationMissing),
      },
      {
          "destination_in_list_wrong_type",
          base::test::ParseJson(R"json([0])json"),
          base::unexpected(SourceRegistrationError::kDestinationWrongType),
      },
      {
          "destination_in_list_untrustworthy",
          base::test::ParseJson(R"json(["http://d.example"])json"),
          base::unexpected(SourceRegistrationError::kDestinationUntrustworthy),
      },
      {
          "multiple_destinations",
          base::test::ParseJson(
              R"json(["https://d.example","https://e.example","https://f.example"])json"),
          destinations,
      },
      {
          "too_many_destinations",
          base::test::ParseJson(
              R"json(["https://d.example","https://e.example","https://f.example","https://g.example"])json"),
          base::unexpected(SourceRegistrationError::kDestinationListTooLong),
      },
  };

  for (const auto& test_case : kTestCases) {
    auto destination_set = DestinationSet::FromJSON(&test_case.json);
    EXPECT_EQ(test_case.expected, destination_set) << test_case.desc;
  }
}

TEST(SourceRegistrationTest, ToJson) {
  const DestinationSet destination = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example")});
  const DestinationSet destinations = *DestinationSet::Create(
      {net::SchemefulSite::Deserialize("https://d.example"),
       net::SchemefulSite::Deserialize("https://e.example"),
       net::SchemefulSite::Deserialize("https://f.example")});

  const struct {
    DestinationSet input;
    const char* expected_json;
  } kTestCases[] = {
      {
          destination,
          R"json("https://d.example")json",
      },
      {
          destinations,
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