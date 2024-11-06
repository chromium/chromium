// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_DEFS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_DEFS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableNamedBudgetDefs {
 public:
  using BudgetMap = base::flat_map<std::string, int>;

  // Returns `std::nullopt` if `budgets` is invalid.
  static std::optional<AggregatableNamedBudgetDefs> FromBudgetMap(BudgetMap);

  static base::expected<AggregatableNamedBudgetDefs,
                        mojom::SourceRegistrationError>
  FromJSON(const base::Value*);

  AggregatableNamedBudgetDefs();
  ~AggregatableNamedBudgetDefs();

  AggregatableNamedBudgetDefs(const AggregatableNamedBudgetDefs&);
  AggregatableNamedBudgetDefs(AggregatableNamedBudgetDefs&&);

  AggregatableNamedBudgetDefs& operator=(const AggregatableNamedBudgetDefs&);
  AggregatableNamedBudgetDefs& operator=(AggregatableNamedBudgetDefs&&);

  const BudgetMap& budgets() const { return budgets_; }

  void Serialize(base::Value::Dict& dict) const;

  friend bool operator==(const AggregatableNamedBudgetDefs&,
                         const AggregatableNamedBudgetDefs&) = default;

 private:
  explicit AggregatableNamedBudgetDefs(BudgetMap);

  BudgetMap budgets_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_DEFS_H_
