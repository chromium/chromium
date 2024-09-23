// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/trigger_config.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_tree.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

constexpr uint32_t DefaultTriggerDataCardinality(SourceType source_type) {
  switch (source_type) {
    case SourceType::kNavigation:
      return 8;
    case SourceType::kEvent:
      return 2;
  }
}

base::expected<void, SourceRegistrationError> ParseTriggerData(
    const base::Value& value,
    TriggerSpecs::TriggerDataIndices& trigger_data_indices,
    const uint8_t spec_index,
    const bool allow_empty,
    SourceRegistrationError list_error,
    SourceRegistrationError duplicate_error,
    SourceRegistrationError excessive_error) {
  const base::Value::List* list = value.GetIfList();
  if (!list) {
    return base::unexpected(list_error);
  }

  if (!allow_empty && list->empty()) {
    return base::unexpected(list_error);
  }

  const size_t new_size = list->size() + trigger_data_indices.size();
  if (new_size > kMaxTriggerDataPerSource) {
    return base::unexpected(excessive_error);
  }

  trigger_data_indices.reserve(new_size);

  for (const base::Value& item : *list) {
    ASSIGN_OR_RETURN(uint32_t trigger_data, ParseUint32(item),
                     [&](ParseError) { return list_error; });

    auto [_, inserted] =
        trigger_data_indices.try_emplace(trigger_data, spec_index);
    if (!inserted) {
      return base::unexpected(duplicate_error);
    }
  }

  return base::ok();
}

bool AreSpecsValid(const TriggerSpecs::TriggerDataIndices& trigger_data_indices,
                   const std::vector<TriggerSpec>& specs) {
  return trigger_data_indices.size() <= kMaxTriggerDataPerSource &&
         base::ranges::all_of(trigger_data_indices, [&specs](const auto& pair) {
           return pair.second < specs.size();
         });
}

base::expected<void, SourceRegistrationError>
ValidateSpecsForTriggerDataMatching(
    const TriggerSpecs::TriggerDataIndices& trigger_data_indices,
    TriggerDataMatching trigger_data_matching) {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      return base::ok();
    case TriggerDataMatching::kModulus:
      for (uint32_t i = 0;
           const auto& [trigger_data, _] : trigger_data_indices) {
        if (trigger_data != i) {
          return base::unexpected(
              SourceRegistrationError::kInvalidTriggerDataForMatchingMode);
        }
        ++i;
      }
      return base::ok();
  }
}

}  // namespace

base::expected<TriggerDataMatching, SourceRegistrationError>
ParseTriggerDataMatching(const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kTriggerDataMatching);
  if (!value) {
    return TriggerDataMatching::kModulus;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingValueInvalid);
  } else if (*str == kTriggerDataMatchingExact) {
    return TriggerDataMatching::kExact;
  } else if (*str == kTriggerDataMatchingModulus) {
    return TriggerDataMatching::kModulus;
  } else {
    return base::unexpected(
        SourceRegistrationError::kTriggerDataMatchingValueInvalid);
  }
}

void Serialize(base::Value::Dict& dict,
               TriggerDataMatching trigger_data_matching) {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      dict.Set(kTriggerDataMatching, kTriggerDataMatchingExact);
      break;
    case TriggerDataMatching::kModulus:
      dict.Set(kTriggerDataMatching, kTriggerDataMatchingModulus);
      break;
  }
}

TriggerSpec::TriggerSpec() = default;

TriggerSpec::TriggerSpec(EventReportWindows event_report_windows)
    : event_report_windows_(std::move(event_report_windows)) {}

TriggerSpec::~TriggerSpec() = default;

TriggerSpec::TriggerSpec(const TriggerSpec&) = default;

TriggerSpec& TriggerSpec::operator=(const TriggerSpec&) = default;

TriggerSpec::TriggerSpec(TriggerSpec&&) = default;

TriggerSpec& TriggerSpec::operator=(TriggerSpec&&) = default;

