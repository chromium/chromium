// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/logging_util.h"

#include <string_view>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/personal_context/proto/features/at_memory.pb.h"

namespace autofill {

using ::personal_context::proto::AutofillFetchPlan;
using ::personal_context::proto::AutofillFetchSpecification;
using ::personal_context::proto::TypedValue;
using Filter = AutofillFetchSpecification::Filter;
using StringFilter = AutofillFetchSpecification::StringFilter;
using TypedValueFilter = AutofillFetchSpecification::TypedValueFilter;

namespace {

// Returns a human-readable string representation of `mode`.
std::string_view StringFilterModeToStringView(
    StringFilter::StringFilterMode mode) {
  switch (mode) {
    case StringFilter::STRING_FILTER_MODE_UNSPECIFIED:
      return "UNSPECIFIED";
    case StringFilter::STRING_FILTER_MODE_SUBSTRING:
      return "SUBSTRING";
    case StringFilter::STRING_FILTER_MODE_EXACT:
      return "EXACT";
    case StringFilter::STRING_FILTER_MODE_FUZZY:
      return "FUZZY";
    default:
      return "UNKNOWN";
  }
}

// Returns a human-readable string representation of `op`.
std::string_view FilterOperatorToStringView(
    TypedValueFilter::FilterOperator op) {
  switch (op) {
    case TypedValueFilter::FILTER_OPERATOR_UNSPECIFIED:
      return "UNSPECIFIED";
    case TypedValueFilter::FILTER_OPERATOR_EQUAL:
      return "EQUAL";
    case TypedValueFilter::FILTER_OPERATOR_NOT_EQUAL:
      return "NOT_EQUAL";
    case TypedValueFilter::FILTER_OPERATOR_LESS_THAN:
      return "LESS_THAN";
    case TypedValueFilter::FILTER_OPERATOR_LESS_THAN_OR_EQUAL:
      return "LESS_THAN_OR_EQUAL";
    case TypedValueFilter::FILTER_OPERATOR_GREATER_THAN:
      return "GREATER_THAN";
    case TypedValueFilter::FILTER_OPERATOR_GREATER_THAN_OR_EQUAL:
      return "GREATER_THAN_OR_EQUAL";
    default:
      return "UNKNOWN";
  }
}

// Returns true if any data type in `filter` is considered sensitive
// information.
bool ContainsSpiiDataType(const Filter& filter) {
  for (int i = 0; i < filter.data_types_size(); ++i) {
    if (IsSpiiMemoryDataType(ToMemoryDataType(filter.data_types(i)))) {
      return true;
    }
  }
  return false;
}

// Formats filter `value` for logging, redacting sensitive information.
template <typename T>
LogBuffer FormatFilterValue(const T& value, bool is_spii) {
  LogBuffer buf;
  if (is_spii) {
    buf << Tag{"span"} << Attrib{"data-pii", "true"} << value << CTag{"span"};
  } else {
    buf << value;
  }
  return buf;
}

}  // namespace

LogBuffer& operator<<(LogBuffer& buffer, const TypedValue& typed_value) {
  switch (typed_value.value_case()) {
    case TypedValue::kCountryCode:
      return buffer << typed_value.country_code();
    case TypedValue::kDate:
      return buffer << base::StringPrintf(
                 "%04d-%02d-%02d", typed_value.date().year(),
                 typed_value.date().month(), typed_value.date().day());
    case TypedValue::kDateTime:
      return buffer << base::StringPrintf("%04d-%02d-%02d %02d:%02d:%02d",
                                          typed_value.date_time().year(),
                                          typed_value.date_time().month(),
                                          typed_value.date_time().day(),
                                          typed_value.date_time().hours(),
                                          typed_value.date_time().minutes(),
                                          typed_value.date_time().seconds());
    case TypedValue::kStringList:
      return buffer << "["
                    << base::JoinString(
                           base::ToVector(
                               typed_value.string_list().values(),
                               [](const std::string& s) -> std::string_view {
                                 return s;
                               }),
                           ", ")
                    << "]";
    case TypedValue::VALUE_NOT_SET:
      return buffer << "(not set)";
  }
}

LogBuffer& operator<<(LogBuffer& buffer, const Filter& filter) {
  if (filter.data_types().empty()) {
    buffer << Tr{} << "Data Types:" << "(none)";
  } else {
    LogBuffer dt_buf;
    for (int i = 0; i < filter.data_types_size(); ++i) {
      if (i > 0) {
        dt_buf << ", ";
      }
      dt_buf << MemoryDataTypeToStringView(
          ToMemoryDataType(filter.data_types(i)));
    }
    buffer << Tr{} << "Data Types:" << std::move(dt_buf);
  }

  const bool is_spii = ContainsSpiiDataType(filter);

  if (filter.has_string_filter()) {
    buffer << Tr{} << "String Filter:"
           << FormatFilterValue(filter.string_filter().value(), is_spii);
    buffer << Tr{} << "Filter mode:"
           << StringFilterModeToStringView(filter.string_filter().mode());
  }

  if (filter.has_typed_value_filter()) {
    if (filter.typed_value_filter().has_typed_value()) {
      buffer << Tr{} << "Typed Value Filter:"
             << FormatFilterValue(filter.typed_value_filter().typed_value(),
                                  is_spii);
    }
    buffer << Tr{} << "Operator:"
           << FilterOperatorToStringView(
                  filter.typed_value_filter().filter_operator());
  }
  return buffer;
}

LogBuffer& operator<<(LogBuffer& buffer,
                      const AutofillFetchSpecification& spec) {
  buffer << Tr{} << "Data Type:"
         << MemoryDataTypeToStringView(ToMemoryDataType(spec.data_type()));

  if (spec.filters().empty()) {
    buffer << Tr{} << "Filters:" << "(none)";
  } else {
    LogBuffer filters_buf;
    filters_buf << Tag{"table"};
    for (const AutofillFetchSpecification::Filter& filter : spec.filters()) {
      filters_buf << filter;
    }
    filters_buf << CTag{"table"};
    buffer << Tr{} << "Filters:" << std::move(filters_buf);
  }
  return buffer;
}

LogBuffer& operator<<(LogBuffer& buffer, const AutofillFetchPlan& plan) {
  if (plan.fetch_specifications().empty()) {
    buffer << "(No fetch specifications)";
    return buffer;
  }
  buffer << Tag{"table"};
  for (const AutofillFetchSpecification& spec : plan.fetch_specifications()) {
    buffer << spec;
  }
  buffer << CTag{"table"};
  return buffer;
}

}  // namespace autofill
