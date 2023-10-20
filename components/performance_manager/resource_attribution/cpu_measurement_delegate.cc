// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"

#include <utility>

#include "base/check.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"

namespace performance_manager::resource_attribution {

void CPUMeasurementDelegate::SetDelegateFactoryForTesting(
    Graph* graph,
    FactoryCallback factory_callback) {
  auto* scheduler = QueryScheduler::GetFromGraph(graph);
  CHECK(scheduler);
  scheduler
      ->GetCPUMonitorForTesting()                   // IN-TEST
      .SetCPUMeasurementDelegateFactoryForTesting(  // IN-TEST
          std::move(factory_callback));
}

}  // namespace performance_manager::resource_attribution
