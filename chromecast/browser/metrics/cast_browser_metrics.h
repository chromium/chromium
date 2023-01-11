// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_CAST_BROWSER_METRICS_H_
#define CHROMECAST_BROWSER_METRICS_CAST_BROWSER_METRICS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chromecast/metrics/cast_metrics_service_client.h"

namespace chromecast {
namespace metrics {

class ExternalMetrics;

class CastBrowserMetrics {
 public:
  explicit CastBrowserMetrics(
      std::unique_ptr<CastMetricsServiceClient> metrics_service_client);

  CastBrowserMetrics(const CastBrowserMetrics&) = delete;
  CastBrowserMetrics& operator=(const CastBrowserMetrics&) = delete;

  ~CastBrowserMetrics();
  void Initialize();
  void Finalize();

  // Processes all events from shared file. This should be used to consume all
  // events in the file before shutdown. This function is safe to call from any
  // thread.
  void ProcessExternalEvents(base::OnceClosure cb);
  void CollectFinalMetricsForLog(base::OnceClosure done_callback);

  metrics::CastMetricsServiceClient* metrics_service_client() const {
    return metrics_service_client_.get();
  }

 private:
  std::unique_ptr<CastMetricsServiceClient> metrics_service_client_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ExternalMetrics* external_metrics_ = nullptr;
  ExternalMetrics* platform_metrics_ = nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_CAST_BROWSER_METRICS_H_
