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
#include "base/test/values_test_util.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

FilterValues CreateFilterValues(size_t n) {
  FilterValues filter_values;
  for (size_t i = 0; i < n; i++) {
    filter_values.emplace(base::NumberToString(i), std::vector<std::string>());
  }
  CHECK_EQ(filter_values.size(), n);
  return filter_values;
}

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
  const auto make_filter_data_with_keys = [](size_t n) {
    base::Value::Dict dict;
    for (size_t i = 0; i < n; ++i) {
      dict.Set(base::NumberToString(i), base::Value::List());
    }
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_key_length = [](size_t n) {
    base::Value::Dict dict;
    dict.Set(std::string(n, 'a'), base::Value::List());
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_values = [](size_t n) {
    base::Value::List list;
    for (size_t i = 0; i < n; ++i) {
      list.Append("x");
    }

    base::Value::Dict dict;
    dict.Set("a", std::move(list));
    return base::Value(std::move(dict));
  };

  const auto make_filter_data_with_value_length = [](size_t n) {
    base::Value::List list;
    list.Append(std::string(n, 'a'));

    base::Value::Dict dict;
    dict.Set("a", std::move(list));
    return base::Value(std::move(dict));
  };

  struct {
    const char* description;
    absl::optional<base::Value> json;
    base::expected<FilterData, SourceRegistrationError> expected;
  } kTestCases[] = {
      {
          "Null",
          absl::nullopt,
          FilterData(),
      },
      {
          "empty",
          base::Value(base::Value::Dict()),
          FilterData(),
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
      },
      {
          "forbidden_key",
          base::test::ParseJson(R"json({
          "source_type": ["a"]
        })json"),
          base::unexpected(
              SourceRegistrationError::kFilterDataHasSourceTypeKey),
      },
      {
          "not_dictionary",
          base::Value(base::Value::List()),
          base::unexpected(SourceRegistrationError::kFilterDataWrongType),
      },
      {
          "value_not_array",
          base::test::ParseJson(R"json({"a": true})json"),
          base::unexpected(SourceRegistrationError::kFilterDataListWrongType),
      },
      {
          "array_element_not_string",
          base::test::ParseJson(R"json({"a": [true]})json"),
          base::unexpected(SourceRegistrationError::kFilterDataValueWrongType),
      },
      {
          "too_many_keys",
          make_filter_data_with_keys(51),
          base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys),
      },
      {
          "key_too_long",
          make_filter_data_with_key_length(26),
          base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong),
      },
      {
          "too_many_values",
          make_filter_data_with_values(51),
          base::unexpected(SourceRegistrationError::kFilterDataListTooLong),
      },
      {
          "value_too_long",
          make_filter_data_with_value_length(26),
          base::unexpected(SourceRegistrationError::kFilterDataValueTooLong),
      },
  };

  for (auto& test_case : kTestCases) {
    EXPECT_EQ(FilterData::FromJSON(base::OptionalToPtr(test_case.json)),
              test_case.expected)
        << test_case.description;
  }

  {
    base::Value json = make_filter_data_with_keys(50);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_key_length(25);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_values(50);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }

  {
    base::Value json = make_filter_data_with_value_length(25);
    EXPECT_TRUE(FilterData::FromJSON(&json).has_value());
  }
}

}  // namespace
}  // namespace attribution_reporting
