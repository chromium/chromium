// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <stdint.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

constexpr char kTriggerData[] = "trigger_data";
constexpr char kTriggerDataMatching[] = "trigger_data_matching";
constexpr char kTriggerSpecs[] = "trigger_specs";

constexpr char kTriggerDataMatchingExact[] = "exact";
constexpr char kTriggerDataMatchingModulus[] = "modulus";

// https://wicg.github.io/attribution-reporting-api/#max-distinct-trigger-data-per-source
constexpr uint8_t kMaxTriggerDataPerSource = 32;

base::expected<TriggerDataMatching, SourceRegistrationError>
ParseTriggerDataMatching(const base::Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingWrongType);
  } else if (*str == kTriggerDataMatchingExact) {
    return TriggerDataMatching::kExact;
  } else if (*str == kTriggerDataMatchingModulus) {
    return TriggerDataMatching::kModulus;
  } else {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingUnknownValue);
  }
}

std::string SerializeTriggerDataMatching(TriggerDataMatching v) {
  switch (v) {
    case TriggerDataMatching::kExact:
      return kTriggerDataMatchingExact;
    case TriggerDataMatching::kModulus:
      return kTriggerDataMatchingModulus;
  }
}

void SerializeTriggerConfig(const TriggerConfig& config,
                            base::Value::Dict& dict) {
  dict.Set(kTriggerDataMatching,
           SerializeTriggerDataMatching(config.trigger_data_matching()));
}

// If `dict` contains a valid "trigger_data" field, writes the resulting keys
// into `trigger_data_indices` using `trigger_data_index` as the value.
// `trigger_data_indices` is also used to perform deduplication checks.
[[nodiscard]] absl::optional<SourceRegistrationError> ParseTriggerData(
    const base::Value::Dict& dict,
    TriggerSpecs::TriggerDataIndices& trigger_data_indices,
    const uint8_t trigger_data_index) {
  const base::Value* value = dict.Find(kTriggerData);
  if (!value) {
    return SourceRegistrationError::kTriggerSpecTriggerDataMissing;
  }

  const base::Value::List* list = value->GetIfList();
  if (!list) {
    return SourceRegistrationError::kTriggerSpecTriggerDataWrongType;
  }

  if (list->empty()) {
    return SourceRegistrationError::kTriggerSpecTriggerDataEmpty;
  }

  if (list->size() + trigger_data_index > kMaxTriggerDataPerSource) {
    return SourceRegistrationError::kExcessiveTriggerData;
  }

  for (const base::Value& item : *list) {
    // We use `base::Value::GetIfDouble()`, which coerces if the value is an
    // integer, because trigger data values are `uint32_t`, but not all
    // `uint32_t` can be represented by 32-bit `int`. We use `std::modf` to
    // check that the fractional part of the `double` is 0.
    //
    // Assumes that all integers we care to support for trigger data (the full
    // range of `uint32_t`) can be represented either by `int` or `double`, and
    // that when represented internally by `base::Value` as an `int`, can be
    // precisely represented by `double`.
    //
    // TODO(apaseltiner): Consider test coverage for all `uint32_t` values, or
    // some kind of fuzzer.
    absl::optional<double> double_value = item.GetIfDouble();
    if (double int_part;
        !double_value.has_value() || std::modf(*double_value, &int_part) != 0) {
      return SourceRegistrationError::kTriggerSpecTriggerDataValueWrongType;
    }

    if (!base::IsValueInRangeForNumericType<uint32_t>(*double_value)) {
      return SourceRegistrationError::kTriggerSpecTriggerDataValueOutOfRange;
    }

    uint32_t trigger_data = static_cast<uint32_t>(*double_value);

    auto [_, inserted] =
        trigger_data_indices.try_emplace(trigger_data, trigger_data_index);
    if (!inserted) {
      return SourceRegistrationError::kDuplicateTriggerData;
    }
  }

  return absl::nullopt;
}

