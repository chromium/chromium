// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/cpu_histogram_logger.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "content/public/browser/browser_child_process_host.h"

namespace {

constexpr base::TimeDelta kSampleInterval = base::Seconds(30);
constexpr int kCpuUsageFactor = 100 * 100;
constexpr int kCpuUsageMin = 1;
constexpr int kCpuUsageMax = 400 * kCpuUsageFactor;
constexpr int kBucketCount = 100;

}  // namespace

namespace history_embeddings {

class CpuHistogramLogger::CpuObserver
    : public resource_attribution::QueryResultObserver {
 public:
  explicit CpuObserver(const resource_attribution::ProcessContext& context);
  ~CpuObserver() override;

  CpuObserver(const CpuObserver&) = delete;
  CpuObserver& operator=(const CpuObserver&) = delete;

  void MakeSelfOwned(std::unique_ptr<CpuObserver> self);

  // QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

 private:
  resource_attribution::ScopedResourceUsageQuery scoped_query_;
  resource_attribution::ScopedQueryObservation query_observation_{this};
  resource_attribution::CPUProportionTracker proportion_tracker_;
  bool proportion_tracker_started_ = false;

  std::unique_ptr<CpuObserver> self_;
};

CpuHistogramLogger::CpuObserver::CpuObserver(
    const resource_attribution::ProcessContext& context)
    : scoped_query_(
          resource_attribution::QueryBuilder()
              .AddResourceType(resource_attribution::ResourceType::kCPUTime)
              .AddResourceContext(context)
              .CreateScopedQuery()) {
  query_observation_.Observe(&scoped_query_);
  scoped_query_.Start(kSampleInterval);
  // Take an immediate baseline measurement for proportion tracker.
  scoped_query_.QueryOnce();
}

CpuHistogramLogger::CpuObserver::~CpuObserver() = default;

void CpuHistogramLogger::CpuObserver::MakeSelfOwned(
    std::unique_ptr<CpuObserver> self) {
  CHECK_EQ(self.get(), this);
  CHECK(!self_);
  self_ = std::move(self);
}

void CpuHistogramLogger::CpuObserver::OnResourceUsageUpdated(
    const resource_attribution::QueryResultMap& results) {
  // If MakeSelfOwned() was called, delete `this` on function exit.
  std::unique_ptr<CpuObserver> to_delete = std::move(self_);

  if (results.size() == 0) {
    // Service process couldn't be measured.
    return;
  }

  CHECK_EQ(results.size(), 1ul);

  if (!proportion_tracker_started_) {
    proportion_tracker_.StartFirstInterval(base::TimeTicks::Now(), results);
    proportion_tracker_started_ = true;
  } else {
    std::map<resource_attribution::ResourceContext, double> cpu_proportion =
        proportion_tracker_.StartNextInterval(base::TimeTicks::Now(), results);
    if (cpu_proportion.size() == 0) {
      // When the service process exits close to the measurement time, the
      // proportion tracker might skip the final measurement due to a race.
      return;
    }
    CHECK_EQ(cpu_proportion.size(), 1ul, base::NotFatalUntil::M134);
    base::UmaHistogramCustomCounts(
        "History.Embeddings.Embedder.CpuUsage2",
        cpu_proportion.begin()->second * kCpuUsageFactor, kCpuUsageMin,
        kCpuUsageMax, kBucketCount);
  }
}

CpuHistogramLogger::CpuHistogramLogger() = default;

CpuHistogramLogger::~CpuHistogramLogger() {
  StopLoggingAfterNextUpdate();
  CHECK(!cpu_observer_);
}

void CpuHistogramLogger::StartLogging(
    content::BrowserChildProcessHost* utility_process_host) {
  CHECK(!cpu_observer_);
  auto process_context =
      resource_attribution::ProcessContext::FromBrowserChildProcessHost(
          utility_process_host);
  if (process_context.has_value()) {
    cpu_observer_ = std::make_unique<CpuObserver>(process_context.value());
  }
}

void CpuHistogramLogger::StopLoggingAfterNextUpdate() {
  // The observer will finish the current interval and then delete itself.
  CpuObserver* observer = cpu_observer_.get();
  if (observer) {
    observer->MakeSelfOwned(std::move(cpu_observer_));
  }
}

}  // namespace history_embeddings
