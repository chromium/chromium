// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include <string_view>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "components/crash/content/browser/process_exit_reason_from_system_android.h"

namespace crash_reporter {
namespace {

constexpr char kKillSpareRendererAvailabilityIntentionalKillUMAName[] =
    "Stability.Android.KillSpareRendererAvailability.IntentionalKill";

constexpr char kKillSpareRendererAvailabilityOOMUMAName[] =
    "Stability.Android.KillSpareRendererAvailability.OOM";

void ReportCrashCount(CrashMetricsReporter::ProcessedCrashCounts crash_type,
                      CrashMetricsReporter::ReportedCrashTypeSet* counts) {
  UMA_HISTOGRAM_ENUMERATION("Stability.Android.ProcessedCrashCounts",
                            crash_type);
  counts->insert(crash_type);
}

void RecordSystemExitReason(
    base::ProcessHandle pid,
    const CrashMetricsReporter::ReportedCrashTypeSet& reported_counts) {
  std::string_view suffix;
  if (reported_counts.count(CrashMetricsReporter::ProcessedCrashCounts::
                                kRendererForegroundVisibleSubframeOom) > 0) {
    suffix = "VisibleSubframeOom";
  } else if (reported_counts.count(CrashMetricsReporter::ProcessedCrashCounts::
                                       kRendererForegroundVisibleOom) > 0) {
    suffix = "VisibleMainFrameOom";
  } else if (reported_counts.count(CrashMetricsReporter::ProcessedCrashCounts::
                                       kGpuForegroundOom) > 0) {
    suffix = "GpuForegroundOom";
  } else if (reported_counts.count(CrashMetricsReporter::ProcessedCrashCounts::
                                       kUtilityForegroundOom) > 0) {
    suffix = "UtilityForegroundOom";
  }
  if (!suffix.empty()) {
    ProcessExitReasonFromSystem::RecordExitReasonToUma(
        pid, base::StrCat({"Stability.Android.SystemExitReason.", suffix}));
  }
}

void RecordSpareRendererAvailability(bool is_oom_kill,
                                     bool is_intentioal_kill,
                                     bool is_spare_renderer_killed,
                                     bool has_spare_renderer) {
  if (!is_oom_kill && !is_intentioal_kill) {
    return;
  }
  using SpareRendererAvailabilityWhenKilled =
      CrashMetricsReporter::SpareRendererAvailabilityWhenKilled;
  SpareRendererAvailabilityWhenKilled availability;
  if (is_spare_renderer_killed) {
    availability = SpareRendererAvailabilityWhenKilled::kKillSpareRenderer;
  } else if (has_spare_renderer) {
    availability = SpareRendererAvailabilityWhenKilled::
        kKillNonSpareRendererWithSpareRender;
  } else {
    availability = SpareRendererAvailabilityWhenKilled::
        kKillNonSpareRendererWithoutSpareRenderer;
  }
  const char* target_uma_name =
      is_oom_kill ? kKillSpareRendererAvailabilityOOMUMAName
                  : kKillSpareRendererAvailabilityIntentionalKillUMAName;
  base::UmaHistogramEnumeration(target_uma_name, availability);
}

void RecordMemoryPressureMetricsOnProcessKill(
    const ChildExitObserver::TerminationInfo& info) {
  if (!info.memory_pressure_metrics) {
    return;
  }

  std::string_view process_type_suffix;
  if (info.process_type == content::PROCESS_TYPE_GPU) {
    process_type_suffix = "Gpu";
  } else if (info.process_type == content::PROCESS_TYPE_RENDERER &&
             info.renderer_has_visible_clients) {
    process_type_suffix = "VisibleRenderer";
  } else {
    return;
  }

  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Browser.Memory.Experimental.MemoryPressureOnProcessKill.",
                    process_type_suffix, ".AvailableMemory"}),
      info.memory_pressure_metrics->available_memory.InMiB());
  base::UmaHistogramMemoryLargeMB(
      base::StrCat({"Browser.Memory.Experimental.MemoryPressureOnProcessKill.",
                    process_type_suffix, ".TotalPrivateFootprint"}),
      info.memory_pressure_metrics->total_private_footprint.InMiB());
  base::UmaHistogramCounts100(
      base::StrCat({"Browser.Memory.Experimental.MemoryPressureOnProcessKill.",
                    process_type_suffix, ".TotalProcessCount"}),
      info.memory_pressure_metrics->total_process_count);
  base::UmaHistogramCounts100(
      base::StrCat({"Browser.Memory.Experimental.MemoryPressureOnProcessKill.",
                    process_type_suffix, ".VisibleRendererCount"}),
      info.memory_pressure_metrics->visible_renderer_count);
}

