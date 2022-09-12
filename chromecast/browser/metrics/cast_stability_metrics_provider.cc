// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/cast_stability_metrics_provider.h"

#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"

namespace chromecast {
namespace metrics {
namespace {

enum RendererType {
  RENDERER_TYPE_RENDERER = 1,
  RENDERER_TYPE_EXTENSION, //  Not used, but needed for correct histogram count.
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/metrics/histograms/histograms.xml is
  // updated with any change in here.
  RENDERER_TYPE_COUNT
};

// Converts an exit code into something that can be inserted into our
// histograms (which expect non-negative numbers less than MAX_INT).
int MapCrashExitCodeForHistogram(int exit_code) {
  return std::abs(exit_code);
}

}  // namespace

CastStabilityMetricsProvider::CastStabilityMetricsProvider(
    ::metrics::MetricsService* metrics_service,
    PrefService* pref_service)
    : metrics_service_(metrics_service), pref_service_(pref_service) {
  DCHECK(pref_service_);
}

CastStabilityMetricsProvider::~CastStabilityMetricsProvider() {}

void CastStabilityMetricsProvider::OnRecordingEnabled() {
  logging_enabled_ = true;
}

void CastStabilityMetricsProvider::OnRecordingDisabled() {
  logging_enabled_ = false;
}

void CastStabilityMetricsProvider::LogExternalCrash(
    const std::string& crash_type) {
  if (crash_type == "user")
    IncrementPrefValue(prefs::kStabilityOtherUserCrashCount);
  else if (crash_type == "kernel")
    IncrementPrefValue(prefs::kStabilityKernelCrashCount);
  else if (crash_type == "uncleanshutdown")
    IncrementPrefValue(prefs::kStabilitySystemUncleanShutdownCount);
  else
    NOTREACHED() << "Unexpected crash type " << crash_type;

  // Wake up metrics logs sending if necessary now that new
  // log data is available.
  metrics_service_->OnApplicationNotIdle();
}

void CastStabilityMetricsProvider::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!scoped_observations_.IsObservingSource(host))
    scoped_observations_.AddObservation(host);
}

void CastStabilityMetricsProvider::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  LogRendererCrash(host, info.status, info.exit_code);
}

void CastStabilityMetricsProvider::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  scoped_observations_.RemoveObservation(host);
}

void CastStabilityMetricsProvider::LogRendererCrash(
    content::RenderProcessHost* host,
    base::TerminationStatus status,
    int exit_code) {
  if (!logging_enabled_)
    return;

  if (status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
      status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    ::metrics::StabilityMetricsHelper::RecordStabilityEvent(
        ::metrics::StabilityEventType::kRendererCrash);

    base::UmaHistogramSparse("CrashExitCodes.Renderer",
                             MapCrashExitCodeForHistogram(exit_code));
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildCrashes",
                              RENDERER_TYPE_RENDERER, RENDERER_TYPE_COUNT);
  } else if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildKills",
                              RENDERER_TYPE_RENDERER, RENDERER_TYPE_COUNT);
  } else if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
    UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.DisconnectedAlive",
                              RENDERER_TYPE_RENDERER, RENDERER_TYPE_COUNT);
  } else if (status == base::TERMINATION_STATUS_LAUNCH_FAILED) {
    ::metrics::StabilityMetricsHelper::RecordStabilityEvent(
        ::metrics::StabilityEventType::kRendererFailedLaunch);
  }
}

void CastStabilityMetricsProvider::IncrementPrefValue(const char* path) {
  int value = pref_service_->GetInteger(path);
  pref_service_->SetInteger(path, value + 1);
}

}  // namespace metrics
}  // namespace chromecast