bool AreSpecsValid(const TriggerSpecs::TriggerDataIndices& trigger_data_indices,
                   const std::vector<TriggerSpec>& specs) {
  return trigger_data_indices.size() <= kMaxTriggerDataPerSource &&
         base::ranges::all_of(trigger_data_indices, [&specs](const auto& pair) {
           return pair.second < specs.size();
         });
}

bool AreSpecsValidForTriggerDataMatching(
    const TriggerSpecs::TriggerDataIndices& trigger_data_indices,
    TriggerDataMatching trigger_data_matching) {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      return true;
    case TriggerDataMatching::kModulus: {
      uint32_t i = 0;
      for (const auto& [trigger_data, index] : trigger_data_indices) {
        if (trigger_data != i) {
          return false;
        }
        ++i;
      }
      return true;
    }
  }
}

}  // namespace

TriggerConfig::TriggerConfig() = default;

TriggerConfig::TriggerConfig(TriggerDataMatching trigger_data_matching)
    : trigger_data_matching_(trigger_data_matching) {}

TriggerConfig::~TriggerConfig() = default;

TriggerConfig::TriggerConfig(const TriggerConfig&) = default;

TriggerConfig& TriggerConfig::operator=(const TriggerConfig&) = default;

TriggerConfig::TriggerConfig(TriggerConfig&&) = default;

TriggerConfig& TriggerConfig::operator=(TriggerConfig&&) = default;

// static
base::expected<TriggerConfig, SourceRegistrationError> TriggerConfig::Parse(
    const base::Value::Dict& dict) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionReportingTriggerConfig)) {
    return TriggerConfig();
  }

  TriggerConfig config;
  if (const base::Value* value = dict.Find(kTriggerDataMatching)) {
    ASSIGN_OR_RETURN(config.trigger_data_matching_,
                     ParseTriggerDataMatching(*value));
  }

  return config;
}

void TriggerConfig::Serialize(base::Value::Dict& dict) const {
  if (base::FeatureList::IsEnabled(
          features::kAttributionReportingTriggerConfig)) {
    SerializeTriggerConfig(*this, dict);
  }
}

void TriggerConfig::SerializeForTesting(base::Value::Dict& dict) const {
  SerializeTriggerConfig(*this, dict);
}

TriggerSpec::TriggerSpec(EventReportWindows event_report_windows)
    : event_report_windows_(std::move(event_report_windows)) {}

TriggerSpec::~TriggerSpec() = default;

TriggerSpec::TriggerSpec(const TriggerSpec&) = default;

TriggerSpec& TriggerSpec::operator=(const TriggerSpec&) = default;

TriggerSpec::TriggerSpec(TriggerSpec&&) = default;

TriggerSpec& TriggerSpec::operator=(TriggerSpec&&) = default;

base::Value::Dict TriggerSpec::ToJson() const {
  base::Value::Dict dict;
  event_report_windows_.Serialize(dict);
  return dict;
}

