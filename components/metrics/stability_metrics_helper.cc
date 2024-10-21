// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_helper.h"

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // Needed for STATUS_* codes
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace metrics {
namespace {

#if !BUILDFLAG(IS_ANDROID)
// Converts an exit code into something that can be inserted into our
// histograms (which expect non-negative numbers less than MAX_INT).
int MapCrashExitCodeForHistogram(int exit_code) {
#if BUILDFLAG(IS_WIN)
  // Since |abs(STATUS_GUARD_PAGE_VIOLATION) == MAX_INT| it causes problems in
  // histograms.cc. Solve this by remapping it to a smaller value, which
  // hopefully doesn't conflict with other codes.
  if (static_cast<DWORD>(exit_code) == STATUS_GUARD_PAGE_VIOLATION)
    return 0x1FCF7EC3;  // Randomly picked number.
#endif

  return std::abs(exit_code);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
const char* HostedContentTypeToString(
    RendererHostedContentType hosted_content_type) {
  switch (hosted_content_type) {
    case metrics::RendererHostedContentType::kExtension:
      return "Extension";
    case metrics::RendererHostedContentType::kForegroundMainFrame:
      return "ForegroundMainFrame";
    case metrics::RendererHostedContentType::kForegroundSubframe:
      return "ForegroundSubframe";
    case metrics::RendererHostedContentType::kBackgroundFrame:
      return "BackgroundFrame";
    case metrics::RendererHostedContentType::kInactiveFrame:
      return "InactiveFrame";
    case metrics::RendererHostedContentType::kNoFrameOrExtension:
      return "NoFrameOrExtension";
  }
}

void RecordRendererAbnormalTerminationByHostedContentType(
    RendererHostedContentType hosted_content_type,
    base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Stability.RendererAbnormalTermination2.HostedContentType",
      hosted_content_type);
  base::UmaHistogramEnumeration(
      base::StrCat({"Stability.RendererAbnormalTermination2.",
                    HostedContentTypeToString(hosted_content_type)}),
      status, base::TERMINATION_STATUS_MAX_ENUM);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

StabilityMetricsHelper::StabilityMetricsHelper(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

StabilityMetricsHelper::~StabilityMetricsHelper() = default;

#if BUILDFLAG(IS_ANDROID)
void StabilityMetricsHelper::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
  SystemProfileProto_Stability* stability_proto =
      system_profile_proto->mutable_stability();

  int count = local_state_->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    stability_proto->set_page_load_count(count);
    local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }
  count = local_state_->GetInteger(prefs::kStabilityRendererLaunchCount);
  if (count) {
    stability_proto->set_renderer_launch_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererLaunchCount, 0);
  }
}

void StabilityMetricsHelper::ClearSavedStabilityMetrics() {
  local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererLaunchCount, 0);
}
#endif  // BUILDFLAG(IS_ANDROID)

// static
void StabilityMetricsHelper::RegisterPrefs(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererLaunchCount, 0);
#endif  // BUILDFLAG(IS_ANDROID)
}

void StabilityMetricsHelper::IncreaseRendererCrashCount() {
  RecordStabilityEvent(StabilityEventType::kRendererCrash);
}

void StabilityMetricsHelper::IncreaseGpuCrashCount() {
  RecordStabilityEvent(StabilityEventType::kGpuCrash);
}

void StabilityMetricsHelper::BrowserUtilityProcessLaunched(
    const std::string& metrics_name) {
  uint32_t hash = variations::HashName(metrics_name);
  base::UmaHistogramSparse("ChildProcess.Launched.UtilityProcessHash", hash);
  RecordStabilityEvent(StabilityEventType::kUtilityLaunch);
}

void StabilityMetricsHelper::BrowserUtilityProcessCrashed(
    const std::string& metrics_name,
    int exit_code) {
  uint32_t hash = variations::HashName(metrics_name);
  base::UmaHistogramSparse("ChildProcess.Crashed.UtilityProcessHash", hash);
  base::UmaHistogramSparse("ChildProcess.Crashed.UtilityProcessExitCode",
                           exit_code);
  RecordStabilityEvent(StabilityEventType::kUtilityCrash);
}

void StabilityMetricsHelper::BrowserUtilityProcessLaunchFailed(
    const std::string& metrics_name,
    int launch_error_code
#if BUILDFLAG(IS_WIN)
    ,
    DWORD last_error
#endif
) {
  uint32_t hash = variations::HashName(metrics_name);
  base::UmaHistogramSparse("ChildProcess.LaunchFailed.UtilityProcessHash",
                           hash);
  base::UmaHistogramSparse("ChildProcess.LaunchFailed.UtilityProcessErrorCode",
                           launch_error_code);
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramSparse("ChildProcess.LaunchFailed.WinLastError",
                           last_error);
#endif
  // TODO(wfh): Decide if this utility process launch failure should also
  // trigger a Stability Event.
}

