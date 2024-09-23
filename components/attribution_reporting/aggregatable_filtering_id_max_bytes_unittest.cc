// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"

#include <stdint.h>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::Property;

TEST(AggregatableFilteringIdsMaxBytes, Parse) {
  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<base::expected<AggregatableFilteringIdsMaxBytes,
                                      TriggerRegistrationError>>
        matches;
  } kTestCases[]{
      {
          "empty",
          R"json({})json",
          ValueIs(Property(&AggregatableFilteringIdsMaxBytes::value, 1u)),
      },
      {
          "wrong-type",
          R"json({"aggregatable_filtering_id_max_bytes":"3"})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "too-big-value",
          R"json({"aggregatable_filtering_id_max_bytes":9})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "too-small-value",
          R"json({"aggregatable_filtering_id_max_bytes":0})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "negative-value",
          R"json({"aggregatable_filtering_id_max_bytes":-1})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "non-integer-value",
          R"json({"aggregatable_filtering_id_max_bytes":0.5})json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableFilteringIdMaxBytesInvalidValue),
      },
      {
          "valid",
          R"json({"aggregatable_filtering_id_max_bytes":3})json",
          ValueIs(Property(&AggregatableFilteringIdsMaxBytes::value, 3u)),
      },
      {
          "valid_trailing_zero",
          R"json({"aggregatable_filtering_id_max_bytes":3.0})json",
          ValueIs(Property(&AggregatableFilteringIdsMaxBytes::value, 3u)),
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    base::Value::Dict input = base::test::ParseJsonDict(test_case.json);

    EXPECT_THAT(AggregatableFilteringIdsMaxBytes::Parse(input),
                test_case.matches);
  }
}

TEST(AggregatableFilteringIdsMaxBytes, CanEncompass) {
  const struct {
    const char* description;
    const uint64_t filtering_id;
    const AggregatableFilteringIdsMaxBytes max_bytes;
    bool expected;
  } kTestCases[]{
      {
          "default-can-encompass",
          255,
          AggregatableFilteringIdsMaxBytes(),
          true,
      },
      {
          "default-cant-encompass",
          256,
          AggregatableFilteringIdsMaxBytes(),
          false,
      },
      {
          "non-default-can-encompass",
          65535,
          AggregatableFilteringIdsMaxBytes(2u),
          true,
      },
      {
          "non-default-cant-encompass",
          65536,
          AggregatableFilteringIdsMaxBytes(2u),
          false,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);

    EXPECT_EQ(test_case.max_bytes.CanEncompass(test_case.filtering_id),
              test_case.expected);
  }
}

}  // namespace
}  // namespace attribution_reporting
