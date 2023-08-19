// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/metrics_features.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {

namespace {

void OnProvideEmbedderMetrics(base::OnceCallback<void(bool)> done_callback) {
  // TODO(crbug/1428679): Remove the UMA timer code, which is currently used to
  // determine if it is worth to finalize independent logs in the background
  // by measuring the time it takes to execute the callback
  // MetricsService::PrepareProviderMetricsLogDone().
  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.IndependentLog.BackgroundTracingMetricsProvider.FinalizeTime");
  std::move(done_callback).Run(true);
}

}  // namespace

BackgroundTracingMetricsProvider::BackgroundTracingMetricsProvider() = default;
BackgroundTracingMetricsProvider::~BackgroundTracingMetricsProvider() = default;

void BackgroundTracingMetricsProvider::Init() {
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::CPUMetricsProvider>());
  system_profile_providers_.emplace_back(
      std::make_unique<metrics::GPUMetricsProvider>());
}

bool BackgroundTracingMetricsProvider::HasIndependentMetrics() {
  return content::BackgroundTracingManager::GetInstance().HasTraceToUpload();
}

void BackgroundTracingMetricsProvider::ProvideIndependentMetrics(
    base::OnceClosure serialize_log_callback,
    base::OnceCallback<void(bool)> done_callback,
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  auto serialized_trace =
      content::BackgroundTracingManager::GetInstance().GetLatestTraceToUpload();
  if (serialized_trace.empty()) {
    std::move(done_callback).Run(false);
    return;
  }

  auto* system_profile = uma_proto->mutable_system_profile();

  for (auto& provider : system_profile_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(
        base::TimeTicks::Now(), system_profile);
  }

  metrics::TraceLog* log = uma_proto->add_trace_log();
  ProvideEmbedderMetrics(uma_proto, std::move(serialized_trace), log,
                         snapshot_manager, std::move(serialize_log_callback),
                         std::move(done_callback));
}

void BackgroundTracingMetricsProvider::ProvideEmbedderMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto,
    std::string&& serialized_trace,
    metrics::TraceLog* log,
    base::HistogramSnapshotManager* snapshot_manager,
    base::OnceClosure serialize_log_callback,
    base::OnceCallback<void(bool)> done_callback) {
  // If |kMetricsServiceAsyncIndependentLogs| is enabled, call SetTrace() and
  // serialize the log on the background instead of on the main thread.
  if (base::FeatureList::IsEnabled(
          metrics::features::kMetricsServiceAsyncIndependentLogs)) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         // CONTINUE_ON_SHUTDOWN because the work done is only useful once the
         // reply task is run (and there are no side effects). So, no need to
         // block shutdown since the reply task won't be run anyway.
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&BackgroundTracingMetricsProvider::SetTrace, log,
                       std::move(serialized_trace))
            .Then(std::move(serialize_log_callback)),
        base::BindOnce(&OnProvideEmbedderMetrics, std::move(done_callback)));
  } else {
    SetTrace(log, std::move(serialized_trace));
    OnProvideEmbedderMetrics(std::move(done_callback));
  }
}

// static
void BackgroundTracingMetricsProvider::SetTrace(
    metrics::TraceLog* log,
    std::string&& serialized_trace) {
  base::UmaHistogramCounts100000("Tracing.Background.UploadingTraceSizeInKB",
                                 serialized_trace.size() / 1024);

  log->set_raw_data(std::move(serialized_trace));
}

}  // namespace tracing
