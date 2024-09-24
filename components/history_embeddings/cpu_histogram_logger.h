// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_

#include "base/scoped_observation.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/queries.h"

namespace content {

class BrowserChildProcessHost;

}  // namespace content

namespace history_embeddings {

class CpuHistogramLogger : public resource_attribution::QueryResultObserver {
 public:
  explicit CpuHistogramLogger(
      content::BrowserChildProcessHost* utility_process_host);
  ~CpuHistogramLogger() override;
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

 private:
  resource_attribution::ScopedResourceUsageQuery scoped_query_;
  resource_attribution::ScopedQueryObservation query_observation_{this};
  resource_attribution::CPUProportionTracker proportion_tracker_;
  bool proportion_tracker_started_ = false;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_CPU_HISTOGRAM_LOGGER_H_
