// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/stability_metrics_helper.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if defined(OS_WIN)
#include <windows.h>  // Needed for STATUS_* codes
#endif

#if defined(OS_CHROMEOS)
#include "components/metrics/system_memory_stats_recorder.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace metrics {

namespace {

enum RendererType {
  RENDERER_TYPE_RENDERER = 1,
  RENDERER_TYPE_EXTENSION,
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/metrics/histograms/histograms.xml is
  // updated with any change in here.
  RENDERER_TYPE_COUNT
};

// Converts an exit code into something that can be inserted into our
// histograms (which expect non-negative numbers less than MAX_INT).
int MapCrashExitCodeForHistogram(int exit_code) {
#if defined(OS_WIN)
  // Since |abs(STATUS_GUARD_PAGE_VIOLATION) == MAX_INT| it causes problems in
  // histograms.cc. Solve this by remapping it to a smaller value, which
  // hopefully doesn't conflict with other codes.
  if (static_cast<DWORD>(exit_code) == STATUS_GUARD_PAGE_VIOLATION)
    return 0x1FCF7EC3;  // Randomly picked number.
#endif

  return std::abs(exit_code);
}

void RecordChildKills(RendererType histogram_type) {
  UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildKills",
                            histogram_type, RENDERER_TYPE_COUNT);
}

// Macro for logging the age of a crashed process.
//
// Notes:
// - IMPORTANT: When changing the constants below, please change the names of
//   the histograms logged via UMA_HISTOGRAM_CRASHED_PROCESS_AGE.
// - 99th percentile of Memory.Experimental.Renderer.Uptime hovers around 17h.
// - |kCrashedProcessAgeMin| is as low as possible, so that we may with
//   high-confidence categorize crashes that occur during early startup (e.g.
//   crashes that end up with STATUS_DLL_INIT_FAILED or STATUS_DLL_NOT_FOUND).
// - Note that even with just 50 buckets, we still get narrow and accurate
//   buckets at the lower end: 0ms, 1ms, 2ms, 3ms, 4-5ms, 6-8ms, 9-12ms, ...
constexpr auto kCrashedProcessAgeMin = base::TimeDelta::FromMilliseconds(1);
constexpr auto kCrashedProcessAgeMax = base::TimeDelta::FromHours(48);
constexpr uint32_t kCrashedProcessAgeCount = 50;
#define UMA_HISTOGRAM_CRASHED_PROCESS_AGE(histogram_name, uptime)           \
  UMA_HISTOGRAM_CUSTOM_TIMES(histogram_name, uptime, kCrashedProcessAgeMin, \
                             kCrashedProcessAgeMax, kCrashedProcessAgeCount)

}  // namespace

StabilityMetricsHelper::StabilityMetricsHelper(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

StabilityMetricsHelper::~StabilityMetricsHelper() {}

void StabilityMetricsHelper::ProvideStabilityMetrics(
    SystemProfileProto* system_profile_proto) {
  SystemProfileProto_Stability* stability_proto =
      system_profile_proto->mutable_stability();

  int count = local_state_->GetInteger(prefs::kStabilityPageLoadCount);
  if (count) {
    stability_proto->set_page_load_count(count);
    local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityChildProcessCrashCount);
  if (count) {
    stability_proto->set_child_process_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityGpuCrashCount);
  if (count) {
    stability_proto->set_gpu_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityGpuCrashCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererCrashCount);
  if (count) {
    stability_proto->set_renderer_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererFailedLaunchCount);
  if (count) {
    stability_proto->set_renderer_failed_launch_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererFailedLaunchCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererLaunchCount);
  if (count) {
    stability_proto->set_renderer_launch_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererLaunchCount, 0);
  }

  count =
      local_state_->GetInteger(prefs::kStabilityExtensionRendererCrashCount);
  if (count) {
    stability_proto->set_extension_renderer_crash_count(count);
    local_state_->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  }

  count = local_state_->GetInteger(
      prefs::kStabilityExtensionRendererFailedLaunchCount);
  if (count) {
    stability_proto->set_extension_renderer_failed_launch_count(count);
    local_state_->SetInteger(
        prefs::kStabilityExtensionRendererFailedLaunchCount, 0);
  }

  count = local_state_->GetInteger(prefs::kStabilityRendererHangCount);
  if (count) {
    stability_proto->set_renderer_hang_count(count);
    local_state_->SetInteger(prefs::kStabilityRendererHangCount, 0);
  }

  count =
      local_state_->GetInteger(prefs::kStabilityExtensionRendererLaunchCount);
  if (count) {
    stability_proto->set_extension_renderer_launch_count(count);
    local_state_->SetInteger(prefs::kStabilityExtensionRendererLaunchCount, 0);
  }
}

