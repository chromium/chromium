// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/filters.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerRegistrationError;

FilterValues CreateFilterValues(size_t n) {
  FilterValues filter_values;
  for (size_t i = 0; i < n; i++) {
    filter_values.emplace(base::NumberToString(i), std::vector<std::string>());
  }
  CHECK_EQ(filter_values.size(), n);
  return filter_values;
}

base::Value MakeFilterValuesWithKeys(size_t n) {
  base::Value::Dict dict;
  for (size_t i = 0; i < n; ++i) {
    dict.Set(base::NumberToString(i), base::Value::List());
  }
  return base::Value(std::move(dict));
}

base::Value MakeFilterValuesWithKeyLength(size_t n) {
  base::Value::Dict dict;
  dict.Set(std::string(n, 'a'), base::Value::List());
  return base::Value(std::move(dict));
}

base::Value MakeFilterValuesWithValues(size_t n) {
  base::Value::List list;
  for (size_t i = 0; i < n; ++i) {
    list.Append("x");
  }

  base::Value::Dict dict;
  dict.Set("a", std::move(list));
  return base::Value(std::move(dict));
}

base::Value MakeFilterValuesWithValueLength(size_t n) {
  base::Value::List list;
  list.Append(std::string(n, 'a'));

  base::Value::Dict dict;
  dict.Set("a", std::move(list));
  return base::Value(std::move(dict));
}

const struct {
  const char* description;
  absl::optional<base::Value> json;
  base::expected<FilterData, SourceRegistrationError> expected_filter_data;
  base::expected<Filters, TriggerRegistrationError> expected_filters;
} kParseTestCases[] = {
    {
        "Null",
        absl::nullopt,
        FilterData(),
        Filters(),
    },
    {
        "empty",
        base::Value(base::Value::Dict()),
        FilterData(),
        Filters(),
    },
    {
        "multiple",
        base::test::ParseJson(R"json({
            "a": ["b"],
            "c": ["e", "d"],
            "f": []
          })json"),
        *FilterData::Create({
            {"a", {"b"}},
            {"c", {"e", "d"}},
            {"f", {}},
        }),
        *Filters::Create({
            {"a", {"b"}},
            {"c", {"e", "d"}},
            {"f", {}},
        }),
    },
    {
        "source_type_key",
        base::test::ParseJson(R"json({
          "source_type": ["a"]
        })json"),
        base::unexpected(SourceRegistrationError::kFilterDataHasSourceTypeKey),
        *Filters::Create({{"source_type", {"a"}}}),
    },
    {
        "not_dictionary",
        base::Value(base::Value::List()),
        base::unexpected(SourceRegistrationError::kFilterDataWrongType),
        base::unexpected(TriggerRegistrationError::kFiltersWrongType),
    },
    {
        "value_not_array",
        base::test::ParseJson(R"json({"a": true})json"),
        base::unexpected(SourceRegistrationError::kFilterDataListWrongType),
        base::unexpected(TriggerRegistrationError::kFiltersListWrongType),
    },
    {
        "array_element_not_string",
        base::test::ParseJson(R"json({"a": [true]})json"),
        base::unexpected(SourceRegistrationError::kFilterDataValueWrongType),
        base::unexpected(TriggerRegistrationError::kFiltersValueWrongType),
    },
    {
        "too_many_keys",
        MakeFilterValuesWithKeys(51),
        base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys),
        base::unexpected(TriggerRegistrationError::kFiltersTooManyKeys),
    },
    {
        "key_too_long",
        MakeFilterValuesWithKeyLength(26),
        base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong),
        base::unexpected(TriggerRegistrationError::kFiltersKeyTooLong),
    },
    {
        "too_many_values",
        MakeFilterValuesWithValues(51),
        base::unexpected(SourceRegistrationError::kFilterDataListTooLong),
        base::unexpected(TriggerRegistrationError::kFiltersListTooLong),
    },
    {
        "value_too_long",
        MakeFilterValuesWithValueLength(26),
        base::unexpected(SourceRegistrationError::kFilterDataValueTooLong),
        base::unexpected(TriggerRegistrationError::kFiltersValueTooLong),
    },
};

