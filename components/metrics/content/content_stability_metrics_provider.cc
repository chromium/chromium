// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/metrics/content/extensions_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/process_type.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif

namespace metrics {

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Determines which value of RendererHostedContentType correctly describes the
// type of content hosted by `host`.
RendererHostedContentType DetermineHostedContentType(
    content::RenderProcessHost* host,
    ExtensionsHelper* extensions_helper) {
  if (extensions_helper && extensions_helper->IsExtensionProcess(host)) {
    return RendererHostedContentType::kExtension;
  }

  // Iterate through `host`'s frames to identify these frame types:
  bool has_active_foreground_main_frame = false;
  bool has_active_foreground_subframe = false;
  bool has_active_background_frame = false;
  bool has_inactive_frame = false;

  host->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsActive()) {
          if (render_frame_host->GetVisibilityState() ==
              blink::mojom::PageVisibilityState::kVisible) {
            if (render_frame_host->GetMainFrame() == render_frame_host) {
              has_active_foreground_main_frame = true;
            } else {
              has_active_foreground_subframe = true;
            }
          } else {
            has_active_background_frame = true;
          }
        } else {
          has_inactive_frame = true;
        }
      });

  // Derive a `RendererHostedContentType` from the frame types hosted by `host`.
  if (has_active_foreground_main_frame) {
    return RendererHostedContentType::kForegroundMainFrame;
  }
  if (has_active_foreground_subframe) {
    return RendererHostedContentType::kForegroundSubframe;
  } else if (has_active_background_frame) {
    return RendererHostedContentType::kBackgroundFrame;
  } else if (has_inactive_frame) {
    return RendererHostedContentType::kInactiveFrame;
  }

  return RendererHostedContentType::kNoFrameOrExtension;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

ContentStabilityMetricsProvider::ContentStabilityMetricsProvider(
    PrefService* local_state,
    std::unique_ptr<ExtensionsHelper> extensions_helper)
    : helper_(local_state), extensions_helper_(std::move(extensions_helper)) {
  BrowserChildProcessObserver::Add(this);

#if BUILDFLAG(IS_ANDROID)
  auto* crash_manager = crash_reporter::CrashMetricsReporter::GetInstance();
  DCHECK(crash_manager);
  scoped_observation_.Observe(crash_manager);
#endif  // BUILDFLAG(IS_ANDROID)
}

ContentStabilityMetricsProvider::~ContentStabilityMetricsProvider() {
  BrowserChildProcessObserver::Remove(this);
}

void ContentStabilityMetricsProvider::OnRecordingEnabled() {}

void ContentStabilityMetricsProvider::OnRecordingDisabled() {}

#if BUILDFLAG(IS_ANDROID)
void ContentStabilityMetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
}

void ContentStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
}
#endif  // BUILDFLAG(IS_ANDROID)

void ContentStabilityMetricsProvider::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  bool was_extension_process =
      extensions_helper_ && extensions_helper_->IsExtensionProcess(host);
  helper_.LogRendererLaunched(was_extension_process);
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

void ContentStabilityMetricsProvider::OnRenderProcessHostCreationFailed(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
#if BUILDFLAG(IS_IOS)
  helper_.LogRendererCrash();
#elif !BUILDFLAG(IS_ANDROID)
  helper_.LogRendererCrash(
      DetermineHostedContentType(host, extensions_helper_.get()), info.status,
      info.exit_code);
#endif
}

void ContentStabilityMetricsProvider::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // On Android, the renderer crashes are recorded in
  // `OnCrashDumpProcessed`.
#if BUILDFLAG(IS_IOS)
  helper_.LogRendererCrash();
#elif !BUILDFLAG(IS_ANDROID)
  helper_.LogRendererCrash(
      DetermineHostedContentType(host, extensions_helper_.get()), info.status,
      info.exit_code);
#endif
}

void ContentStabilityMetricsProvider::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  // In single-process mode, RenderProcessExited isn't called, so we ensure
  // we remove observations here rather than there, to avoid later use-after-
  // frees in single process mode.
  host_observation_.RemoveObservation(host);
}

void ContentStabilityMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK(!data.metrics_name.empty());
  if (data.process_type == content::PROCESS_TYPE_UTILITY)
    helper_.BrowserUtilityProcessCrashed(data.metrics_name, info.exit_code);
}

void ContentStabilityMetricsProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK(!data.metrics_name.empty());
  if (data.process_type == content::PROCESS_TYPE_UTILITY)
    helper_.BrowserUtilityProcessLaunched(data.metrics_name);
}

void ContentStabilityMetricsProvider::BrowserChildProcessLaunchFailed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK(!data.metrics_name.empty());
  DCHECK_EQ(info.status, base::TERMINATION_STATUS_LAUNCH_FAILED);
  if (data.process_type == content::PROCESS_TYPE_UTILITY)
    helper_.BrowserUtilityProcessLaunchFailed(data.metrics_name, info.exit_code
#if BUILDFLAG(IS_WIN)
                                              ,
                                              info.last_error
#endif
    );
}

#if BUILDFLAG(IS_ANDROID)
void ContentStabilityMetricsProvider::OnCrashDumpProcessed(
    int rph_id,
    const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
        reported_counts) {
  if (reported_counts.count(crash_reporter::CrashMetricsReporter::
                                ProcessedCrashCounts::kRendererCrashAll)) {
    helper_.IncreaseRendererCrashCount();
  }
  if (reported_counts.count(crash_reporter::CrashMetricsReporter::
                                ProcessedCrashCounts::kGpuCrashAll)) {
    helper_.IncreaseGpuCrashCount();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

void ContentStabilityMetricsProvider::OnPageLoadStarted() {
  helper_.LogLoadStarted();
}

}  // namespace metrics