TriggerSpecs::const_iterator TriggerSpecs::find(
    uint64_t trigger_data,
    TriggerDataMatching trigger_data_matching) const {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      return Iterator(*this, trigger_data_indices_.find(trigger_data));
    case TriggerDataMatching::kModulus:
      // Prevent modulus-by-zero.
      if (trigger_data_indices_.empty()) {
        return end();
      }
      // `std::next()` is constant-time due to the underlying iterator being
      // random-access.
      return Iterator(*this,
                      std::next(trigger_data_indices_.begin(),
                                trigger_data % trigger_data_indices_.size()));
  }
}

base::Value::Dict TriggerSpec::ToJson() const {
  base::Value::Dict dict;
  event_report_windows_.Serialize(dict);
  return dict;
}

// static
base::expected<TriggerSpecs, SourceRegistrationError>
TriggerSpecs::ParseFullFlexForTesting(
    const base::Value::Dict& registration,
    SourceType source_type,
    base::TimeDelta expiry,
    EventReportWindows default_report_windows,
    TriggerDataMatching trigger_data_matching) {
  const base::Value* trigger_specs = registration.Find(kTriggerSpecs);
  if (!trigger_specs) {
    return ParseTopLevelTriggerData(registration, source_type,
                                    std::move(default_report_windows),
                                    trigger_data_matching);
  }

  if (registration.contains(kTriggerData)) {
    return base::unexpected(
        SourceRegistrationError::kTopLevelTriggerDataAndTriggerSpecs);
  }

  ASSIGN_OR_RETURN(auto max_event_level_reports,
                   MaxEventLevelReports::Parse(registration, source_type));

  // TODO(apaseltiner): If `max_event_level_reports` is 0, consider validating
  // but not accumulating the specs below.

  const base::Value::List* list = trigger_specs->GetIfList();
  if (!list) {
    return base::unexpected(SourceRegistrationError::kTriggerSpecsWrongType);
  }

  if (list->size() > kMaxTriggerDataPerSource) {
    return base::unexpected(
        SourceRegistrationError::kTriggerSpecExcessiveTriggerData);
  }

  TriggerDataIndices trigger_data_indices;

  std::vector<TriggerSpec> specs;
  specs.reserve(list->size());

  for (const base::Value& item : *list) {
    const base::Value::Dict* dict = item.GetIfDict();
    if (!dict) {
      return base::unexpected(SourceRegistrationError::kTriggerSpecsWrongType);
    }

    const base::Value* trigger_data = dict->Find(kTriggerData);
    if (!trigger_data) {
      return base::unexpected(
          SourceRegistrationError::kTriggerSpecTriggerDataMissing);
    }

    RETURN_IF_ERROR(ParseTriggerData(
        *trigger_data, trigger_data_indices,
        /*spec_index=*/base::checked_cast<uint8_t>(specs.size()),
        /*allow_empty=*/false,
        SourceRegistrationError::kTriggerSpecTriggerDataListInvalid,
        SourceRegistrationError::kTriggerSpecDuplicateTriggerData,
        SourceRegistrationError::kTriggerSpecExcessiveTriggerData));

    ASSIGN_OR_RETURN(auto event_report_windows,
                     EventReportWindows::ParseWindows(*dict, expiry,
                                                      default_report_windows));

    specs.emplace_back(std::move(event_report_windows));
  }

  RETURN_IF_ERROR(ValidateSpecsForTriggerDataMatching(trigger_data_indices,
                                                      trigger_data_matching));

  return TriggerSpecs(std::move(trigger_data_indices), std::move(specs),
                      max_event_level_reports);
}