TEST(FilterDataTest, Create_ProhibitsSourceTypeFilter) {
  EXPECT_FALSE(FilterData::Create({{"source_type", {"event"}}}));
}

TEST(FiltersTest, Create_AllowsSourceTypeFilter) {
  EXPECT_TRUE(Filters::Create({{"source_type", {"event"}}}));
}

TEST(FilterDataTest, Create_LimitsFilterCount) {
  EXPECT_TRUE(
      FilterData::Create(CreateFilterValues(kMaxFiltersPerSource)).has_value());

  EXPECT_FALSE(FilterData::Create(CreateFilterValues(kMaxFiltersPerSource + 1))
                   .has_value());
}

TEST(FiltersTest, Create_LimitsFilterCount) {
  EXPECT_TRUE(
      Filters::Create(CreateFilterValues(kMaxFiltersPerSource)).has_value());

  EXPECT_FALSE(Filters::Create(CreateFilterValues(kMaxFiltersPerSource + 1))
                   .has_value());
}

TEST(FilterDataTest, FromJSON) {
  for (auto& test_case : kParseTestCases) {
    absl::optional<base::Value> json_copy =
        test_case.json ? absl::make_optional(test_case.json->Clone())
                       : absl::nullopt;
    EXPECT_EQ(FilterData::FromJSON(base::OptionalToPtr(json_copy)),
              test_case.expected_filter_data)
        << test_case.description;
  }

  {
    base::Value json = MakeFilterValuesWithKeys(50);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithKeyLength(25);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithValues(50);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithValueLength(25);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }
}

TEST(FilterDataTest, FromJSON_RecordsMetrics) {
  using ::base::Bucket;
  using ::testing::ElementsAre;

  absl::optional<base::Value> json = base::test::ParseJson(R"json({
      "a": ["1", "2", "3"],
      "b": [],
      "c": ["4"],
      "d": ["5"],
    })json");
  ASSERT_TRUE(json);

  base::HistogramTester histograms;
  ASSERT_TRUE(FilterData::FromJSON(base::OptionalToPtr(json)).has_value());

  EXPECT_THAT(histograms.GetAllSamples("Conversions.FiltersPerFilterData"),
              ElementsAre(Bucket(4, 1)));

  EXPECT_THAT(histograms.GetAllSamples("Conversions.ValuesPerFilter"),
              ElementsAre(Bucket(0, 1), Bucket(1, 2), Bucket(3, 1)));
}