void StabilityMetricsHelper::ClearSavedStabilityMetrics() {
  // Clear all the prefs used in this class in UMA reports (which doesn't
  // include |kUninstallMetricsPageLoadCount| as it's not sent up by UMA).
  local_state_->SetInteger(prefs::kStabilityChildProcessCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityExtensionRendererCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityExtensionRendererFailedLaunchCount,
                           0);
  local_state_->SetInteger(prefs::kStabilityExtensionRendererLaunchCount, 0);
  local_state_->SetInteger(prefs::kStabilityGpuCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererFailedLaunchCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererHangCount, 0);
  local_state_->SetInteger(prefs::kStabilityRendererLaunchCount, 0);
}

// static
void StabilityMetricsHelper::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityChildProcessCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityExtensionRendererCrashCount,
                                0);
  registry->RegisterIntegerPref(
      prefs::kStabilityExtensionRendererFailedLaunchCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityExtensionRendererLaunchCount,
                                0);
  registry->RegisterIntegerPref(prefs::kStabilityGpuCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererFailedLaunchCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererHangCount, 0);
  registry->RegisterIntegerPref(prefs::kStabilityRendererLaunchCount, 0);

  registry->RegisterInt64Pref(prefs::kUninstallMetricsPageLoadCount, 0);
}

void StabilityMetricsHelper::IncreaseRendererCrashCount() {
  IncrementPrefValue(prefs::kStabilityRendererCrashCount);
  RecordStabilityEvent(StabilityEventType::kRendererCrash);
}

void StabilityMetricsHelper::IncreaseGpuCrashCount() {
  IncrementPrefValue(prefs::kStabilityGpuCrashCount);
}

void StabilityMetricsHelper::BrowserUtilityProcessLaunched(
    const std::string& metrics_name) {
  uint32_t hash = variations::HashName(metrics_name);
  base::UmaHistogramSparse("ChildProcess.Launched.UtilityProcessHash", hash);
}

void StabilityMetricsHelper::BrowserUtilityProcessCrashed(
    const std::string& metrics_name,
    int exit_code) {
  uint32_t hash = variations::HashName(metrics_name);
  base::UmaHistogramSparse("ChildProcess.Crashed.UtilityProcessHash", hash);
  base::UmaHistogramSparse("ChildProcess.Crashed.UtilityProcessExitCode",
                           exit_code);
}

void StabilityMetricsHelper::BrowserChildProcessCrashed() {
  IncrementPrefValue(prefs::kStabilityChildProcessCrashCount);
}

void StabilityMetricsHelper::LogLoadStarted() {
  base::RecordAction(base::UserMetricsAction("PageLoad"));
  IncrementPrefValue(prefs::kStabilityPageLoadCount);
  IncrementLongPrefsValue(prefs::kUninstallMetricsPageLoadCount);
  RecordStabilityEvent(StabilityEventType::kPageLoad);
}

