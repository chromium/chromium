// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_CANDIDATE_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_CANDIDATE_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-forward.h"

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) AggregatableNamedBudgetCandidate {
 public:
  static base::expected<AggregatableNamedBudgetCandidate,
                        mojom::TriggerRegistrationError>
  FromJSON(base::Value&);

  const std::optional<std::string>& name() const { return name_; }

  const FilterPair& filters() const { return filters_; }

  AggregatableNamedBudgetCandidate(std::optional<std::string> name, FilterPair);

  AggregatableNamedBudgetCandidate();
  ~AggregatableNamedBudgetCandidate();

  AggregatableNamedBudgetCandidate(const AggregatableNamedBudgetCandidate&);
  AggregatableNamedBudgetCandidate(AggregatableNamedBudgetCandidate&&);

  AggregatableNamedBudgetCandidate& operator=(
      const AggregatableNamedBudgetCandidate&);
  AggregatableNamedBudgetCandidate& operator=(
      AggregatableNamedBudgetCandidate&&);

  base::Value::Dict ToJson() const;

  friend bool operator==(const AggregatableNamedBudgetCandidate&,
                         const AggregatableNamedBudgetCandidate&) = default;

 private:
  std::optional<std::string> name_;
  FilterPair filters_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_NAMED_BUDGET_CANDIDATE_H_