// static
base::expected<TriggerSpecs, SourceRegistrationError>
TriggerSpecs::ParseTopLevelTriggerData(
    const base::Value::Dict& registration,
    SourceType source_type,
    EventReportWindows default_report_windows,
    TriggerDataMatching trigger_data_matching) {
  ASSIGN_OR_RETURN(auto max_event_level_reports,
                   MaxEventLevelReports::Parse(registration, source_type));

  // TODO(apaseltiner): If `max_event_level_reports` is 0, consider validating
  // but not accumulating the specs below.

  const base::Value* trigger_data = registration.Find(kTriggerData);
  if (!trigger_data) {
    return TriggerSpecs(source_type, std::move(default_report_windows),
                        max_event_level_reports);
  }

  TriggerDataIndices trigger_data_indices;
  RETURN_IF_ERROR(ParseTriggerData(
      *trigger_data, trigger_data_indices,
      /*spec_index=*/0,
      /*allow_empty=*/true, SourceRegistrationError::kTriggerDataListInvalid,
      SourceRegistrationError::kDuplicateTriggerData,
      SourceRegistrationError::kExcessiveTriggerData));

  RETURN_IF_ERROR(ValidateSpecsForTriggerDataMatching(trigger_data_indices,
                                                      trigger_data_matching));

  std::vector<TriggerSpec> specs;
  if (!trigger_data_indices.empty()) {
    specs.reserve(1);
    specs.emplace_back(std::move(default_report_windows));
  }

  return TriggerSpecs(std::move(trigger_data_indices), std::move(specs),
                      max_event_level_reports);
}

TriggerSpecs::TriggerSpecs(SourceType source_type,
                           EventReportWindows event_report_windows,
                           MaxEventLevelReports max_event_level_reports)
    : max_event_level_reports_(max_event_level_reports) {
  specs_.emplace_back(std::move(event_report_windows));

  uint32_t cardinality = DefaultTriggerDataCardinality(source_type);

  TriggerDataIndices::container_type trigger_data_indices;
  trigger_data_indices.reserve(cardinality);

  for (uint32_t i = 0; i < cardinality; ++i) {
    trigger_data_indices.emplace_back(i, 0);
  }

  trigger_data_indices_.replace(std::move(trigger_data_indices));
}

// static
std::optional<TriggerSpecs> TriggerSpecs::Create(
    TriggerDataIndices trigger_data_indices,
    std::vector<TriggerSpec> specs,
    MaxEventLevelReports max_event_level_reports) {
  if (!AreSpecsValid(trigger_data_indices, specs)) {
    return std::nullopt;
  }
  return TriggerSpecs(std::move(trigger_data_indices), std::move(specs),
                      max_event_level_reports);
}

TriggerSpecs::TriggerSpecs(TriggerDataIndices trigger_data_indices,
                           std::vector<TriggerSpec> specs,
                           MaxEventLevelReports max_event_level_reports)
    : trigger_data_indices_(std::move(trigger_data_indices)),
      specs_(std::move(specs)),
      max_event_level_reports_(max_event_level_reports) {
  CHECK(AreSpecsValid(trigger_data_indices_, specs_));
}

TriggerSpecs::TriggerSpecs() = default;

TriggerSpecs::~TriggerSpecs() = default;

TriggerSpecs::TriggerSpecs(const TriggerSpecs&) = default;

TriggerSpecs& TriggerSpecs::operator=(const TriggerSpecs&) = default;

TriggerSpecs::TriggerSpecs(TriggerSpecs&&) = default;

TriggerSpecs& TriggerSpecs::operator=(TriggerSpecs&&) = default;

const TriggerSpec* TriggerSpecs::SingleSharedSpec() const {
  return specs_.size() == 1 ? &specs_[0] : nullptr;
}

base::Value::List TriggerSpecs::ToJson() const {
  auto spec_list = base::Value::List::with_capacity(specs_.size());

  for (const auto& spec : specs_) {
    spec_list.Append(spec.ToJson().Set(kTriggerData, base::Value::List()));
  }

  for (const auto& [trigger_data, index] : trigger_data_indices_) {
    spec_list[index]
        .GetDict()
        .FindList(kTriggerData)
        ->Append(Uint32ToJson(trigger_data));
  }

  return spec_list;
}

void TriggerSpecs::Serialize(base::Value::Dict& dict) const {
  dict.Set(kTriggerSpecs, ToJson());
  max_event_level_reports_.Serialize(dict);
}

TriggerSpecs::Iterator::Iterator(const TriggerSpecs& specs,
                                 TriggerDataIndices::const_iterator it)
    : specs_(specs), it_(it) {}

}  // namespace attribution_reporting
