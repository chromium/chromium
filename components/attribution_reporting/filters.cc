// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/filters.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

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
  // TODO(johnidel): Consider logging registration JSON metrics here.
  if (!input_value)
    return FilterData();

  base::Value::Dict* dict = input_value->GetIfDict();
  if (!dict)
    return base::unexpected(SourceRegistrationError::kFilterDataWrongType);

  const size_t num_filters = dict->size();
  if (num_filters > kMaxFiltersPerSource)
    return base::unexpected(SourceRegistrationError::kFilterDataTooManyKeys);

  if (dict->contains(kSourceTypeFilterKey)) {
    return base::unexpected(
        SourceRegistrationError::kFilterDataHasSourceTypeKey);
  }

  FilterValues::container_type filter_values;
  filter_values.reserve(dict->size());

  for (auto [filter, value] : *dict) {
    if (filter.size() > kMaxBytesPerFilterString)
      return base::unexpected(SourceRegistrationError::kFilterDataKeyTooLong);

    base::Value::List* list = value.GetIfList();
    if (!list) {
      return base::unexpected(
          SourceRegistrationError::kFilterDataListWrongType);
    }

    const size_t num_values = list->size();
    if (num_values > kMaxValuesPerFilter)
      return base::unexpected(SourceRegistrationError::kFilterDataListTooLong);

    std::vector<std::string> values;
    values.reserve(num_values);

    for (base::Value& item : *list) {
      std::string* string = item.GetIfString();
      if (!string) {
        return base::unexpected(
            SourceRegistrationError::kFilterDataValueWrongType);
      }

      if (string->size() > kMaxBytesPerFilterString) {
        return base::unexpected(
            SourceRegistrationError::kFilterDataValueTooLong);
      }

      values.push_back(std::move(*string));
    }

    filter_values.emplace_back(filter, std::move(values));
  }

  return FilterData(
      FilterValues(base::sorted_unique, std::move(filter_values)));
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

// static
absl::optional<Filters> Filters::Create(FilterValues filter_values) {
  if (!IsValidForSourceOrTrigger(filter_values))
    return absl::nullopt;

  return Filters(std::move(filter_values));
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

}  // namespace attribution_reporting