void RecordProcessKillSinceSpareCreation(
    const ChildExitObserver::TerminationInfo& info) {
  if (!info.last_spare_renderer_creation_info) {
    return;
  }
  base::TimeDelta time_since_creation =
      base::TimeTicks::Now() -
      info.last_spare_renderer_creation_info->creation_time;
  int available_memory =
      info.last_spare_renderer_creation_info->available_memory_mb;
  std::string_view suffix;
  if (info.process_type == content::PROCESS_TYPE_GPU) {
    suffix = "Gpu";
  } else if (info.process_type == content::PROCESS_TYPE_RENDERER &&
             info.renderer_has_visible_clients) {
    suffix = "Visible";
  } else if (info.process_type == content::PROCESS_TYPE_RENDERER &&
             info.binding_state == base::android::ChildBindingState::WAIVED) {
    suffix = "Waived";
  }

  if (suffix.empty()) {
    return;
  }

  if (time_since_creation < base::Seconds(1)) {
    base::UmaHistogramMemoryLargeMB(
        base::StrCat({"BrowserRenderProcessHost.AvailableMemory.SpareRenderer.",
                      suffix, "ProcessKillWithin1s"}),
        available_memory);
  }
  if (time_since_creation < base::Seconds(5)) {
    base::UmaHistogramMemoryLargeMB(
        base::StrCat({"BrowserRenderProcessHost.AvailableMemory.SpareRenderer.",
                      suffix, "ProcessKillWithin5s"}),
        available_memory);
  }
}

}  // namespace

//  static
CrashMetricsReporter* CrashMetricsReporter::GetInstance() {
  static CrashMetricsReporter* instance = new CrashMetricsReporter();
  return instance;
}

CrashMetricsReporter::CrashMetricsReporter()
    : async_observers_(
          base::MakeRefCounted<
              base::ObserverListThreadSafe<CrashMetricsReporter::Observer>>()) {
}

CrashMetricsReporter::~CrashMetricsReporter() = default;

void CrashMetricsReporter::AddObserver(
    CrashMetricsReporter::Observer* observer) {
  async_observers_->AddObserver(observer);
}

void CrashMetricsReporter::RemoveObserver(
    CrashMetricsReporter::Observer* observer) {
  async_observers_->RemoveObserver(observer);
}

