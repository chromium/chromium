// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_UTILS_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/function_ref.h"
#include "base/time/time.h"

namespace attribution_reporting {

class AggregatableTriggerConfig;

struct NullAggregatableReport {
  base::Time fake_source_time;

  friend bool operator==(const NullAggregatableReport&,
                         const NullAggregatableReport&) = default;
};

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::Time RoundDownToWholeDaySinceUnixEpoch(base::Time);

using GenerateNullAggregatableReportFunc =
    base::FunctionRef<bool(int lookback_day)>;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::vector<NullAggregatableReport> GetNullAggregatableReports(
    const AggregatableTriggerConfig&,
    base::Time trigger_time,
    std::optional<base::Time> attributed_source_time,
    GenerateNullAggregatableReportFunc);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool IsAggregatableValueInRange(int value);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool IsRemainingAggregatableBudgetInRange(int budget);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_AGGREGATABLE_UTILS_H_
