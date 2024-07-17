// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_data.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool ScopesValid(const AttributionScopesSet& scopes,
                 std::optional<uint32_t> scope_limit) {
  if (scope_limit.has_value()) {
    return !scopes.scopes().empty() && scopes.IsValidForSource(*scope_limit);
  } else {
    return scopes.scopes().empty();
  }
}

bool EventStatesValid(uint32_t max_event_states,
                      std::optional<uint32_t> scope_limit) {
  return max_event_states > 0 &&
         max_event_states <= MaxTriggerStateCardinality() &&
         (scope_limit.has_value() ||
          max_event_states == kDefaultMaxEventStates);
}

bool DataValid(const AttributionScopesSet& scopes_set,
               std::optional<uint32_t> scope_limit,
               uint32_t max_event_states) {
  return (!scope_limit.has_value() || *scope_limit > 0) &&
         ScopesValid(scopes_set, scope_limit) &&
         EventStatesValid(max_event_states, scope_limit);
}

}  // namespace

// static
std::optional<AttributionScopesData> AttributionScopesData::Create(
    AttributionScopesSet attribution_scopes_set,
    std::optional<uint32_t> attribution_scope_limit,
    uint32_t max_event_states) {
  if (!DataValid(attribution_scopes_set, attribution_scope_limit,
                 max_event_states)) {
    return std::nullopt;
  }

  return AttributionScopesData(std::move(attribution_scopes_set),
                               attribution_scope_limit, max_event_states);
}

// static
base::expected<AttributionScopesData, SourceRegistrationError>
AttributionScopesData::FromJSON(base::Value::Dict& registration) {
  if (!base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    return AttributionScopesData();
  }

  std::optional<uint32_t> attribution_scope_limit;
  if (const base::Value* attribution_scope_limit_value =
          registration.Find(kAttributionScopeLimit)) {
    ASSIGN_OR_RETURN(
        attribution_scope_limit,
        ParsePositiveUint32(*attribution_scope_limit_value), [](ParseError) {
          return SourceRegistrationError::kAttributionScopeLimitInvalid;
        });
  }

  uint32_t max_event_states = kDefaultMaxEventStates;
  if (const base::Value* event_states_value =
          registration.Find(kMaxEventStates)) {
    ASSIGN_OR_RETURN(max_event_states, ParsePositiveUint32(*event_states_value),
                     [](ParseError) {
                       return SourceRegistrationError::kMaxEventStatesInvalid;
                     });
    if (max_event_states > MaxTriggerStateCardinality()) {
      return base::unexpected(SourceRegistrationError::kMaxEventStatesInvalid);
    }
    if (max_event_states != kDefaultMaxEventStates &&
        !attribution_scope_limit.has_value()) {
      return base::unexpected(
          SourceRegistrationError::kAttributionScopeLimitRequired);
    }
  }

  ASSIGN_OR_RETURN(
      AttributionScopesSet attribution_scopes,
      AttributionScopesSet::FromJSON(registration, attribution_scope_limit));
  return AttributionScopesData(std::move(attribution_scopes),
                               attribution_scope_limit, max_event_states);
}

AttributionScopesData::AttributionScopesData(
    AttributionScopesSet scopes,
    std::optional<uint32_t> attribution_scope_limit,
    uint32_t max_event_states)
    : attribution_scopes_set_(std::move(scopes)),
      attribution_scope_limit_(attribution_scope_limit),
      max_event_states_(max_event_states) {
  CHECK(DataValid(attribution_scopes_set_, attribution_scope_limit_,
                  max_event_states_));
}

AttributionScopesData::AttributionScopesData() = default;

AttributionScopesData::~AttributionScopesData() = default;

AttributionScopesData::AttributionScopesData(const AttributionScopesData&) =
    default;

AttributionScopesData::AttributionScopesData(AttributionScopesData&&) = default;

AttributionScopesData& AttributionScopesData::operator=(
    const AttributionScopesData&) = default;

AttributionScopesData& AttributionScopesData::operator=(
    AttributionScopesData&&) = default;

void AttributionScopesData::Serialize(base::Value::Dict& dict) const {
  if (!base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    return;
  }
  if (attribution_scope_limit_.has_value()) {
    dict.Set(kAttributionScopeLimit,
             Uint32ToJson(attribution_scope_limit_.value()));
  }

  dict.Set(kMaxEventStates, Uint32ToJson(max_event_states_));

  attribution_scopes_set_.Serialize(dict);
}

}  // namespace attribution_reporting
