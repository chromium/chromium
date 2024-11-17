// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_named_budget_defs.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool AggregatableNamedBudgetKeyHasValidLength(const std::string& key) {
  return key.size() <= kMaxLengthPerAggregatableNamedBudgetName;
}

bool IsValid(const AggregatableNamedBudgetDefs::BudgetMap& budgets) {
  return budgets.size() <= kMaxAggregatableNamedBudgetsPerSource &&
         base::ranges::all_of(budgets, [](const auto& budget) {
           return AggregatableNamedBudgetKeyHasValidLength(budget.first) &&
                  IsAggregatableBudgetInRange(budget.second);
         });
}

}  // namespace

// static
base::expected<AggregatableNamedBudgetDefs, SourceRegistrationError>
AggregatableNamedBudgetDefs::FromJSON(const base::Value* v) {
  if (!base::FeatureList::IsEnabled(
          features::kAttributionAggregatableNamedBudgets) ||
      !v) {
    return AggregatableNamedBudgetDefs();
  }

  const base::Value::Dict* dict = v->GetIfDict();
  if (!dict) {
    return base::unexpected(
        SourceRegistrationError::kAggregatableNamedBudgetsDictInvalid);
  }

  const size_t num_keys = dict->size();

  if (num_keys > kMaxAggregatableNamedBudgetsPerSource) {
    return base::unexpected(
        SourceRegistrationError::kAggregatableNamedBudgetsDictInvalid);
  }

  BudgetMap::container_type budgets;
  budgets.reserve(num_keys);

  for (auto [key, value] : *dict) {
    if (!AggregatableNamedBudgetKeyHasValidLength(key)) {
      return base::unexpected(
          SourceRegistrationError::kAggregatableNamedBudgetsKeyTooLong);
    }

    ASSIGN_OR_RETURN(int budget, ParseInt(value), [](ParseError) {
      return SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid;
    });

    if (!IsAggregatableBudgetInRange(budget)) {
      return base::unexpected(
          SourceRegistrationError::kAggregatableNamedBudgetsValueInvalid);
    }

    budgets.emplace_back(key, budget);
  }

  return AggregatableNamedBudgetDefs(
      BudgetMap(base::sorted_unique, std::move(budgets)));
}

// static
std::optional<AggregatableNamedBudgetDefs>
AggregatableNamedBudgetDefs::FromBudgetMap(BudgetMap budgets) {
  if (!IsValid(budgets)) {
    return std::nullopt;
  }

  return AggregatableNamedBudgetDefs(std::move(budgets));
}

AggregatableNamedBudgetDefs::AggregatableNamedBudgetDefs(
    AggregatableNamedBudgetDefs::BudgetMap budgets)
    : budgets_(std::move(budgets)) {
  CHECK(IsValid(budgets_));
}

AggregatableNamedBudgetDefs::AggregatableNamedBudgetDefs() = default;

AggregatableNamedBudgetDefs::~AggregatableNamedBudgetDefs() = default;

AggregatableNamedBudgetDefs::AggregatableNamedBudgetDefs(
    const AggregatableNamedBudgetDefs&) = default;

AggregatableNamedBudgetDefs::AggregatableNamedBudgetDefs(
    AggregatableNamedBudgetDefs&&) = default;

AggregatableNamedBudgetDefs& AggregatableNamedBudgetDefs::operator=(
    const AggregatableNamedBudgetDefs&) = default;

AggregatableNamedBudgetDefs& AggregatableNamedBudgetDefs::operator=(
    AggregatableNamedBudgetDefs&&) = default;

void AggregatableNamedBudgetDefs::Serialize(base::Value::Dict& dict) const {
  if (budgets_.empty()) {
    return;
  }

  base::Value::Dict budget_dict;
  for (const auto& [key, value] : budgets_) {
    budget_dict.Set(key, value);
  }

  dict.Set(kAggregatableNamedBudgets, std::move(budget_dict));
}

}  // namespace attribution_reporting
