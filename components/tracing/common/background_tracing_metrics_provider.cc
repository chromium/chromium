// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
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

  base::UmaHistogramCounts100000("Tracing.Background.UploadingTraceSizeInKB",
                                 serialized_trace.size() / 1024);
  metrics::TraceLog* log = uma_proto->add_trace_log();
  log->set_raw_data(std::move(serialized_trace));

  auto* system_profile = uma_proto->mutable_system_profile();

  for (auto& provider : system_profile_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(
        base::TimeTicks::Now(), system_profile);
  }

  ProvideEmbedderMetrics(uma_proto, snapshot_manager);
  std::move(done_callback).Run(true);
}

void BackgroundTracingMetricsProvider::ProvideEmbedderMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {}

}  // namespace tracing
