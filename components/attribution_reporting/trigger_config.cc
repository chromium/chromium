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
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
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

base::expected<TriggerDataSet::TriggerData, SourceRegistrationError>
ParseTriggerData(const base::Value& value) {
  const base::Value::List* list = value.GetIfList();
  if (!list) {
    return base::unexpected(SourceRegistrationError::kTriggerDataListInvalid);
  }

  const size_t size = list->size();
  if (size > kMaxTriggerDataPerSource) {
    return base::unexpected(SourceRegistrationError::kExcessiveTriggerData);
  }

  TriggerDataSet::TriggerData trigger_data;
  trigger_data.reserve(size);

  for (const base::Value& item : *list) {
    ASSIGN_OR_RETURN(uint32_t trigger_datum, ParseUint32(item), [](ParseError) {
      return SourceRegistrationError::kTriggerDataListInvalid;
    });

    auto [_, inserted] = trigger_data.insert(trigger_datum);
    if (!inserted) {
      return base::unexpected(SourceRegistrationError::kDuplicateTriggerData);
    }
  }

  return trigger_data;
}

bool IsTriggerDataValid(const TriggerDataSet::TriggerData& trigger_data) {
  return trigger_data.size() <= kMaxTriggerDataPerSource;
}

base::expected<void, SourceRegistrationError>
ValidateTriggerDataForTriggerDataMatching(
    const TriggerDataSet::TriggerData& trigger_data,
    TriggerDataMatching trigger_data_matching) {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      return base::ok();
    case TriggerDataMatching::kModulus:
      for (uint32_t i = 0; const uint32_t trigger_datum : trigger_data) {
        if (trigger_datum != i) {
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

std::optional<uint32_t> TriggerDataSet::find(
    uint64_t trigger_data,
    TriggerDataMatching trigger_data_matching) const {
  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      if (trigger_data_.contains(trigger_data)) {
        return trigger_data;
      }
      return std::nullopt;
    case TriggerDataMatching::kModulus:
      // Prevent modulus-by-zero.
      if (trigger_data_.empty()) {
        return std::nullopt;
      }
      // `std::next()` is constant-time due to the underlying iterator being
      // random-access.
      const uint32_t out = *std::next(trigger_data_.begin(),
                                      trigger_data % trigger_data_.size());
      return out;
  }
}

// static
base::expected<TriggerDataSet, SourceRegistrationError> TriggerDataSet::Parse(
    const base::Value::Dict& registration,
    SourceType source_type,
    TriggerDataMatching trigger_data_matching) {
  const base::Value* trigger_data = registration.Find(kTriggerData);
  if (!trigger_data) {
    return TriggerDataSet(source_type);
  }

  ASSIGN_OR_RETURN(TriggerData trigger_data_set,
                   ParseTriggerData(*trigger_data));

  RETURN_IF_ERROR(ValidateTriggerDataForTriggerDataMatching(
      trigger_data_set, trigger_data_matching));

  return TriggerDataSet(std::move(trigger_data_set));
}

TriggerDataSet::TriggerDataSet(SourceType source_type) {
  uint32_t cardinality = DefaultTriggerDataCardinality(source_type);

  TriggerData::container_type trigger_data;
  trigger_data.reserve(cardinality);

  for (uint32_t i = 0; i < cardinality; ++i) {
    trigger_data.push_back(i);
  }

  trigger_data_.replace(std::move(trigger_data));
}

// static
std::optional<TriggerDataSet> TriggerDataSet::Create(TriggerData trigger_data) {
  if (!IsTriggerDataValid(trigger_data)) {
    return std::nullopt;
  }
  return TriggerDataSet(std::move(trigger_data));
}

TriggerDataSet::TriggerDataSet(TriggerData trigger_data)
    : trigger_data_(std::move(trigger_data)) {
  CHECK(IsTriggerDataValid(trigger_data_));
}

TriggerDataSet::TriggerDataSet() = default;

TriggerDataSet::~TriggerDataSet() = default;

TriggerDataSet::TriggerDataSet(const TriggerDataSet&) = default;

TriggerDataSet& TriggerDataSet::operator=(const TriggerDataSet&) = default;

TriggerDataSet::TriggerDataSet(TriggerDataSet&&) = default;

TriggerDataSet& TriggerDataSet::operator=(TriggerDataSet&&) = default;

base::Value::Dict TriggerDataSet::ToJson() const {
  base::Value::Dict dict;
  Serialize(dict);
  return dict;
}

void TriggerDataSet::Serialize(base::Value::Dict& dict) const {
  auto trigger_data_list =
      base::Value::List::with_capacity(trigger_data_.size());

  for (const uint32_t trigger_data : trigger_data_) {
    trigger_data_list.Append(Uint32ToJson(trigger_data));
  }

  dict.Set(kTriggerData, std::move(trigger_data_list));
}

}  // namespace attribution_reporting
