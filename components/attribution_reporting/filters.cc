// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/filters.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

enum class FilterValuesError {
  kTooManyKeys,
  kKeyTooLong,
  kListWrongType,
  kListTooLong,
  kValueWrongType,
  kValueTooLong,
};

constexpr char kFilters[] = "filters";
constexpr char kNotFilters[] = "not_filters";

bool IsValidForSourceOrTrigger(const FilterValues& filter_values) {
  if (filter_values.size() > kMaxFiltersPerSource)
    return false;

  for (const auto& [filter, values] : filter_values) {
    if (filter.size() > kMaxBytesPerFilterString)
      return false;

    if (values.size() > kMaxValuesPerFilter)
      return false;

    for (const auto& value : values) {
      if (value.size() > kMaxBytesPerFilterString)
        return false;
    }
  }

  return true;
}

bool IsValidForSource(const FilterValues& filter_values) {
  return !filter_values.contains(FilterData::kSourceTypeFilterKey) &&
         IsValidForSourceOrTrigger(filter_values);
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
    base::Value::Dict dict) {
  const size_t num_filters = dict.size();
  if (num_filters > kMaxFiltersPerSource)
    return base::unexpected(FilterValuesError::kTooManyKeys);

  RecordFiltersPerFilterData(num_filters);

  FilterValues::container_type filter_values;
  filter_values.reserve(dict.size());

  for (auto [filter, value] : dict) {
    if (filter.size() > kMaxBytesPerFilterString)
      return base::unexpected(FilterValuesError::kKeyTooLong);

    base::Value::List* list = value.GetIfList();
    if (!list)
      return base::unexpected(FilterValuesError::kListWrongType);

    const size_t num_values = list->size();
    if (num_values > kMaxValuesPerFilter)
      return base::unexpected(FilterValuesError::kListTooLong);

    RecordValuesPerFilter(num_values);

    std::vector<std::string> values;
    values.reserve(num_values);

    for (base::Value& item : *list) {
      std::string* string = item.GetIfString();
      if (!string)
        return base::unexpected(FilterValuesError::kValueWrongType);

      if (string->size() > kMaxBytesPerFilterString)
        return base::unexpected(FilterValuesError::kValueTooLong);

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
  if (!IsValidForSource(filter_values))
    return absl::nullopt;

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

  auto filter_values = ParseFilterValuesFromJSON(std::move(*dict));
  if (filter_values.has_value())
    return FilterData(std::move(*filter_values));

  switch (filter_values.error()) {
    case FilterValuesError::kTooManyKeys:
      return base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys);
    case FilterValuesError::kKeyTooLong:
      return base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong);
    case FilterValuesError::kListWrongType:
      return base::unexpected(
          SourceRegistrationError::kFilterDataListWrongType);
    case FilterValuesError::kListTooLong:
      return base::unexpected(SourceRegistrationError::kFilterDataListTooLong);
    case FilterValuesError::kValueWrongType:
      return base::unexpected(
          SourceRegistrationError::kFilterDataValueWrongType);
    case FilterValuesError::kValueTooLong:
      return base::unexpected(SourceRegistrationError::kFilterDataValueTooLong);
  }
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
                         const Filters& filters,
                         bool negated) const {
  if (filters.disjunction().empty()) {
    return true;
  }

  // A filter_value is considered matched if the filter key is only present
  // either on the source or trigger, or the intersection of the filter values
  // is non-empty. Returns true if all the filters matched.
  //
  // If the filters are negated, the behavior should be that every single filter
  // key does not match between the two (negating the function result is not
  // sufficient by the API definition).
  return base::ranges::any_of(
      filters.disjunction(), [&](const auto& trigger_filter_values) {
        return base::ranges::all_of(
            trigger_filter_values, [&](const auto& trigger_filter) {
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
                                   const Filters& filters,
                                   bool negated) const {
  return Matches(source_type, filters, negated);
}

bool FilterData::Matches(mojom::SourceType source_type,
                         const FilterPair& filters) const {
  return Matches(source_type, filters.positive, /*negated=*/false) &&
         Matches(source_type, filters.negative, /*negated=*/true);
}

// static
absl::optional<Filters> Filters::Create(Filters::Disjunction filters) {
  if (!base::ranges::all_of(filters, &IsValidForSourceOrTrigger)) {
    return absl::nullopt;
  }

  return Filters(std::move(filters));
}

// static
base::expected<Filters, TriggerRegistrationError> Filters::FromJSON(
    base::Value* input_value) {
  if (!input_value) {
    return Filters();
  }

  Disjunction disjunction;
  const auto append_if_valid =
      [&disjunction](
          base::Value& value) -> absl::optional<TriggerRegistrationError> {
    base::Value::Dict* dict = value.GetIfDict();
    if (!dict) {
      return TriggerRegistrationError::kFiltersWrongType;
    }

    auto filter_values = ParseFilterValuesFromJSON(std::move(*dict));
    if (filter_values.has_value()) {
      if (!filter_values->empty()) {
        disjunction.push_back(std::move(filter_values.value()));
      }
      return absl::nullopt;
    }

    switch (filter_values.error()) {
      case FilterValuesError::kTooManyKeys:
        return TriggerRegistrationError::kFiltersTooManyKeys;
      case FilterValuesError::kKeyTooLong:
        return TriggerRegistrationError::kFiltersKeyTooLong;
      case FilterValuesError::kListWrongType:
        return TriggerRegistrationError::kFiltersListWrongType;
      case FilterValuesError::kListTooLong:
        return TriggerRegistrationError::kFiltersListTooLong;
      case FilterValuesError::kValueWrongType:
        return TriggerRegistrationError::kFiltersValueWrongType;
      case FilterValuesError::kValueTooLong:
        return TriggerRegistrationError::kFiltersValueTooLong;
    }
  };

  if (base::Value::List* list = input_value->GetIfList()) {
    disjunction.reserve(list->size());
    for (base::Value& item : *list) {
      if (auto error = append_if_valid(item)) {
        return base::unexpected(*error);
      }
    }
    return Filters(std::move(disjunction));
  }

  if (auto error = append_if_valid(*input_value)) {
    return base::unexpected(*error);
  }
  return Filters(std::move(disjunction));
}

// static
Filters Filters::ForSourceTypeForTesting(mojom::SourceType source_type) {
  std::vector<std::string> values;
  values.reserve(1);
  values.emplace_back(SourceTypeName(source_type));

  FilterValues filter_values;
  filter_values.reserve(1);
  filter_values.emplace(FilterData::kSourceTypeFilterKey, std::move(values));

  return Filters({std::move(filter_values)});
}

Filters::Filters() = default;

Filters::Filters(Filters::Disjunction disjunction)
    : disjunction_(std::move(disjunction)) {
  DCHECK(base::ranges::all_of(disjunction_, &IsValidForSourceOrTrigger));
}

Filters::~Filters() = default;

Filters::Filters(const Filters&) = default;

Filters::Filters(Filters&&) = default;

Filters& Filters::operator=(const Filters&) = default;

Filters& Filters::operator=(Filters&&) = default;

base::Value::List Filters::ToJson() const {
  base::Value::List list;
  for (const auto& filter_values : disjunction_) {
    list.Append(FilterValuesToJson(filter_values));
  }
  return list;
}

void Filters::SerializeIfNotEmpty(base::Value::Dict& dict,
                                  base::StringPiece key) const {
  if (!disjunction_.empty()) {
    dict.Set(key, ToJson());
  }
}

// static
base::expected<FilterPair, mojom::TriggerRegistrationError>
FilterPair::FromJSON(base::Value::Dict& dict) {
  auto positive = Filters::FromJSON(dict.Find(kFilters));
  if (!positive.has_value()) {
    return base::unexpected(positive.error());
  }

  auto negative = Filters::FromJSON(dict.Find(kNotFilters));
  if (!negative.has_value()) {
    return base::unexpected(negative.error());
  }

  return FilterPair{.positive = std::move(*positive),
                    .negative = std::move(*negative)};
}

void FilterPair::SerializeIfNotEmpty(base::Value::Dict& dict) const {
  positive.SerializeIfNotEmpty(dict, kFilters);
  negative.SerializeIfNotEmpty(dict, kNotFilters);
}

}  // namespace attribution_reporting