TEST(FiltersTest, FromJSON) {
  for (auto& test_case : kParseTestCases) {
    absl::optional<base::Value> json_copy =
        test_case.json ? absl::make_optional(test_case.json->Clone())
                       : absl::nullopt;
    EXPECT_EQ(Filters::FromJSON(base::OptionalToPtr(json_copy)),
              test_case.expected_filters)
        << test_case.description;
  }

  {
    base::Value json = MakeFilterValuesWithKeys(50);
    EXPECT_TRUE(Filters::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithKeyLength(25);
    EXPECT_TRUE(Filters::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithValues(50);
    EXPECT_TRUE(Filters::FromJSON(&json).has_value());
  }

  {
    base::Value json = MakeFilterValuesWithValueLength(25);
    EXPECT_TRUE(Filters::FromJSON(&json).has_value());
  }
}

TEST(FilterValuesTest, ToJson) {
  const struct {
    FilterValues input;
    const char* expected_json;
  } kTestCases[] = {
      {
          FilterValues(),
          R"json({})json",
      },
      {
          FilterValues({{"a", {}}, {"b", {"c"}}}),
          R"json({"a":[],"b":["c"]})json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(FilterData::Create(test_case.input)->ToJson(),
                base::test::IsJson(test_case.expected_json));

    EXPECT_THAT(Filters::Create(test_case.input)->ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

TEST(FilterDataTest, EmptyOrMissingAttributionFilters) {
  const auto empty_filter = FilterValues();

  const auto empty_filter_values = FilterValues({{"filter1", {}}});

  const auto one_filter = FilterValues({{"filter1", {"value1"}}});

  const struct {
    const char* description;
    FilterValues filter_data;
    FilterValues filters;
  } kTestCases[] = {
      {"No source filters, no trigger filters", empty_filter, empty_filter},
      {"No source filters, trigger filter without values", empty_filter,
       empty_filter_values},
      {"No source filters, trigger filter with value", empty_filter,
       one_filter},

      {"Source filter without values, no trigger filters", empty_filter_values,
       empty_filter},

      {"Source filter with value, no trigger filters", one_filter,
       empty_filter}};

  // Behavior should match for negated and non-negated filters as it
  // requires a value on each side.
  for (const auto& test_case : kTestCases) {
    absl::optional<FilterData> filter_data =
        FilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<Filters> filters = Filters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_TRUE(filter_data->MatchesForTesting(SourceType::kNavigation,
                                               *filters, /*negated=*/false))
        << test_case.description;

    EXPECT_TRUE(filter_data->MatchesForTesting(SourceType::kNavigation,
                                               *filters, /*negated=*/true))
        << test_case.description << " with negation";
  }
}

TEST(FilterDataTest, AttributionFilterDataMatch) {
  const auto empty_filter_values = FilterValues({{"filter1", {}}});

  const auto one_filter = FilterValues({{"filter1", {"value1"}}});

  const auto one_filter_different = FilterValues({{"filter1", {"value2"}}});

  const auto two_filters =
      FilterValues({{"filter1", {"value1"}}, {"filter2", {"value2"}}});

  const auto one_mismatched_filter =
      FilterValues({{"filter1", {"value1"}}, {"filter2", {"value3"}}});

  const auto two_mismatched_filter =
      FilterValues({{"filter1", {"value3"}}, {"filter2", {"value4"}}});

  const struct {
    const char* description;
    FilterValues filter_data;
    FilterValues filters;
    bool match_expected;
  } kTestCases[] = {
      {"Source filter without values, trigger filter with value",
       empty_filter_values, one_filter, false},
      {"Source filter without values, trigger filter without values",
       empty_filter_values, empty_filter_values, true},
      {"Source filter with value, trigger filter without values", one_filter,
       empty_filter_values, false},

      {"One filter with matching values", one_filter, one_filter, true},
      {"One filter with no matching values", one_filter, one_filter_different,
       false},

      {"Two filters with matching values", two_filters, two_filters, true},
      {"Two filters no matching values", one_mismatched_filter,
       two_mismatched_filter, false},

      {"One filter not present in source, other matches", one_filter,
       two_filters, true},
      {"One filter not present in trigger, other matches", two_filters,
       one_filter, true},

      {"Two filters one filter no match", two_filters, one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<FilterData> filter_data =
        FilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<Filters> filters = Filters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_EQ(test_case.match_expected,
              filter_data->MatchesForTesting(SourceType::kNavigation, *filters,
                                             /*negated=*/false))
        << test_case.description;
  }
}

TEST(FilterDataTest, NegatedAttributionFilterDataMatch) {
  const auto empty_filter_values = FilterValues({{"filter1", {}}});

  const auto one_filter = FilterValues({{"filter1", {"value1"}}});

  const auto one_filter_different = FilterValues({{"filter1", {"value2"}}});

  const auto one_filter_one_different =
      FilterValues({{"filter1", {"value1", "value2"}}});

  const auto one_filter_multiple_different =
      FilterValues({{"filter1", {"value2", "value3"}}});

  const auto two_filters =
      FilterValues({{"filter1", {"value1"}}, {"filter2", {"value2"}}});

  const auto one_mismatched_filter =
      FilterValues({{"filter1", {"value1"}}, {"filter2", {"value3"}}});

  const auto two_mismatched_filter =
      FilterValues({{"filter1", {"value3"}}, {"filter2", {"value4"}}});

  const struct {
    const char* description;
    FilterValues filter_data;
    FilterValues filters;
    bool match_expected;
  } kTestCases[] = {
      // True because there is not matching values within source.
      {"Source filter without values, trigger filter with value",
       empty_filter_values, one_filter, true},
      {"Source filter without values, trigger filter without values",
       empty_filter_values, empty_filter_values, false},
      {"Source filter with value, trigger filter without values", one_filter,
       empty_filter_values, true},

      {"One filter with matching values", one_filter, one_filter, false},
      {"One filter with non-matching value", one_filter, one_filter_different,
       true},
      {"One filter with one non-matching value", one_filter,
       one_filter_one_different, false},
      {"One filter with multiple non-matching values", one_filter,
       one_filter_multiple_different, true},

      {"Two filters with matching values", two_filters, two_filters, false},
      {"Two filters no matching values", one_mismatched_filter,
       two_mismatched_filter, true},

      {"One filter not present in source, other matches", one_filter,
       two_filters, false},
      {"One filter not present in trigger, other matches", two_filters,
       one_filter, false},

      {"Two filters one filter no match", two_filters, one_mismatched_filter,
       false},
  };

  for (const auto& test_case : kTestCases) {
    absl::optional<FilterData> filter_data =
        FilterData::Create(test_case.filter_data);
    ASSERT_TRUE(filter_data) << test_case.description;

    absl::optional<Filters> filters = Filters::Create(test_case.filters);
    ASSERT_TRUE(filters) << test_case.description;

    EXPECT_EQ(test_case.match_expected,
              filter_data->MatchesForTesting(SourceType::kNavigation, *filters,
                                             /*negated=*/true))
        << test_case.description << " with negation";
  }
}

TEST(FilterDataTest, AttributionFilterDataMatch_SourceType) {
  const struct {
    const char* description;
    SourceType source_type;
    Filters filters;
    bool negated;
    bool match_expected;
  } kTestCases[] = {
      {
          .description = "empty-filters",
          .source_type = SourceType::kNavigation,
          .filters = Filters(),
          .negated = false,
          .match_expected = true,
      },
      {
          .description = "empty-filters-negated",
          .source_type = SourceType::kNavigation,
          .filters = Filters(),
          .negated = true,
          .match_expected = true,
      },
      {
          .description = "empty-filter-values",
          .source_type = SourceType::kNavigation,
          .filters = *Filters::Create({
              {FilterData::kSourceTypeFilterKey, {}},
          }),
          .negated = false,
          .match_expected = false,
      },
      {
          .description = "empty-filter-values-negated",
          .source_type = SourceType::kNavigation,
          .filters = *Filters::Create({
              {FilterData::kSourceTypeFilterKey, {}},
          }),
          .negated = true,
          .match_expected = true,
      },
      {
          .description = "same-source-type",
          .source_type = SourceType::kNavigation,
          .filters = Filters::ForSourceTypeForTesting(SourceType::kNavigation),
          .negated = false,
          .match_expected = true,
      },
      {
          .description = "same-source-type-negated",
          .source_type = SourceType::kNavigation,
          .filters = Filters::ForSourceTypeForTesting(SourceType::kNavigation),
          .negated = true,
          .match_expected = false,
      },
      {
          .description = "other-source-type",
          .source_type = SourceType::kNavigation,
          .filters = Filters::ForSourceTypeForTesting(SourceType::kEvent),
          .negated = false,
          .match_expected = false,
      },
      {
          .description = "other-source-type-negated",
          .source_type = SourceType::kNavigation,
          .filters = Filters::ForSourceTypeForTesting(SourceType::kEvent),
          .negated = true,
          .match_expected = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.match_expected,
              FilterData().MatchesForTesting(
                  test_case.source_type, test_case.filters, test_case.negated))
        << test_case.description;
  }
}

}  // namespace
}  // namespace attribution_reporting
