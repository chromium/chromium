// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "components/crash/content/browser/process_exit_reason_from_system_android.h"

namespace crash_reporter {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BindingStateCombo {
  kNoWaivedNoModerateNoStrong = 0,
  kNoWaivedNoModerateHasStrong = 1,
  kNoWaivedHasModerateNoStrong = 2,
  kNoWaivedHasModerateHasStrong = 3,
  kHasWaivedNoModerateNoStrong = 4,
  kHasWaivedNoModerateHasStrong = 5,
  kHasWaivedHasModerateNoStrong = 6,
  kHasWaivedHasModerateHasStrong = 7,
  kMaxValue = kHasWaivedHasModerateHasStrong
};

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
  const uint64_t private_footprint_kb =
      info.blink_oom_metrics.current_private_footprint_kb;

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
    if (app_foreground && renderer_visible)
      ReportCrashCount(
          ProcessedCrashCounts::kRendererForegroundVisibleAllocationFailure,
          &reported_counts);
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
        // Report memory metrics when visible foreground renderer is OOM.
        if (private_footprint_kb > 0) {
          // Report only when the metrics are not non-0, because the metrics
          // are recorded only when oom intervention is on.
          UMA_HISTOGRAM_MEMORY_LARGE_MB(
              "Memory.Experimental.OomIntervention."
              "RendererPrivateMemoryFootprintAtOOM",
              private_footprint_kb / 1024);
        }
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
