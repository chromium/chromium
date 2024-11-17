// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_named_budget_candidate.h"

#include <optional>
#include <string>
#include <utility>

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;

}  // namespace

// static
base::expected<AggregatableNamedBudgetCandidate, TriggerRegistrationError>
AggregatableNamedBudgetCandidate::FromJSON(base::Value& v) {
  base::Value::Dict* dict = v.GetIfDict();
  if (!dict) {
    return base::unexpected(
        TriggerRegistrationError::kAggregatableNamedBudgetWrongType);
  }

  std::optional<std::string> name;
  if (base::Value* name_value = dict->Find(kName)) {
    std::string* name_str = name_value->GetIfString();
    if (!name_str) {
      return base::unexpected(
          TriggerRegistrationError::kAggregatableNamedBudgetNameInvalid);
    }
    name = std::move(*name_str);
  }

  ASSIGN_OR_RETURN(FilterPair filters, FilterPair::FromJSON(*dict));

  return AggregatableNamedBudgetCandidate(std::move(name), std::move(filters));
}

AggregatableNamedBudgetCandidate::AggregatableNamedBudgetCandidate(
    std::optional<std::string> name,
    FilterPair filters)
    : name_(std::move(name)), filters_(std::move(filters)) {}

AggregatableNamedBudgetCandidate::AggregatableNamedBudgetCandidate() = default;

AggregatableNamedBudgetCandidate::~AggregatableNamedBudgetCandidate() = default;

AggregatableNamedBudgetCandidate::AggregatableNamedBudgetCandidate(
    const AggregatableNamedBudgetCandidate&) = default;

AggregatableNamedBudgetCandidate::AggregatableNamedBudgetCandidate(
    AggregatableNamedBudgetCandidate&&) = default;

AggregatableNamedBudgetCandidate& AggregatableNamedBudgetCandidate::operator=(
    const AggregatableNamedBudgetCandidate&) = default;

AggregatableNamedBudgetCandidate& AggregatableNamedBudgetCandidate::operator=(
    AggregatableNamedBudgetCandidate&&) = default;

base::Value::Dict AggregatableNamedBudgetCandidate::ToJson() const {
  base::Value::Dict dict;

  if (name_.has_value()) {
    dict.Set(kName, *name_);
  }

  filters_.SerializeIfNotEmpty(dict);

  return dict;
}

}  // namespace attribution_reporting
