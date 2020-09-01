// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include <vector>

#include "base/check.h"
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

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif

namespace metrics {

ContentStabilityMetricsProvider::ContentStabilityMetricsProvider(
    PrefService* local_state,
    std::unique_ptr<ExtensionsHelper> extensions_helper)
    :
#if defined(OS_ANDROID)
      scoped_observer_(this),
#endif  // defined(OS_ANDROID)
      helper_(local_state),
      extensions_helper_(std::move(extensions_helper)) {
  BrowserChildProcessObserver::Add(this);

  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());

#if defined(OS_ANDROID)
  auto* crash_manager = crash_reporter::CrashMetricsReporter::GetInstance();
  DCHECK(crash_manager);
  scoped_observer_.Add(crash_manager);
#endif  // defined(OS_ANDROID)
}

ContentStabilityMetricsProvider::~ContentStabilityMetricsProvider() {
  registrar_.RemoveAll();
  BrowserChildProcessObserver::Remove(this);
}

void ContentStabilityMetricsProvider::OnRecordingEnabled() {}

void ContentStabilityMetricsProvider::OnRecordingDisabled() {}

void ContentStabilityMetricsProvider::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
}

void ContentStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
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
      content::ChildProcessTerminationInfo* process_info =
          content::Details<content::ChildProcessTerminationInfo>(details).ptr();
      bool was_extension_process =
          extensions_helper_ &&
          extensions_helper_->IsExtensionProcess(
              content::Source<content::RenderProcessHost>(source).ptr());
      helper_.LogRendererCrash(was_extension_process, process_info->status,
                               process_info->exit_code);
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      helper_.LogRendererHang();
      break;

    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      bool was_extension_process =
          extensions_helper_ &&
          extensions_helper_->IsExtensionProcess(
              content::Source<content::RenderProcessHost>(source).ptr());
      helper_.LogRendererLaunched(was_extension_process);
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
#if BUILDFLAG(ENABLE_PLUGINS)
  // Exclude plugin crashes from the count below because we report them via
  // a separate UMA metric.
  if (data.process_type == content::PROCESS_TYPE_PPAPI_PLUGIN ||
      data.process_type == content::PROCESS_TYPE_PPAPI_BROKER) {
    return;
  }
#endif

  if (data.process_type == content::PROCESS_TYPE_UTILITY)
    helper_.BrowserUtilityProcessCrashed(data.metrics_name, info.exit_code);
  helper_.BrowserChildProcessCrashed();
}

void ContentStabilityMetricsProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK(!data.metrics_name.empty());
  if (data.process_type == content::PROCESS_TYPE_UTILITY)
    helper_.BrowserUtilityProcessLaunched(data.metrics_name);
}

#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)

}  // namespace metrics
