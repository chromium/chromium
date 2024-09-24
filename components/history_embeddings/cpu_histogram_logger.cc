// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/cpu_histogram_logger.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/public/browser/browser_child_process_host.h"

namespace {

constexpr base::TimeDelta kSampleInterval = base::Minutes(2);
constexpr int kCpuUsageFactor = 100 * 100;
constexpr int kCpuUsageMin = 1;
constexpr int kCpuUsageMax = 400 * kCpuUsageFactor;
constexpr int kBucketCount = 100;

}  // namespace

namespace history_embeddings {

CpuHistogramLogger::CpuHistogramLogger(
    content::BrowserChildProcessHost* utility_process_host)
    : scoped_query_(
          resource_attribution::QueryBuilder()
              .AddResourceType(resource_attribution::ResourceType::kCPUTime)
              .AddResourceContext(
                  *(resource_attribution::ProcessContext::
                        FromBrowserChildProcessHost(utility_process_host)))
              .CreateScopedQuery()) {
  query_observation_.Observe(&scoped_query_);
  scoped_query_.Start(kSampleInterval);
  // Take an immediate baseline measurement for proportion tracker.
  scoped_query_.QueryOnce();
}

CpuHistogramLogger::~CpuHistogramLogger() = default;

void CpuHistogramLogger::OnResourceUsageUpdated(
    const resource_attribution::QueryResultMap& results) {
  if (!proportion_tracker_started_) {
    proportion_tracker_.StartFirstInterval(base::TimeTicks::Now(), results);
    proportion_tracker_started_ = true;
  } else {
    std::map<resource_attribution::ResourceContext, double> cpu_proportion =
        proportion_tracker_.StartNextInterval(base::TimeTicks::Now(), results);
    CHECK_EQ(results.size(), 1ul);
    CHECK_EQ(cpu_proportion.size(), 1ul);
    base::UmaHistogramCustomCounts(
        "History.Embeddings.Embedder.CpuUsage",
        cpu_proportion.begin()->second * kCpuUsageFactor, kCpuUsageMin,
        kCpuUsageMax, kBucketCount);
  }
}

}  // namespace history_embeddings