void StabilityMetricsHelper::LogLoadStarted() {
#if BUILDFLAG(IS_ANDROID)
  IncrementPrefValue(prefs::kStabilityPageLoadCount);
#endif
  RecordStabilityEvent(StabilityEventType::kPageLoad);
}

#if BUILDFLAG(IS_IOS)
void StabilityMetricsHelper::LogRendererCrash() {
  // The actual exit code isn't provided on iOS; use a dummy value.
  constexpr int kDummyExitCode = 105;
  LogRendererCrashImpl(CoarseRendererType::kRenderer, kDummyExitCode);
}
#elif !BUILDFLAG(IS_ANDROID)
void StabilityMetricsHelper::LogRendererCrash(
    RendererHostedContentType hosted_content_type,
    base::TerminationStatus status,
    int exit_code) {
  RecordRendererAbnormalTerminationByHostedContentType(hosted_content_type,
                                                       status);

  CoarseRendererType coarse_renderer_type =
      hosted_content_type == RendererHostedContentType::kExtension
          ? CoarseRendererType::kExtension
          : CoarseRendererType::kRenderer;

  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_OOM:
      LogRendererCrashImpl(coarse_renderer_type, exit_code);
      break;
#if BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      base::UmaHistogramEnumeration("BrowserRenderProcessHost.ChildKills.OOM",
                                    coarse_renderer_type);
      [[fallthrough]];
#endif  // BUILDFLAG(IS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      base::UmaHistogramEnumeration("BrowserRenderProcessHost.ChildKills",
                                    coarse_renderer_type);
      break;
    case base::TERMINATION_STATUS_STILL_RUNNING:
      base::UmaHistogramEnumeration(
          "BrowserRenderProcessHost.DisconnectedAlive", coarse_renderer_type);
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      // TODO(rkaplow): See if we can remove this histogram as we have
      // Stability.Counts2 which has the same metrics.
      base::UmaHistogramEnumeration(
          "BrowserRenderProcessHost.ChildLaunchFailures", coarse_renderer_type);
      base::UmaHistogramSparse(
          "BrowserRenderProcessHost.ChildLaunchFailureCodes", exit_code);
      RecordStabilityEvent(
          hosted_content_type == RendererHostedContentType::kExtension
              ? StabilityEventType::kExtensionRendererFailedLaunch
              : StabilityEventType::kRendererFailedLaunch);
      break;
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      base::UmaHistogramEnumeration(
          "BrowserRenderProcessHost.ChildCodeIntegrityFailures",
          coarse_renderer_type);
      break;
#endif
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void StabilityMetricsHelper::LogRendererLaunched(bool was_extension_process) {
  auto metric = was_extension_process
                    ? StabilityEventType::kExtensionRendererLaunch
                    : StabilityEventType::kRendererLaunch;
  RecordStabilityEvent(metric);
#if BUILDFLAG(IS_ANDROID)
  if (!was_extension_process)
    IncrementPrefValue(prefs::kStabilityRendererLaunchCount);
#endif  // BUILDFLAG(IS_ANDROID)
}

void StabilityMetricsHelper::IncrementPrefValue(const char* path) {
  int value = local_state_->GetInteger(path);
  local_state_->SetInteger(path, value + 1);
}

// static
void StabilityMetricsHelper::RecordStabilityEvent(
    StabilityEventType stability_event_type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.Counts2",
                                      stability_event_type);
}

#if !BUILDFLAG(IS_ANDROID)
void StabilityMetricsHelper::LogRendererCrashImpl(
    CoarseRendererType renderer_type,
    int exit_code) {
  if (renderer_type == CoarseRendererType::kExtension) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
    NOTREACHED_IN_MIGRATION();
#endif
    RecordStabilityEvent(StabilityEventType::kExtensionCrash);
    base::UmaHistogramSparse("CrashExitCodes.Extension",
                             MapCrashExitCodeForHistogram(exit_code));
  } else {
    IncreaseRendererCrashCount();
    base::UmaHistogramSparse("CrashExitCodes.Renderer",
                             MapCrashExitCodeForHistogram(exit_code));
  }

  base::UmaHistogramEnumeration("BrowserRenderProcessHost.ChildCrashes",
                                renderer_type);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace metrics
