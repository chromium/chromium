// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/attribution_scopes_set.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
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

base::expected<AttributionScopesSet, AttributionScopesError>
ScopesFromJSON(base::Value* v, size_t max_string_size, size_t max_set_size) {
  if (!v) {
    return AttributionScopesSet();
  }

  base::Value::List* list = v->GetIfList();
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

}  // namespace

// static
base::expected<AttributionScopesSet, SourceRegistrationError>
AttributionScopesSet::FromJSON(
    base::Value::Dict& reg,
    std::optional<uint32_t> attribution_scope_limit) {
  if (!base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    return AttributionScopesSet();
  }

  const size_t max_set_size =
      attribution_scope_limit.has_value()
          ? std::min(kMaxScopesPerSource,
                     static_cast<size_t>(*attribution_scope_limit))
          : 0;

  ASSIGN_OR_RETURN(
      AttributionScopesSet scopes_set,
      ScopesFromJSON(reg.Find(kAttributionScopes),
                     kMaxLengthPerAttributionScope, max_set_size)
          .transform_error([&](AttributionScopesError error) {
            switch (error) {
              case AttributionScopesError::kListWrongType:
                return SourceRegistrationError::kAttributionScopesInvalid;
              case AttributionScopesError::kSetTooLong:
                return attribution_scope_limit.has_value()
                           ? SourceRegistrationError::kAttributionScopesInvalid
                           : SourceRegistrationError::
                                 kAttributionScopeLimitRequired;
              case AttributionScopesError::kScopeWrongType:
              case AttributionScopesError::kScopeTooLong:
                return SourceRegistrationError::kAttributionScopesValueInvalid;
            }
          }));

  if (attribution_scope_limit.has_value() && scopes_set.scopes().empty()) {
    return base::unexpected(SourceRegistrationError::kAttributionScopesInvalid);
  }
  return scopes_set;
}

// static
base::expected<AttributionScopesSet, TriggerRegistrationError>
AttributionScopesSet::FromJSON(base::Value::Dict& reg) {
  if (!base::FeatureList::IsEnabled(features::kAttributionScopes)) {
    return AttributionScopesSet();
  }
  return ScopesFromJSON(reg.Find(kAttributionScopes),
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
            NOTREACHED_NORETURN();
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

void AttributionScopesSet::Serialize(base::Value::Dict& dict) const {
  if (!base::FeatureList::IsEnabled(features::kAttributionScopes) ||
      scopes_.empty()) {
    return;
  }
  auto list = base::Value::List::with_capacity(scopes_.size());
  for (const auto& scope : scopes_) {
    list.Append(scope);
  }
  dict.Set(kAttributionScopes, std::move(list));
}

}  // namespace attribution_reporting
