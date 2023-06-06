// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/metrics/content/gpu_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "content/public/browser/background_tracing_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {

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
                         snapshot_manager, std::move(done_callback));
}

void BackgroundTracingMetricsProvider::ProvideEmbedderMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto,
    std::string&& serialized_trace,
    metrics::TraceLog* log,
    base::HistogramSnapshotManager* snapshot_manager,
    base::OnceCallback<void(bool)> done_callback) {
  SetTrace(log, std::move(serialized_trace));
  // TODO(crbug/1052796): Remove the UMA timer code, which is currently used to
  // determine if it is worth to finalize independent logs in the background
  // by measuring the time it takes to execute the callback
  // MetricsService::PrepareProviderMetricsLogDone().
  SCOPED_UMA_HISTOGRAM_TIMER(
      "UMA.IndependentLog.BackgroundTracingMetricsProvider.FinalizeTime");
  std::move(done_callback).Run(true);
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
