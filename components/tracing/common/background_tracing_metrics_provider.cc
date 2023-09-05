// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
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
  auto* system_profile = uma_proto->mutable_system_profile();
  for (auto& provider : system_profile_providers_) {
    provider->ProvideSystemProfileMetricsWithLogCreationTime(
        base::TimeTicks::Now(), system_profile);
  }

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto provide_embedder_metrics = GetEmbedderMetricsProvider();
  content::BackgroundTracingManager::GetInstance().GetTraceToUpload(
      base::BindOnce(
          [](base::OnceCallback<bool(metrics::ChromeUserMetricsExtension*,
                                     std::string&&)> provide_embedder_metrics,
             base::OnceClosure serialize_log_callback,
             base::OnceCallback<void(bool)> done_callback,
             metrics::ChromeUserMetricsExtension* uma_proto,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             std::string compressed_trace) {
            if (compressed_trace.empty() ||
                !std::move(provide_embedder_metrics)
                     .Run(uma_proto, std::move(compressed_trace))) {
              task_runner->PostTask(
                  FROM_HERE, base::BindOnce(std::move(done_callback), false));
              return;
            }
            // If |kMetricsServiceAsyncIndependentLogs| is enabled,
            // serialize the log on the background instead of on the
            // main thread.
            if (base::FeatureList::IsEnabled(
                    metrics::features::kMetricsServiceAsyncIndependentLogs)) {
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  {base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                  std::move(serialize_log_callback)
                      .Then(base::BindPostTask(
                          task_runner,
                          base::BindOnce(&OnProvideEmbedderMetrics,
                                         std::move(done_callback)))));
            } else {
              task_runner->PostTask(FROM_HERE,
                                    base::BindOnce(&OnProvideEmbedderMetrics,
                                                   std::move(done_callback)));
            }
          },
          std::move(provide_embedder_metrics),
          std::move(serialize_log_callback), std::move(done_callback),
          uma_proto, task_runner));
}

base::OnceCallback<bool(metrics::ChromeUserMetricsExtension*, std::string&&)>
BackgroundTracingMetricsProvider::GetEmbedderMetricsProvider() {
  return base::BindOnce([](metrics::ChromeUserMetricsExtension* uma_proto,
                           std::string&& compressed_trace) {
    SetTrace(uma_proto->add_trace_log(), std::move(compressed_trace));
    return true;
  });
}

// static
void BackgroundTracingMetricsProvider::SetTrace(
    metrics::TraceLog* log,
    std::string&& compressed_trace) {
  base::UmaHistogramCounts100000("Tracing.Background.UploadingTraceSizeInKB",
                                 compressed_trace.size() / 1024);

  log->set_raw_data(std::move(compressed_trace));
  log->set_compression_type(metrics::TraceLog::COMPRESSION_TYPE_ZLIB);
}

}  // namespace tracing