// static
base::expected<TriggerSpecs, SourceRegistrationError> TriggerSpecs::Parse(
    const base::Value::Dict& registration,
    SourceType source_type,
    base::TimeDelta expiry,
    EventReportWindows default_report_windows,
    TriggerDataMatching trigger_data_matching) {
  const base::Value* value = registration.Find(kTriggerSpecs);

  if (!base::FeatureList::IsEnabled(
          features::kAttributionReportingTriggerConfig) ||
      !value) {
    return Default(source_type, std::move(default_report_windows));
  }

  const base::Value::List* list = value->GetIfList();
  if (!list) {
    return base::unexpected(SourceRegistrationError::kTriggerSpecsWrongType);
  }

  if (list->size() > kMaxTriggerDataPerSource) {
    return base::unexpected(SourceRegistrationError::kExcessiveTriggerData);
  }

  TriggerDataIndices trigger_data_indices;

  std::vector<TriggerSpec> specs;
  specs.reserve(list->size());

  for (const base::Value& item : *list) {
    const base::Value::Dict* dict = item.GetIfDict();
    if (!dict) {
      return base::unexpected(SourceRegistrationError::kTriggerSpecWrongType);
    }

    if (absl::optional<SourceRegistrationError> error = ParseTriggerData(
            *dict, trigger_data_indices,
            /*trigger_data_index=*/base::checked_cast<uint8_t>(specs.size()))) {
      return base::unexpected(*error);
    }

    ASSIGN_OR_RETURN(auto event_report_windows,
                     EventReportWindows::ParseWindows(*dict, expiry,
                                                      default_report_windows));

    specs.emplace_back(std::move(event_report_windows));
  }

  if (!AreSpecsValidForTriggerDataMatching(trigger_data_indices,
                                           trigger_data_matching)) {
    return base::unexpected(
        SourceRegistrationError::kInvalidTriggerDataForMatchingMode);
  }

  return TriggerSpecs(std::move(trigger_data_indices), std::move(specs));
}

// static
TriggerSpecs TriggerSpecs::Default(SourceType source_type,
                                   EventReportWindows event_report_windows) {
  std::vector<TriggerSpec> specs;
  specs.emplace_back(std::move(event_report_windows));

  uint32_t cardinality = DefaultTriggerDataCardinality(source_type);

  TriggerDataIndices::container_type trigger_data_indices;
  trigger_data_indices.reserve(cardinality);

  for (uint32_t i = 0; i < cardinality; ++i) {
    trigger_data_indices.emplace_back(i, 0);
  }

  return TriggerSpecs(
      TriggerDataIndices(base::sorted_unique, std::move(trigger_data_indices)),
      std::move(specs));
}

// static
TriggerSpecs TriggerSpecs::CreateForTesting(
    TriggerDataIndices trigger_data_indices,
    std::vector<TriggerSpec> specs) {
  return TriggerSpecs(std::move(trigger_data_indices), std::move(specs));
}

TriggerSpecs::TriggerSpecs(TriggerDataIndices trigger_data_indices,
                           std::vector<TriggerSpec> specs)
    : trigger_data_indices_(std::move(trigger_data_indices)),
      specs_(std::move(specs)) {
  CHECK(AreSpecsValid(trigger_data_indices_, specs_));
}

TriggerSpecs::TriggerSpecs() = default;

TriggerSpecs::~TriggerSpecs() = default;

TriggerSpecs::TriggerSpecs(const TriggerSpecs&) = default;

TriggerSpecs& TriggerSpecs::operator=(const TriggerSpecs&) = default;

TriggerSpecs::TriggerSpecs(TriggerSpecs&&) = default;

TriggerSpecs& TriggerSpecs::operator=(TriggerSpecs&&) = default;

void TriggerSpecs::Serialize(base::Value::Dict& dict) const {
  base::Value::List spec_list;
  spec_list.reserve(specs_.size());

  for (const auto& spec : specs_) {
    spec_list.Append(spec.ToJson().Set(kTriggerData, base::Value::List()));
  }

  for (const auto& [trigger_data, index] : trigger_data_indices_) {
    base::Value::List* trigger_data_list =
        spec_list[index].GetDict().FindList(kTriggerData);

    if (base::IsValueInRangeForNumericType<int>(trigger_data)) {
      trigger_data_list->Append(static_cast<int>(trigger_data));
    } else {
      // This cast is safe because all `uint32_t` can be represented exactly by
      // `double`.
      trigger_data_list->Append(static_cast<double>(trigger_data));
    }
  }

  dict.Set(kTriggerSpecs, std::move(spec_list));
}

TriggerSpecs::Iterator::Iterator(const TriggerSpecs& specs,
                                 TriggerDataIndices::const_iterator it)
    : specs_(specs), it_(it) {}

}  // namespace attribution_reporting