void CrashMetricsReporter::ChildProcessExited(
    const ChildExitObserver::TerminationInfo& info) {
  ReportedCrashTypeSet reported_counts;
  const bool crashed = info.is_crashed();
  const bool app_foreground =
      info.app_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      info.app_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;
  const bool intentional_kill = info.was_killed_intentionally_by_browser;
  const bool android_oom_kill = !info.was_killed_intentionally_by_browser &&
                                !crashed && !info.normal_termination &&
                                !info.renderer_shutdown_requested;
  const bool renderer_visible = info.renderer_has_visible_clients;
  const bool renderer_subframe = info.renderer_was_subframe;
  const bool renderer_allocation_failed =
      info.blink_oom_metrics.allocation_failed;
  const base::ByteCount available_memory =
      info.blink_oom_metrics.current_available_memory;
  const base::ByteCount swap_free = info.blink_oom_metrics.current_swap_free;

  RecordSpareRendererAvailability(android_oom_kill, intentional_kill,
                                  info.is_spare_renderer,
                                  info.has_spare_renderer);
  RecordProcessKillSinceSpareCreation(info);
  RecordMemoryPressureMetricsOnProcessKill(info);

  if (app_foreground && android_oom_kill) {
    if (info.process_type == content::PROCESS_TYPE_GPU) {
      ReportCrashCount(ProcessedCrashCounts::kGpuForegroundOom,
                       &reported_counts);
    } else if (info.process_type == content::PROCESS_TYPE_UTILITY) {
      ReportCrashCount(ProcessedCrashCounts::kUtilityForegroundOom,
                       &reported_counts);
    }
  }

  if (info.process_type == content::PROCESS_TYPE_RENDERER &&
      !intentional_kill && !info.normal_termination &&
      renderer_allocation_failed) {
    ReportCrashCount(ProcessedCrashCounts::kRendererAllocationFailureAll,
                     &reported_counts);
    if (app_foreground && renderer_visible) {
      ReportCrashCount(
          ProcessedCrashCounts::kRendererForegroundVisibleAllocationFailure,
          &reported_counts);
    }
  }

  if (info.process_type == content::PROCESS_TYPE_RENDERER && app_foreground) {
    if (renderer_visible) {
      if (crashed) {
        ReportCrashCount(
            renderer_subframe
                ? ProcessedCrashCounts::kRendererForegroundVisibleSubframeCrash
                : ProcessedCrashCounts::kRendererForegroundVisibleCrash,
            &reported_counts);
      } else if (intentional_kill) {
        ReportCrashCount(
            renderer_subframe
                ? ProcessedCrashCounts::
                      kRendererForegroundVisibleSubframeIntentionalKill
                : ProcessedCrashCounts::
                      kRendererForegroundVisibleMainFrameIntentionalKill,
            &reported_counts);
      } else if (info.normal_termination) {
        ReportCrashCount(ProcessedCrashCounts::
                             kRendererForegroundVisibleNormalTermNoMinidump,
                         &reported_counts);
      } else if (!info.renderer_shutdown_requested) {
        DCHECK(android_oom_kill);
        if (renderer_subframe) {
          ReportCrashCount(
              ProcessedCrashCounts::kRendererForegroundVisibleSubframeOom,
              &reported_counts);
        } else {
          ReportCrashCount(ProcessedCrashCounts::kRendererForegroundVisibleOom,
                           &reported_counts);
          base::RecordAction(
              base::UserMetricsAction("RendererForegroundMainFrameOOM"));
        }
        base::SystemMemoryInfo meminfo;
        base::GetSystemMemoryInfo(&meminfo);
        base::UmaHistogramMemoryLargeMB(
            "Memory.Experimental.Renderer.TotalMemoryAfterOOM", meminfo.total);
        base::UmaHistogramMemoryLargeMB(
            "Memory.Experimental.Renderer.AvailableMemoryAfterOOM",
            meminfo.available);
        base::UmaHistogramMemoryLargeMB(
            "Memory.Experimental.Renderer.SwapFreeAfterOOM", meminfo.swap_free);
        base::UmaHistogramMemoryLargeMB(
            "Memory.Experimental.Renderer.AvailableMemoryBeforeOOM",
            available_memory);
        base::UmaHistogramMemoryLargeMB(
            "Memory.Experimental.Renderer.SwapFreeBeforeOOM", swap_free);
      }
    } else if (!crashed) {
      // Record stats when renderer is not visible, but the process has oom
      // protected bindings. This case occurs when a tab is switched or closed,
      // the bindings are updated later than visibility on web contents.
      switch (info.binding_state) {
        case base::android::ChildBindingState::UNBOUND:
          break;
        case base::android::ChildBindingState::WAIVED:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithWaivedBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithWaivedBindingOom,
                &reported_counts);
          }
          break;
        case base::android::ChildBindingState::STRONG:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithStrongBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithStrongBindingOom,
                &reported_counts);
          }
          break;
        case base::android::ChildBindingState::VISIBLE:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithVisibleBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithVisibleBindingOom,
                &reported_counts);
          }
          break;
        case base::android::ChildBindingState::NOT_PERCEPTIBLE:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithNotPerceptibleBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithNotPerceptibleBindingOom,
                &reported_counts);
          }
          break;
      }
    }
  }

  if (info.process_type == content::PROCESS_TYPE_RENDERER &&
      (info.app_state ==
           base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
       info.app_state ==
           base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES) &&
      info.was_killed_intentionally_by_browser) {
    ReportCrashCount(ProcessedCrashCounts::kRendererForegroundIntentionalKill,
                     &reported_counts);
  }

  if (crashed) {
    if (info.process_type == content::PROCESS_TYPE_RENDERER) {
      ReportCrashCount(ProcessedCrashCounts::kRendererCrashAll,
                       &reported_counts);
    } else if (info.process_type == content::PROCESS_TYPE_GPU) {
      ReportCrashCount(ProcessedCrashCounts::kGpuCrashAll, &reported_counts);
    } else if (info.process_type == content::PROCESS_TYPE_UTILITY) {
      ReportCrashCount(ProcessedCrashCounts::kUtilityCrashAll,
                       &reported_counts);
    }
  }

  if (!info.was_killed_intentionally_by_browser && !crashed &&
      !info.normal_termination && info.renderer_shutdown_requested) {
    ReportCrashCount(ProcessedCrashCounts::kRendererProcessHostShutdown,
                     &reported_counts);
  }

  RecordSystemExitReason(info.pid, reported_counts);
  NotifyObservers(info.process_host_id, reported_counts);
}

void CrashMetricsReporter::NotifyObservers(
    int rph_id,
    const CrashMetricsReporter::ReportedCrashTypeSet& reported_counts) {
  async_observers_->Notify(
      FROM_HERE, &CrashMetricsReporter::Observer::OnCrashDumpProcessed, rph_id,
      reported_counts);
}

}  // namespace crash_reporter
