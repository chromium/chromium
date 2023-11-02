// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_CAST_STABILITY_METRICS_PROVIDER_H_
#define CHROMECAST_BROWSER_METRICS_CAST_STABILITY_METRICS_PROVIDER_H_

#include "base/process/kill.h"
#include "base/scoped_multi_source_observation.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"

class PrefService;

namespace content {
class RenderProcessHost;
}

namespace metrics {
class MetricsService;
}

namespace chromecast {
namespace metrics {

// Responsible for gathering and logging stability-related metrics. Loosely
// based on the ContentStabilityMetricsProvider in components/metrics/content.
class CastStabilityMetricsProvider
    : public ::metrics::MetricsProvider,
      public content::RenderProcessHostObserver,
      public content::RenderProcessHostCreationObserver {
 public:
  CastStabilityMetricsProvider(::metrics::MetricsService* metrics_service,
                               PrefService* pref_service);

  CastStabilityMetricsProvider(const CastStabilityMetricsProvider&) = delete;
  CastStabilityMetricsProvider& operator=(const CastStabilityMetricsProvider&) =
      delete;

  ~CastStabilityMetricsProvider() override;

  // metrics::MetricsProvider implementation:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;

  // Logs an external crash, presumably from the ExternalMetrics service.
  void LogExternalCrash(const std::string& crash_type);

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // Records a renderer process crash.
  void LogRendererCrash(content::RenderProcessHost* host,
                        base::TerminationStatus status,
                        int exit_code);

  // Increments the specified pref by 1.
  void IncrementPrefValue(const char* path);

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      scoped_observations_{this};

  // Reference to the current MetricsService. Raw pointer is safe, since
  // MetricsService is responsible for the lifetime of
  // CastStabilityMetricsProvider.
  ::metrics::MetricsService* metrics_service_;

  PrefService* const pref_service_;

  bool logging_enabled_ = false;
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_CAST_STABILITY_METRICS_PROVIDER_H_
