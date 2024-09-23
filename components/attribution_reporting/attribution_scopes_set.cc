// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_set.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;
using ::attribution_reporting::mojom::TriggerRegistrationError;

enum class AttributionScopesError {
  kListWrongType,
  kScopeWrongType,
  kSetTooLong,
  kScopeTooLong,
};

base::expected<AttributionScopesSet, AttributionScopesError> ScopesFromJSON(
    base::Value::List* list,
    size_t max_string_size,
    size_t max_set_size) {
  if (!list) {
    return base::unexpected(AttributionScopesError::kListWrongType);
  }

  ASSIGN_OR_RETURN(
      base::flat_set<std::string> attribution_scopes,
      ExtractStringSet(std::move(*list), max_string_size, max_set_size)
          .transform_error([](StringSetError error) {
            switch (error) {
              case StringSetError::kSetTooLong:
                return AttributionScopesError::kSetTooLong;
              case StringSetError::kWrongType:
                return AttributionScopesError::kScopeWrongType;
              case StringSetError::kStringTooLong:
                return AttributionScopesError::kScopeTooLong;
            }
          }));

  return AttributionScopesSet(std::move(attribution_scopes));
}

void Serialize(const base::flat_set<std::string>& scopes,
               std::string_view key,
               base::Value::Dict& dict) {
  if (scopes.empty()) {
    return;
  }
  auto list = base::Value::List::with_capacity(scopes.size());
  for (const auto& scope : scopes) {
    list.Append(scope);
  }
  dict.Set(key, std::move(list));
}

}  // namespace

// static
base::expected<AttributionScopesSet, SourceRegistrationError>
AttributionScopesSet::FromJSON(base::Value::Dict& reg,
                               uint32_t attribution_scope_limit) {
  base::Value* scopes_value = reg.Find(kValues);
  if (!scopes_value) {
    return base::unexpected(
        SourceRegistrationError::kAttributionScopesListInvalid);
  }

  base::Value::List* scopes_list = scopes_value->GetIfList();
  if (!scopes_list || scopes_list->empty()) {
    return base::unexpected(
        SourceRegistrationError::kAttributionScopesListInvalid);
  }

  const size_t max_set_size = std::min(
      kMaxScopesPerSource, static_cast<size_t>(attribution_scope_limit));

  return ScopesFromJSON(scopes_list, kMaxLengthPerAttributionScope,
                        max_set_size)
      .transform_error([](AttributionScopesError error) {
        switch (error) {
          case AttributionScopesError::kListWrongType:
          case AttributionScopesError::kSetTooLong:
            return SourceRegistrationError::kAttributionScopesListInvalid;
          case AttributionScopesError::kScopeWrongType:
          case AttributionScopesError::kScopeTooLong:
            return SourceRegistrationError::kAttributionScopesListValueInvalid;
        }
      });
}

// static
base::expected<AttributionScopesSet, TriggerRegistrationError>
AttributionScopesSet::FromJSON(base::Value::Dict& reg) {
  base::Value* scopes_value = reg.Find(kAttributionScopes);
  if (!scopes_value) {
    return AttributionScopesSet();
  }

  base::Value::List* scopes_list = scopes_value->GetIfList();
  if (!scopes_list) {
    return base::unexpected(
        TriggerRegistrationError::kAttributionScopesInvalid);
  }

  return ScopesFromJSON(scopes_list,
                        /*max_string_size=*/std::numeric_limits<size_t>::max(),
                        /*max_set_size=*/std::numeric_limits<size_t>::max())
      .transform_error([](AttributionScopesError error) {
        switch (error) {
          case AttributionScopesError::kListWrongType:
            return TriggerRegistrationError::kAttributionScopesInvalid;
          case AttributionScopesError::kScopeWrongType:
            return TriggerRegistrationError::kAttributionScopesValueInvalid;
          case AttributionScopesError::kSetTooLong:
          case AttributionScopesError::kScopeTooLong:
            NOTREACHED();
        }
      });
}

AttributionScopesSet::AttributionScopesSet(Scopes scopes)
    : scopes_(std::move(scopes)) {}

AttributionScopesSet::AttributionScopesSet() = default;

AttributionScopesSet::~AttributionScopesSet() = default;

AttributionScopesSet::AttributionScopesSet(const AttributionScopesSet&) =
    default;

AttributionScopesSet::AttributionScopesSet(AttributionScopesSet&&) = default;

AttributionScopesSet& AttributionScopesSet::operator=(
    const AttributionScopesSet&) = default;

AttributionScopesSet& AttributionScopesSet::operator=(AttributionScopesSet&&) =
    default;

bool AttributionScopesSet::IsValidForSource(uint32_t scope_limit) const {
  CHECK_GT(scope_limit, 0u);
  return scopes_.size() <=
             std::min(kMaxScopesPerSource, static_cast<size_t>(scope_limit)) &&
         base::ranges::all_of(scopes_, [](const std::string& scope) {
           return scope.length() <= kMaxLengthPerAttributionScope;
         });
}

void AttributionScopesSet::SerializeForSource(base::Value::Dict& dict) const {
  Serialize(scopes_, kValues, dict);
}

void AttributionScopesSet::SerializeForTrigger(base::Value::Dict& dict) const {
  Serialize(scopes_, kAttributionScopes, dict);
}

// Rather than retrieving the whole intersection and checking its size using
// `std::set_intersection`, we iterate through and compare each element and
// early exit when two matching elements are found.
bool AttributionScopesSet::HasIntersection(
    const AttributionScopesSet& other_scopes) const {
  const auto& scopes_2 = other_scopes.scopes();
  if (scopes_.empty() || scopes_2.empty()) {
    return false;
  }

  AttributionScopesSet::Scopes::const_iterator it_1 = scopes_.begin(),
                                               it_1_end = scopes_.end();
  AttributionScopesSet::Scopes::const_iterator it_2 = scopes_2.begin(),
                                               it_2_end = scopes_2.end();

  if (*it_1 > *scopes_2.rbegin() || *it_2 > *scopes_.rbegin()) {
    return false;
  }

  while (it_1 != it_1_end && it_2 != it_2_end) {
    if (*it_1 == *it_2) {
      return true;
    }
    if (*it_1 < *it_2) {
      it_1++;
    } else {
      it_2++;
    }
  }
  return false;
}

}  // namespace attribution_reporting
