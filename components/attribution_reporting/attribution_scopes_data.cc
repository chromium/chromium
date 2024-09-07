// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_data.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/attribution_scopes_set.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool ScopesValid(const AttributionScopesSet& scopes, uint32_t scope_limit) {
  return !scopes.scopes().empty() && scopes.IsValidForSource(scope_limit);
}

bool EventStatesValid(uint32_t max_event_states) {
  return max_event_states > 0 &&
         max_event_states <= MaxTriggerStateCardinality();
}

bool DataValid(const AttributionScopesSet& scopes_set,
               uint32_t scope_limit,
               uint32_t max_event_states) {
  return scope_limit > 0 && ScopesValid(scopes_set, scope_limit) &&
         EventStatesValid(max_event_states);
}

}  // namespace

// static
std::optional<AttributionScopesData> AttributionScopesData::Create(
    AttributionScopesSet attribution_scopes_set,
    uint32_t attribution_scope_limit,
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
AttributionScopesData::FromJSON(base::Value& v) {
  base::Value::Dict* scopes_dict = v.GetIfDict();
  if (!scopes_dict) {
    return base::unexpected(SourceRegistrationError::kAttributionScopesInvalid);
  }

  uint32_t attribution_scope_limit;
  if (const base::Value* attribution_scope_limit_value =
          scopes_dict->Find(kLimit)) {
    ASSIGN_OR_RETURN(
        attribution_scope_limit,
        ParsePositiveUint32(*attribution_scope_limit_value), [](ParseError) {
          return SourceRegistrationError::kAttributionScopeLimitInvalid;
        });
  } else {
    return base::unexpected(
        SourceRegistrationError::kAttributionScopeLimitRequired);
  }

  uint32_t max_event_states = kDefaultMaxEventStates;
  if (const base::Value* event_states_value =
          scopes_dict->Find(kMaxEventStates)) {
    ASSIGN_OR_RETURN(max_event_states, ParsePositiveUint32(*event_states_value),
                     [](ParseError) {
                       return SourceRegistrationError::kMaxEventStatesInvalid;
                     });
    if (max_event_states > MaxTriggerStateCardinality()) {
      return base::unexpected(SourceRegistrationError::kMaxEventStatesInvalid);
    }
  }

  ASSIGN_OR_RETURN(
      AttributionScopesSet attribution_scopes,
      AttributionScopesSet::FromJSON(*scopes_dict, attribution_scope_limit));
  return AttributionScopesData(std::move(attribution_scopes),
                               attribution_scope_limit, max_event_states);
}

AttributionScopesData::AttributionScopesData(AttributionScopesSet scopes,
                                             uint32_t attribution_scope_limit,
                                             uint32_t max_event_states)
    : attribution_scopes_set_(std::move(scopes)),
      attribution_scope_limit_(attribution_scope_limit),
      max_event_states_(max_event_states) {
  CHECK(DataValid(attribution_scopes_set_, attribution_scope_limit_,
                  max_event_states_));
}

AttributionScopesData::AttributionScopesData(mojo::DefaultConstruct::Tag) {
  CHECK(!DataValid(attribution_scopes_set_, attribution_scope_limit_,
                   max_event_states_));
}

AttributionScopesData::~AttributionScopesData() = default;

AttributionScopesData::AttributionScopesData(const AttributionScopesData&) =
    default;

AttributionScopesData::AttributionScopesData(AttributionScopesData&&) = default;

AttributionScopesData& AttributionScopesData::operator=(
    const AttributionScopesData&) = default;

AttributionScopesData& AttributionScopesData::operator=(
    AttributionScopesData&&) = default;

base::Value::Dict AttributionScopesData::ToJson() const {
  base::Value::Dict dict;

  dict.Set(kLimit, Uint32ToJson(attribution_scope_limit_));
  dict.Set(kMaxEventStates, Uint32ToJson(max_event_states_));
  attribution_scopes_set_.SerializeForSource(dict);

  return dict;
}

}  // namespace attribution_reporting
