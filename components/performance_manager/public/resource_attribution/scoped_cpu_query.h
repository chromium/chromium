// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"

namespace performance_manager::resource_attribution {

// A temporary public interface to request CPU measurements. As soon as a
// ScopedCPUQuery instance is created, CPUMeasurementMonitor will begin
// monitoring CPU usage. When no more instances exist, it will stop.
//
// TODO(crbug.com/1471683): Replace this with the full Resource Attribution
// query API described in bit.ly/resource-attribution-api.
class ScopedCPUQuery final : public QueryResultObserver {
 public:
  using ResultCallback = base::OnceCallback<void(const QueryResultMap&)>;

  ScopedCPUQuery();
  ~ScopedCPUQuery() final;

  ScopedCPUQuery(const ScopedCPUQuery&) = delete;
  ScopedCPUQuery& operator=(const ScopedCPUQuery&) = delete;

  // Requests the current CPU measurements to be passed to `callback`.
  void QueryOnce(ResultCallback callback);

  // QueryResultObserver:
  void OnResourceUsageUpdated(const QueryResultMap& results) final;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  ScopedResourceUsageQuery wrapped_query_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<ResultCallback> callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_SCOPED_CPU_QUERY_H_
