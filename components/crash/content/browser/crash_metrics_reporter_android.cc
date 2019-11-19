// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_metrics_reporter_android.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/rand_util.h"

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

void ReportLegacyCrashUma(const ChildExitObserver::TerminationInfo& info,
                          bool has_valid_dump) {
  // TODO(wnwen): If these numbers match up to TabWebContentsObserver's
  //     TabRendererCrashStatus histogram, then remove that one as this is more
  //     accurate with more detail.
  if ((info.process_type == content::PROCESS_TYPE_RENDERER ||
       info.process_type == content::PROCESS_TYPE_GPU) &&
      info.app_state != base::android::APPLICATION_STATE_UNKNOWN) {
    CrashMetricsReporter::ExitStatus exit_status;
    bool is_running = (info.app_state ==
                       base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    bool is_paused = (info.app_state ==
                      base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
    if (!has_valid_dump) {
      if (is_running) {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = CrashMetricsReporter::EMPTY_MINIDUMP_WHILE_BACKGROUND;
      }
    } else {
      if (is_running) {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = CrashMetricsReporter::VALID_MINIDUMP_WHILE_BACKGROUND;
      }
    }
    if (info.process_type == content::PROCESS_TYPE_RENDERER) {
      if (info.was_oom_protected_status) {
        UMA_HISTOGRAM_ENUMERATION(
            "Tab.RendererDetailedExitStatus", exit_status,
            CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Tab.RendererDetailedExitStatusUnbound", exit_status,
            CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
      }
    } else if (info.process_type == content::PROCESS_TYPE_GPU) {
      UMA_HISTOGRAM_ENUMERATION(
          "GPU.GPUProcessDetailedExitStatus", exit_status,
          CrashMetricsReporter::ExitStatus::MINIDUMP_STATUS_COUNT);
    }
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

CrashMetricsReporter::~CrashMetricsReporter() {}

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
                                !crashed && !info.normal_termination;
  const bool renderer_visible = info.renderer_has_visible_clients;
  const bool renderer_subframe = info.renderer_was_subframe;
  const bool renderer_allocation_failed =
      info.blink_oom_metrics.allocation_failed;
  const uint64_t private_footprint_kb =
      info.blink_oom_metrics.current_private_footprint_kb;
  const uint64_t swap_kb = info.blink_oom_metrics.current_swap_kb;
  const uint64_t vm_size_kb = info.blink_oom_metrics.current_vm_size_kb;
  const uint64_t blink_usage_kb = info.blink_oom_metrics.current_blink_usage_kb;

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
      } else {
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
          UMA_HISTOGRAM_MEMORY_MB(
              "Memory.Experimental.OomIntervention.RendererSwapFootprintAtOOM",
              swap_kb / 1024);
          UMA_HISTOGRAM_MEMORY_MB(
              "Memory.Experimental.OomIntervention.RendererBlinkUsageAtOOM",
              blink_usage_kb / 1024);
          UMA_HISTOGRAM_MEMORY_LARGE_MB(
              "Memory.Experimental.OomIntervention.RendererVmSizeAtOOMLarge",
              vm_size_kb / 1024);
        }
      }
    } else if (!crashed) {
      // Record stats when renderer is not visible, but the process has oom
      // protected bindings. This case occurs when a tab is switched or closed,
      // the bindings are updated later than visibility on web contents.
      switch (info.binding_state) {
        case base::android::ChildBindingState::UNBOUND:
        case base::android::ChildBindingState::WAIVED:
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
        case base::android::ChildBindingState::MODERATE:
          if (intentional_kill || info.normal_termination) {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithModerateBindingKilled,
                &reported_counts);
          } else {
            ReportCrashCount(
                ProcessedCrashCounts::
                    kRendererForegroundInvisibleWithModerateBindingOom,
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

  if (app_foreground && android_oom_kill &&
      info.binding_state == base::android::ChildBindingState::STRONG) {
    const bool has_waived = info.remaining_process_with_waived_binding > 0;
    const bool has_moderate = info.remaining_process_with_moderate_binding > 0;
    const bool has_strong = info.remaining_process_with_strong_binding > 0;
    BindingStateCombo combo;
    if (has_waived && has_moderate) {
      combo = has_strong ? BindingStateCombo::kHasWaivedHasModerateHasStrong
                         : BindingStateCombo::kHasWaivedHasModerateNoStrong;
    } else if (has_waived) {
      combo = has_strong ? BindingStateCombo::kHasWaivedNoModerateHasStrong
                         : BindingStateCombo::kHasWaivedNoModerateNoStrong;
    } else if (has_moderate) {
      combo = has_strong ? BindingStateCombo::kNoWaivedHasModerateHasStrong
                         : BindingStateCombo::kNoWaivedHasModerateNoStrong;
    } else {
      combo = has_strong ? BindingStateCombo::kNoWaivedNoModerateHasStrong
                         : BindingStateCombo::kNoWaivedNoModerateNoStrong;
    }
    UMA_HISTOGRAM_ENUMERATION(
        "Stability.Android.StrongBindingOomRemainingBindingState", combo);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Stability.Android.StrongBindingOomRemainingStrongBindingCount",
        info.remaining_process_with_strong_binding, 20);
  }

  if (android_oom_kill) {
    if (info.best_effort_reverse_rank >= 0) {
      UMA_HISTOGRAM_EXACT_LINEAR("Stability.Android.OomKillReverseRank",
                                 info.best_effort_reverse_rank, 50);
    }
    if (info.best_effort_reverse_rank != -2) {
      UMA_HISTOGRAM_BOOLEAN("Stability.Android.OomKillReverseRankSuccess",
                            info.best_effort_reverse_rank != -1);
    }
  }

  ReportLegacyCrashUma(info, crashed);
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
