// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/metrics/content/extensions_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif

namespace metrics {

ContentStabilityMetricsProvider::ContentStabilityMetricsProvider(
    PrefService* local_state,
    std::unique_ptr<ExtensionsHelper> extensions_helper)
    : helper_(local_state), extensions_helper_(std::move(extensions_helper)) {
  BrowserChildProcessObserver::Add(this);

  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());

#if BUILDFLAG(IS_ANDROID)
  auto* crash_manager = crash_reporter::CrashMetricsReporter::GetInstance();
  DCHECK(crash_manager);
  scoped_observation_.Observe(crash_manager);
#endif  // BUILDFLAG(IS_ANDROID)
}

ContentStabilityMetricsProvider::~ContentStabilityMetricsProvider() {
  registrar_.RemoveAll();
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
}

void ContentStabilityMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START:
      helper_.LogLoadStarted();
      break;

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      // On Android, the renderer crashes are recorded in
      // `OnCrashDumpProcessed`.
#if !BUILDFLAG(IS_ANDROID)
      content::ChildProcessTerminationInfo* process_info =
          content::Details<content::ChildProcessTerminationInfo>(details).ptr();
      bool was_extension_process =
          extensions_helper_ &&
          extensions_helper_->IsExtensionProcess(
              content::Source<content::RenderProcessHost>(source).ptr());
      helper_.LogRendererCrash(was_extension_process, process_info->status,
                               process_info->exit_code);
#endif  // !BUILDFLAG(IS_ANDROID)
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG: {
      helper_.LogRendererHang();
      break;
    }

    default:
      NOTREACHED();
      break;
  }
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

}  // namespace metrics
