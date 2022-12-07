// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/filters.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

enum class FilterValuesError {
  kWrongType,
  kTooManyKeys,
  kFilterDataHasSourceTypeKey,
  kKeyTooLong,
  kListWrongType,
  kListTooLong,
  kValueWrongType,
  kValueTooLong,
};

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
    base::Value* input_value,
    bool is_filter_data) {
  if (!input_value)
    return FilterValues();

  base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict)
    return base::unexpected(FilterValuesError::kWrongType);

  const size_t num_filters = dict->size();
  if (num_filters > kMaxFiltersPerSource)
    return base::unexpected(FilterValuesError::kTooManyKeys);

  RecordFiltersPerFilterData(num_filters);

  if (is_filter_data && dict->contains(FilterData::kSourceTypeFilterKey))
    return base::unexpected(FilterValuesError::kFilterDataHasSourceTypeKey);

  FilterValues::container_type filter_values;
  filter_values.reserve(dict->size());

  for (auto [filter, value] : *dict) {
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
  auto filter_values =
      ParseFilterValuesFromJSON(input_value, /*is_filter_data=*/true);

  if (filter_values.has_value())
    return FilterData(std::move(*filter_values));

  switch (filter_values.error()) {
    case FilterValuesError::kWrongType:
      return base::unexpected(SourceRegistrationError::kFilterDataWrongType);
    case FilterValuesError::kTooManyKeys:
      return base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys);
    case FilterValuesError::kFilterDataHasSourceTypeKey:
      return base::unexpected(
          SourceRegistrationError::kFilterDataHasSourceTypeKey);
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

// static
absl::optional<Filters> Filters::Create(FilterValues filter_values) {
  if (!IsValidForSourceOrTrigger(filter_values))
    return absl::nullopt;

  return Filters(std::move(filter_values));
}

// static
base::expected<Filters, TriggerRegistrationError> Filters::FromJSON(
    base::Value* input_value) {
  auto filter_values =
      ParseFilterValuesFromJSON(input_value, /*is_filter_data=*/false);

  if (filter_values.has_value())
    return Filters(std::move(*filter_values));

  switch (filter_values.error()) {
    case FilterValuesError::kWrongType:
      return base::unexpected(TriggerRegistrationError::kFiltersWrongType);
    case FilterValuesError::kTooManyKeys:
      return base::unexpected(TriggerRegistrationError::kFiltersTooManyKeys);
    case FilterValuesError::kFilterDataHasSourceTypeKey:
      NOTREACHED();
      return base::unexpected(TriggerRegistrationError::kFiltersWrongType);
    case FilterValuesError::kKeyTooLong:
      return base::unexpected(TriggerRegistrationError::kFiltersKeyTooLong);
    case FilterValuesError::kListWrongType:
      return base::unexpected(TriggerRegistrationError::kFiltersListWrongType);
    case FilterValuesError::kListTooLong:
      return base::unexpected(TriggerRegistrationError::kFiltersListTooLong);
    case FilterValuesError::kValueWrongType:
      return base::unexpected(TriggerRegistrationError::kFiltersValueWrongType);
    case FilterValuesError::kValueTooLong:
      return base::unexpected(TriggerRegistrationError::kFiltersValueTooLong);
  }
}

Filters::Filters() = default;

Filters::Filters(FilterValues filter_values)
    : filter_values_(std::move(filter_values)) {
  DCHECK(IsValidForSourceOrTrigger(filter_values_));
}

Filters::~Filters() = default;

Filters::Filters(const Filters&) = default;

Filters::Filters(Filters&&) = default;

Filters& Filters::operator=(const Filters&) = default;

Filters& Filters::operator=(Filters&&) = default;

base::Value::Dict Filters::ToJson() const {
  return FilterValuesToJson(filter_values_);
}

void Filters::SerializeIfNotEmpty(base::Value::Dict& dict,
                                  base::StringPiece key) const {
  if (!filter_values_.empty()) {
    dict.Set(key, ToJson());
  }
}

}  // namespace attribution_reporting
