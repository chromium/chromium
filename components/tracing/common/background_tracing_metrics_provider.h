// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_METRICS_PROVIDER_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_METRICS_PROVIDER_H_

#include <vector>

#include "components/metrics/metrics_provider.h"
#include "components/tracing/tracing_export.h"
#include "third_party/metrics_proto/trace_log.pb.h"

namespace tracing {

// Provides trace log metrics collected using BackgroundTracingManager to UMA
// proto. Background tracing uploads metrics of larger size compared to UMA
// histograms and it is better to upload them as independent metrics rather
// than part of UMA histograms log. The background tracing manager will make
// sure the traces are small when uploading over data.
class TRACING_EXPORT BackgroundTracingMetricsProvider
    : public metrics::MetricsProvider {
 public:
  BackgroundTracingMetricsProvider();

  BackgroundTracingMetricsProvider(const BackgroundTracingMetricsProvider&) =
      delete;
  BackgroundTracingMetricsProvider& operator=(
      const BackgroundTracingMetricsProvider&) = delete;

  ~BackgroundTracingMetricsProvider() override;

  // metrics::MetricsProvider:
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(
      base::OnceCallback<void(bool)> done_callback,
      metrics::ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) override;
  void Init() override;

 protected:
  // Embedders can override this to do any additional processing of the log
  // before it is sent. This includes processing of the trace itself (e.g.
  // compression).
  virtual void ProvideEmbedderMetrics(
      metrics::ChromeUserMetricsExtension* uma_proto,
      std::string&& serialized_trace,
      metrics::TraceLog* log,
      base::HistogramSnapshotManager* snapshot_manager,
      base::OnceCallback<void(bool)> done_callback);

  // Writes |serialized_trace| into |logs|'s |raw_data| field.
  static void SetTrace(metrics::TraceLog* log, std::string&& serialized_trace);

  std::vector<std::unique_ptr<metrics::MetricsProvider>>
      system_profile_providers_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_METRICS_PROVIDER_H_