void StabilityMetricsHelper::LogRendererCrash(
    bool was_extension_process,
    base::TerminationStatus status,
    int exit_code,
    base::Optional<base::TimeDelta> uptime) {
  RendererType histogram_type =
      was_extension_process ? RENDERER_TYPE_EXTENSION : RENDERER_TYPE_RENDERER;

  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_OOM:
      if (was_extension_process) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
        NOTREACHED();
#endif
        IncrementPrefValue(prefs::kStabilityExtensionRendererCrashCount);
        RecordStabilityEvent(StabilityEventType::kExtensionCrash);

        base::UmaHistogramSparse("CrashExitCodes.Extension",
                                 MapCrashExitCodeForHistogram(exit_code));
        if (uptime.has_value()) {
          UMA_HISTOGRAM_CRASHED_PROCESS_AGE(
              "Stability.CrashedProcessAge.Extension", uptime.value());
        }
      } else {
        IncreaseRendererCrashCount();

        base::UmaHistogramSparse("CrashExitCodes.Renderer",
                                 MapCrashExitCodeForHistogram(exit_code));
        if (uptime.has_value()) {
          UMA_HISTOGRAM_CRASHED_PROCESS_AGE(
              "Stability.CrashedProcessAge.Renderer", uptime.value());
        }
      }

      UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildCrashes",
                                histogram_type, RENDERER_TYPE_COUNT);
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordChildKills(histogram_type);
      break;
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      // TODO(wfh): Check if this should be a Kill or a Crash on Android.
      break;
#endif
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      RecordChildKills(histogram_type);
      UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildKills.OOM",
                                was_extension_process ? 2 : 1, 3);
      RecordMemoryStats(was_extension_process
                            ? RECORD_MEMORY_STATS_EXTENSIONS_OOM_KILLED
                            : RECORD_MEMORY_STATS_CONTENTS_OOM_KILLED);
      break;
#endif
    case base::TERMINATION_STATUS_STILL_RUNNING:
      UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.DisconnectedAlive",
                                histogram_type, RENDERER_TYPE_COUNT);
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.ChildLaunchFailures",
                                histogram_type, RENDERER_TYPE_COUNT);
      base::UmaHistogramSparse(
          "BrowserRenderProcessHost.ChildLaunchFailureCodes", exit_code);
      if (was_extension_process)
        IncrementPrefValue(prefs::kStabilityExtensionRendererFailedLaunchCount);
      else
        IncrementPrefValue(prefs::kStabilityRendererFailedLaunchCount);
      break;
#if defined(OS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      UMA_HISTOGRAM_ENUMERATION(
          "BrowserRenderProcessHost.ChildCodeIntegrityFailures", histogram_type,
          RENDERER_TYPE_COUNT);
      break;
#endif
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED();
      break;
  }
}

void StabilityMetricsHelper::LogRendererLaunched(bool was_extension_process) {
  if (was_extension_process)
    IncrementPrefValue(prefs::kStabilityExtensionRendererLaunchCount);
  else
    IncrementPrefValue(prefs::kStabilityRendererLaunchCount);
}

void StabilityMetricsHelper::IncrementPrefValue(const char* path) {
  int value = local_state_->GetInteger(path);
  local_state_->SetInteger(path, value + 1);
}

void StabilityMetricsHelper::IncrementLongPrefsValue(const char* path) {
  int64_t value = local_state_->GetInt64(path);
  local_state_->SetInt64(path, value + 1);
}

void StabilityMetricsHelper::LogRendererHang() {
#if defined(OS_ANDROID)
  base::android::ApplicationState app_state =
      base::android::ApplicationStatusListener::GetState();
  bool is_foreground =
      app_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      app_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
  UMA_HISTOGRAM_BOOLEAN("ChildProcess.HungRendererInForeground", is_foreground);
#endif
  UMA_HISTOGRAM_MEMORY_MB(
      "ChildProcess.HungRendererAvailableMemoryMB",
      base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024 / 1024);
  IncrementPrefValue(prefs::kStabilityRendererHangCount);
}

// static
void StabilityMetricsHelper::RecordStabilityEvent(
    StabilityEventType stability_event_type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.Experimental.Counts",
                                      stability_event_type);
}

}  // namespace metrics
