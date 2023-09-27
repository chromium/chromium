// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/filters.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

enum class FilterValuesError {
  kListWrongType,
  kValueWrongType,
  kTooManyKeys,
  kKeyTooLong,
  kListTooLong,
  kValueTooLong,
};

constexpr char kFilters[] = "filters";
constexpr char kNotFilters[] = "not_filters";

bool IsValidForSource(const FilterValues& filter_values) {
  if (filter_values.contains(FilterData::kSourceTypeFilterKey)) {
    return false;
  }

  if (filter_values.size() > kMaxFiltersPerSource) {
    return false;
  }

  for (const auto& [filter, values] : filter_values) {
    if (filter.size() > kMaxBytesPerFilterString) {
      return false;
    }

    if (values.size() > kMaxValuesPerFilter) {
      return false;
    }

    for (const auto& value : values) {
      if (value.size() > kMaxBytesPerFilterString) {
        return false;
      }
    }
  }

  return true;
}

// Records the Conversions.FiltersPerFilterData metric.
void RecordFiltersPerFilterData(base::HistogramBase::Sample count) {
  const int kExclusiveMaxHistogramValue = 101;

  static_assert(
      kMaxFiltersPerSource < kExclusiveMaxHistogramValue,
      "Bump the version for histogram Conversions.FiltersPerFilterData");

  // The metrics are called potentially many times while parsing an attribution
  // header, therefore using the macros to avoid the overhead of taking a lock
  // and performing a map lookup.
  UMA_HISTOGRAM_COUNTS_100("Conversions.FiltersPerFilterData", count);
}

// Records the Conversions.ValuesPerFilter metric.
void RecordValuesPerFilter(base::HistogramBase::Sample count) {
  const int kExclusiveMaxHistogramValue = 101;

  static_assert(kMaxValuesPerFilter < kExclusiveMaxHistogramValue,
                "Bump the version for histogram Conversions.ValuesPerFilter");

  UMA_HISTOGRAM_COUNTS_100("Conversions.ValuesPerFilter", count);
}

base::expected<FilterValues, FilterValuesError> ParseFilterValuesFromJSON(
    base::Value::Dict dict,
    const bool check_sizes) {
  const size_t num_filters = dict.size();
  if (check_sizes && num_filters > kMaxFiltersPerSource) {
    return base::unexpected(FilterValuesError::kTooManyKeys);
  }

  RecordFiltersPerFilterData(num_filters);

  FilterValues::container_type filter_values;
  filter_values.reserve(dict.size());

  for (auto [filter, value] : dict) {
    if (check_sizes && filter.size() > kMaxBytesPerFilterString) {
      return base::unexpected(FilterValuesError::kKeyTooLong);
    }

    base::Value::List* list = value.GetIfList();
    if (!list) {
      return base::unexpected(FilterValuesError::kListWrongType);
    }

    const size_t num_values = list->size();
    if (check_sizes && num_values > kMaxValuesPerFilter) {
      return base::unexpected(FilterValuesError::kListTooLong);
    }

    RecordValuesPerFilter(num_values);

    std::vector<std::string> values;
    values.reserve(num_values);

    for (base::Value& item : *list) {
      std::string* string = item.GetIfString();
      if (!string) {
        return base::unexpected(FilterValuesError::kValueWrongType);
      }

      if (check_sizes && string->size() > kMaxBytesPerFilterString) {
        return base::unexpected(FilterValuesError::kValueTooLong);
      }

      values.push_back(std::move(*string));
    }

    filter_values.emplace_back(filter, std::move(values));
  }

  return FilterValues(base::sorted_unique, std::move(filter_values));
}

base::Value::Dict FilterValuesToJson(const FilterValues& filter_values) {
  base::Value::Dict dict;
  for (auto [key, values] : filter_values) {
    base::Value::List list;
    for (const auto& value : values) {
      list.Append(value);
    }
    dict.Set(key, std::move(list));
  }
  return dict;
}

}  // namespace

// static
absl::optional<FilterData> FilterData::Create(FilterValues filter_values) {
  if (!IsValidForSource(filter_values)) {
    return absl::nullopt;
  }

  return FilterData(std::move(filter_values));
}

