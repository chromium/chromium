// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/cast_browser_metrics.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/browser/metrics/cast_stability_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"

#if defined(OS_LINUX)
#include "chromecast/browser/metrics/external_metrics.h"
#endif  // defined(OS_LINUX)

#if defined(OS_ANDROID)
#include "chromecast/base/android/dumpstate_writer.h"
#endif

namespace chromecast {
namespace metrics {

const int kMetricsFetchTimeoutSeconds = 60;

#if defined(OS_LINUX)
const char kExternalUmaEventsRelativePath[] = "metrics/uma-events";
const char kPlatformUmaEventsPath[] = "/data/share/chrome/metrics/uma-events";
#endif  // defined(OS_LINUX)

CastBrowserMetrics::CastBrowserMetrics(
    std::unique_ptr<CastMetricsServiceClient> metrics_service_client) {
  metrics_service_client_ = std::move(metrics_service_client);
  metrics_service_client_->SetCallbacks(
      base::BindRepeating(&CastBrowserMetrics::CollectFinalMetricsForLog,
                          base::Unretained(this)),
      base::BindRepeating(&CastBrowserMetrics::ProcessExternalEvents,
                          base::Unretained(this)));
}

CastBrowserMetrics::~CastBrowserMetrics() {
#if defined(OS_LINUX)
  DCHECK(!external_metrics_);
  DCHECK(!platform_metrics_);
#endif  // defined(OS_LINUX)
}

void CastBrowserMetrics::Initialize() {
  metrics_service_client_->InitializeMetricsService();

  auto* metrics_service = metrics_service_client_->GetMetricsService();
  auto stability_provider_unique_ptr =
      std::make_unique<CastStabilityMetricsProvider>(
          metrics_service, metrics_service_client_->pref_service());
#if defined(OS_LINUX)
  auto* stability_provider = stability_provider_unique_ptr.get();
#endif  // defined(OS_LINUX)
  metrics_service->RegisterMetricsProvider(
      std::move(stability_provider_unique_ptr));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableGpu)) {
    metrics_service->RegisterMetricsProvider(
        std::make_unique<::metrics::GPUMetricsProvider>());

    // TODO(gfhuang): Does ChromeCast actually need metrics about screen info?
    // crbug.com/541577
    metrics_service->RegisterMetricsProvider(
        std::make_unique<::metrics::ScreenInfoMetricsProvider>());
  }

  metrics_service->RegisterMetricsProvider(
      std::make_unique<::metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));

  metrics_service_client_->StartMetricsService();

#if defined(OS_LINUX)
  // Start external metrics collection, which feeds data from external
  // processes into the main external metrics.
  external_metrics_ = new ExternalMetrics(
      stability_provider,
      GetHomePathASCII(kExternalUmaEventsRelativePath).value());
  external_metrics_->Start();
  platform_metrics_ =
      new ExternalMetrics(stability_provider, kPlatformUmaEventsPath);
  platform_metrics_->Start();
#endif  // defined(OS_LINUX)
}

void CastBrowserMetrics::Finalize() {
#if !defined(OS_ANDROID)
  // Set clean_shutdown bit.
  metrics_service_client_->GetMetricsService()->RecordCompletedSessionEnd();
#endif  // !defined(OS_ANDROID)

#if defined(OS_LINUX)
  // Stop metrics service cleanly before destructing CastMetricsServiceClient.
  // The pointer will be deleted in StopAndDestroy().
  external_metrics_->StopAndDestroy();
  external_metrics_ = nullptr;
  platform_metrics_->StopAndDestroy();
  platform_metrics_ = nullptr;
#endif  // defined(OS_LINUX)

  metrics_service_client_->Finalize();
}

void CastBrowserMetrics::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  // Asynchronously fetch metrics data from child processes. Since this method
  // is called on log upload, metrics that occur between log upload and child
  // process termination will not be uploaded.
  content::FetchHistogramsAsynchronously(
      base::ThreadTaskRunnerHandle::Get(), done_callback,
      base::TimeDelta::FromSeconds(kMetricsFetchTimeoutSeconds));
}

void CastBrowserMetrics::ProcessExternalEvents(const base::Closure& cb) {
#if defined(OS_LINUX)
  external_metrics_->ProcessExternalEvents(
      base::Bind(&ExternalMetrics::ProcessExternalEvents,
                 base::Unretained(platform_metrics_), cb));
#else
  cb.Run();
#endif  // defined(OS_LINUX)
}

}  // namespace metrics
}  // namespace chromecast
