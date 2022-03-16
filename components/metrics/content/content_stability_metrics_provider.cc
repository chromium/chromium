// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/metrics/content/extensions_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif

namespace metrics {
namespace {

using content::RenderProcessHost;

base::LazyInstance<std::vector<ContentStabilityMetricsProvider*>>::Leaky
    g_providers;

}  // namespace

ContentStabilityMetricsProvider::ContentStabilityMetricsProvider(
    PrefService* local_state,
    std::unique_ptr<ExtensionsHelper> extensions_helper)
    : helper_(local_state), extensions_helper_(std::move(extensions_helper)) {
  BrowserChildProcessObserver::Add(this);
  g_providers.Get().push_back(this);

  // Observe existing render processes. (When a new render process is created,
  // we will observe it in OnRenderProcessHostCreated.)
  for (auto it = RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
       it.Advance()) {
    scoped_observations_.AddObservation(it.GetCurrentValue());
  }

#if BUILDFLAG(IS_ANDROID)
  auto* crash_manager = crash_reporter::CrashMetricsReporter::GetInstance();
  DCHECK(crash_manager);
  scoped_observation_.Observe(crash_manager);
#endif  // BUILDFLAG(IS_ANDROID)
}

ContentStabilityMetricsProvider::~ContentStabilityMetricsProvider() {
  BrowserChildProcessObserver::Remove(this);
  base::Erase(g_providers.Get(), this);
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

void ContentStabilityMetricsProvider::OnRenderProcessHostCreated(
    RenderProcessHost* host) {
  // Sometimes, the same host will cause multiple notifications in tests so
  // could possibly do the same in a release build.
  if (!scoped_observations_.IsObservingSource(host))
    scoped_observations_.AddObservation(host);

  bool is_extension_process =
      extensions_helper_ && extensions_helper_->IsExtensionProcess(host);
  helper_.LogRendererLaunched(is_extension_process);
}

void ContentStabilityMetricsProvider::RenderProcessExited(
    RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // On Android, the renderer crashes are recorded in `OnCrashDumpProcessed`.
#if !BUILDFLAG(IS_ANDROID)
  bool was_extension_process =
      extensions_helper_ && extensions_helper_->IsExtensionProcess(host);
  helper_.LogRendererCrash(was_extension_process, info.status, info.exit_code);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ContentStabilityMetricsProvider::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  scoped_observations_.RemoveObservation(host);
}

void ContentStabilityMetricsProvider::DidStartLoading() {
  helper_.LogLoadStarted();
}

void ContentStabilityMetricsProvider::OnRendererUnresponsive() {
  helper_.LogRendererHang();
}

void ContentStabilityMetricsProvider::SetupWebContentsObserver(
    content::WebContents* web_contents) {
  web_contents->SetUserData(
      WebContentsObserverImpl::UserDataKey(),
      base::WrapUnique(new WebContentsObserverImpl(web_contents)));
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

ContentStabilityMetricsProvider::WebContentsObserverImpl::
    WebContentsObserverImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebContentsObserverImpl>(*web_contents) {}

void ContentStabilityMetricsProvider::WebContentsObserverImpl::
    DidStartLoading() {
  for (auto* provider : g_providers.Get())
    provider->DidStartLoading();
}

void ContentStabilityMetricsProvider::WebContentsObserverImpl::
    OnRendererUnresponsive(RenderProcessHost* host) {
  for (auto* provider : g_providers.Get())
    provider->OnRendererUnresponsive();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    ContentStabilityMetricsProvider::WebContentsObserverImpl);

}  // namespace metrics