// static
base::expected<FilterData, SourceRegistrationError> FilterData::FromJSON(
    base::Value* input_value) {
  if (!input_value) {
    return FilterData();
  }

  base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict) {
    return base::unexpected(SourceRegistrationError::kFilterDataWrongType);
  }

  if (dict->contains(kSourceTypeFilterKey)) {
    return base::unexpected(
        SourceRegistrationError::kFilterDataHasSourceTypeKey);
  }
  if (dict->contains(FilterConfig::kLookbackWindowKey)) {
    return base::unexpected(
        SourceRegistrationError::kFilterDataHasLookbackWindowKey);
  }

  const auto map_errors = [](FilterValuesError error) {
    switch (error) {
      case FilterValuesError::kTooManyKeys:
        return SourceRegistrationError::kFilterDataTooManyKeys;
      case FilterValuesError::kKeyTooLong:
        return SourceRegistrationError::kFilterDataKeyTooLong;
      case FilterValuesError::kListWrongType:
        return SourceRegistrationError::kFilterDataListWrongType;
      case FilterValuesError::kListTooLong:
        return SourceRegistrationError::kFilterDataListTooLong;
      case FilterValuesError::kValueWrongType:
        return SourceRegistrationError::kFilterDataValueWrongType;
      case FilterValuesError::kValueTooLong:
        return SourceRegistrationError::kFilterDataValueTooLong;
      default:
        NOTREACHED_NORETURN();
    }
  };
  ASSIGN_OR_RETURN(auto filter_values,
                   ParseFilterValuesFromJSON(std::move(*dict),
                                             /*check_sizes=*/true)
                       .transform_error(map_errors));
  return FilterData(std::move(filter_values));
}

FilterData::FilterData() = default;

FilterData::FilterData(FilterValues filter_values)
    : filter_values_(std::move(filter_values)) {
  DCHECK(IsValidForSource(filter_values_));
}

FilterData::~FilterData() = default;

FilterData::FilterData(const FilterData&) = default;

FilterData::FilterData(FilterData&&) = default;

FilterData& FilterData::operator=(const FilterData&) = default;

FilterData& FilterData::operator=(FilterData&&) = default;

base::Value::Dict FilterData::ToJson() const {
  return FilterValuesToJson(filter_values_);
}

bool FilterData::Matches(mojom::SourceType source_type,
                         const base::Time& source_time,
                         const base::Time& trigger_time,
                         const FiltersDisjunction& filters,
                         bool negated) const {
  if (filters.empty()) {
    return true;
  }

  // While contradictory, it is possible for a source to have an assigned time
  // of T and a trigger that is attributed to it to have a time of T-X e.g. due
  // to user-initiated clock changes. see: https://crbug.com/1486489
  //
  // TODO(https://crbug.com/1486496): Assume `source_time` is smaller than
  // `trigger_time` once attribution time resolution is implemented in storage.
  const base::TimeDelta duration_since_source_registration =
      (source_time < trigger_time) ? trigger_time - source_time
                                   : base::Microseconds(0);

  // A filter_value is considered matched if the filter key is only present
  // either on the source or trigger, or the intersection of the filter values
  // is non-empty. Returns true if all the filters matched.
  //
  // If the filters are negated, the behavior should be that every single filter
  // key does not match between the two (negating the function result is not
  // sufficient by the API definition).
  return base::ranges::any_of(filters, [&](const FilterConfig& config) {
    if (config.lookback_window()) {
      if (duration_since_source_registration >
          config.lookback_window().value()) {
        if (!negated) {
          return false;
        }
      } else if (negated) {
        return false;
      }
    }

    return base::ranges::all_of(
        config.filter_values(), [&](const auto& trigger_filter) {
          if (trigger_filter.first == kSourceTypeFilterKey) {
            bool has_intersection = base::ranges::any_of(
                trigger_filter.second, [&](const std::string& value) {
                  return value == SourceTypeName(source_type);
                });

            return negated != has_intersection;
          }

          auto source_filter = filter_values_.find(trigger_filter.first);
          if (source_filter == filter_values_.end()) {
            return true;
          }

          // Desired behavior is to treat any empty set of values as a
          // single unique value itself. This means:
          //  - x:[] match x:[] is false when negated, and true otherwise.
          //  - x:[1,2,3] match x:[] is true when negated, and false
          //  otherwise.
          if (trigger_filter.second.empty()) {
            return negated != source_filter->second.empty();
          }

          bool has_intersection = base::ranges::any_of(
              trigger_filter.second, [&](const std::string& value) {
                return base::Contains(source_filter->second, value);
              });
          // Negating filters are considered matched if the intersection of
          // the filter values is empty.
          return negated != has_intersection;
        });
  });
}

bool FilterData::MatchesForTesting(mojom::SourceType source_type,
                                   const base::Time& source_time,
                                   const base::Time& trigger_time,
                                   const FiltersDisjunction& filters,
                                   bool negated) const {
  return Matches(source_type, source_time, trigger_time, filters, negated);
}

bool FilterData::Matches(mojom::SourceType source_type,
                         const base::Time& source_time,
                         const base::Time& trigger_time,
                         const FilterPair& filters) const {
  return Matches(source_type, source_time, trigger_time, filters.positive,
                 /*negated=*/false) &&
         Matches(source_type, source_time, trigger_time, filters.negative,
                 /*negated=*/true);
}

FilterConfig::FilterConfig() = default;

absl::optional<FilterConfig> FilterConfig::Create(
    FilterValues filter_values,
    absl::optional<base::TimeDelta> lookback_window) {
  if (lookback_window && !lookback_window->is_positive()) {
    return absl::nullopt;
  }
  return FilterConfig(std::move(filter_values), lookback_window);
}

