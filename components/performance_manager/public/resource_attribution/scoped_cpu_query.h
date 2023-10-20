// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"

namespace performance_manager {
class Graph;
}

namespace performance_manager::resource_attribution {

class QueryScheduler;

// A temporary public interface to request CPU measurements. As soon as a
// ScopedCPUQuery instance is created, CPUMeasurementMonitor will begin
// monitoring CPU usage. When no more instances exist, it will stop.
//
// TODO(crbug.com/1471683): Replace this with the full Resource Attribution
// query API described in bit.ly/resource-attribution-api.
class ScopedCPUQuery {
 public:
  explicit ScopedCPUQuery(Graph* graph);
  ~ScopedCPUQuery();

  ScopedCPUQuery(const ScopedCPUQuery&) = delete;
  ScopedCPUQuery& operator=(const ScopedCPUQuery&) = delete;

  // Requests the current CPU measurements.
  QueryResultMap QueryOnce();

 private:
  base::WeakPtr<QueryScheduler> scheduler_;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_