FilterConfig::FilterConfig(FilterValues filter_values,
                           absl::optional<base::TimeDelta> lookback_window)
    : lookback_window_(lookback_window),
      filter_values_(std::move(filter_values)) {
  DCHECK(!lookback_window_.has_value() || lookback_window_->is_positive());
}

FilterConfig::~FilterConfig() = default;

FilterConfig::FilterConfig(const FilterConfig&) = default;

FilterConfig::FilterConfig(FilterConfig&&) = default;

FilterConfig& FilterConfig::operator=(const FilterConfig&) = default;

FilterConfig& FilterConfig::operator=(FilterConfig&&) = default;

namespace {

base::expected<FiltersDisjunction, TriggerRegistrationError> FiltersFromJSON(
    base::Value* input_value) {
  if (!input_value) {
    return FiltersDisjunction();
  }

  FiltersDisjunction disjunction;
  const auto append_if_valid = [&disjunction](base::Value& value)
      -> base::expected<void, TriggerRegistrationError> {
    base::Value::Dict* dict = value.GetIfDict();
    if (!dict) {
      return base::unexpected(TriggerRegistrationError::kFiltersWrongType);
    }

    absl::optional<base::TimeDelta> lookback_window;
    absl::optional<base::Value> lookback_window_value =
        dict->Extract(FilterConfig::kLookbackWindowKey);
    if (lookback_window_value.has_value()) {
      if (lookback_window_value->is_int()) {
        lookback_window = base::Seconds(lookback_window_value->GetInt());
      } else {
        return base::unexpected(
            TriggerRegistrationError::kFiltersValueWrongType);
      }
    }

    const auto map_errors = [](FilterValuesError error) {
      if (error == FilterValuesError::kValueWrongType) {
        return TriggerRegistrationError::kFiltersValueWrongType;
      }
      CHECK_EQ(FilterValuesError::kListWrongType, error);
      return TriggerRegistrationError::kFiltersListWrongType;
    };
    ASSIGN_OR_RETURN(
        auto filter_values,
        ParseFilterValuesFromJSON(std::move(*dict), /*check_sizes=*/false)
            .transform_error(map_errors));
    if (!filter_values.empty() || lookback_window.has_value()) {
      auto config =
          FilterConfig::Create(std::move(filter_values), lookback_window);
      if (!config.has_value()) {
        return base::unexpected(
            TriggerRegistrationError::kFiltersValueWrongType);
      }
      disjunction.push_back(std::move(config.value()));
    }
    return base::ok();
  };

  if (base::Value::List* list = input_value->GetIfList()) {
    disjunction.reserve(list->size());
    for (base::Value& item : *list) {
      RETURN_IF_ERROR(append_if_valid(item));
    }
  } else {
    RETURN_IF_ERROR(append_if_valid(*input_value));
  }
  return disjunction;
}

base::Value::List ToJson(const FiltersDisjunction& filters) {
  base::Value::List list;
  for (const auto& filter_config : filters) {
    base::Value::Dict dict = FilterValuesToJson(filter_config.filter_values());
    if (filter_config.lookback_window().has_value()) {
      dict.Set(FilterConfig::kLookbackWindowKey,
               static_cast<int>(
                   filter_config.lookback_window().value().InSeconds()));
    }
    list.Append(std::move(dict));
  }
  return list;
}

}  // namespace

// static
base::expected<FilterPair, mojom::TriggerRegistrationError>
FilterPair::FromJSON(base::Value::Dict& dict) {
  ASSIGN_OR_RETURN(auto positive, FiltersFromJSON(dict.Find(kFilters)));
  ASSIGN_OR_RETURN(auto negative, FiltersFromJSON(dict.Find(kNotFilters)));
  return FilterPair(std::move(positive), std::move(negative));
}

FilterPair::FilterPair() = default;

FilterPair::FilterPair(FiltersDisjunction positive, FiltersDisjunction negative)
    : positive(std::move(positive)), negative(std::move(negative)) {}

FilterPair::~FilterPair() = default;

FilterPair::FilterPair(const FilterPair&) = default;

FilterPair::FilterPair(FilterPair&&) = default;

FilterPair& FilterPair::operator=(const FilterPair&) = default;

FilterPair& FilterPair::operator=(FilterPair&&) = default;

void FilterPair::SerializeIfNotEmpty(base::Value::Dict& dict) const {
  if (!positive.empty()) {
    dict.Set(kFilters, ToJson(positive));
  }

  if (!negative.empty()) {
    dict.Set(kNotFilters, ToJson(negative));
  }
}

base::expected<FiltersDisjunction, TriggerRegistrationError>
FiltersFromJSONForTesting(base::Value* input_value) {
  return FiltersFromJSON(input_value);
}

base::Value::List ToJsonForTesting(const FiltersDisjunction& filters) {
  return ToJson(filters);
}

}  // namespace attribution_reporting
