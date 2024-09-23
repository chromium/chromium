// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_METRICS_REPORTER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_METRICS_REPORTER_ANDROID_H_

#include "base/containers/flat_set.h"
#include "base/observer_list_threadsafe.h"
#include "components/crash/content/browser/child_exit_observer_android.h"

namespace crash_reporter {

// Reports crash metrics about child processes to UMA, which is used as ground
// truth for child process stability. This class should be used by any code that
// wants to observe reason for the death of a child process.
class CrashMetricsReporter {
 public:
  // This enum is used to back a UMA histogram, and must be treated as
  // append-only.
  enum ExitStatus {
    EMPTY_MINIDUMP_WHILE_RUNNING,
    EMPTY_MINIDUMP_WHILE_PAUSED,
    EMPTY_MINIDUMP_WHILE_BACKGROUND,
    VALID_MINIDUMP_WHILE_RUNNING,
    VALID_MINIDUMP_WHILE_PAUSED,
    VALID_MINIDUMP_WHILE_BACKGROUND,
    MINIDUMP_STATUS_COUNT
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ProcessedCrashCounts {
    kGpuForegroundOom = 0,
    kRendererForegroundVisibleOom = 1,
    kRendererForegroundIntentionalKill = 2,
    kRendererForegroundVisibleSubframeOom = 3,
    kRendererForegroundVisibleSubframeIntentionalKill = 4,
    kRendererForegroundVisibleCrash = 5,
    kRendererForegroundVisibleSubframeCrash = 6,
    kGpuCrashAll = 7,
    kRendererCrashAll = 8,
    kRendererForegroundVisibleMainFrameIntentionalKill = 9,
    kRendererForegroundVisibleNormalTermNoMinidump = 10,
    kRendererForegroundInvisibleWithStrongBindingKilled = 11,
    kRendererForegroundInvisibleWithStrongBindingOom = 12,
    kRendererForegroundInvisibleWithModerateBindingKilled = 13,
    kRendererForegroundInvisibleWithModerateBindingOom = 14,
    kRendererForegroundVisibleAllocationFailure = 15,
    kRendererAllocationFailureAll = 16,
    kUtilityForegroundOom = 17,
    kUtilityCrashAll = 18,
    kRendererProcessHostShutdown = 19,
    kRendererForegroundInvisibleWithVisibleBindingKilled = 20,
    kRendererForegroundInvisibleWithVisibleBindingOom = 21,
    kRendererForegroundInvisibleWithNotPerceptibleBindingKilled = 22,
    kRendererForegroundInvisibleWithNotPerceptibleBindingOom = 23,
    kRendererForegroundInvisibleWithWaivedBindingOom = 24,
    kRendererForegroundInvisibleWithWaivedBindingKilled = 25,
    kMaxValue = kRendererForegroundInvisibleWithWaivedBindingKilled
  };
  using ReportedCrashTypeSet = base::flat_set<ProcessedCrashCounts>;

  // Careful note: the CrashMetricsReporter observers are asynchronous, and are
  // notified via PostTask. This could be problematic with a large number of
  // observers. Consider using a middle-layer observer to fan out synchronously
  // to leaf observers if you need many objects listening to these messages.
  class Observer {
   public:
    // Called when child process is dead and minidump was processed.
    // |reported_counts| is a set of recorded metrics about child process
    // crashes. It could be empty if no metrics were recorded.
    virtual void OnCrashDumpProcessed(
        int rph_id,
        const ReportedCrashTypeSet& reported_counts) = 0;
  };

  static CrashMetricsReporter* GetInstance();

  CrashMetricsReporter(const CrashMetricsReporter&) = delete;
  CrashMetricsReporter& operator=(const CrashMetricsReporter&) = delete;

  // Can be called on any thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ChildProcessExited(
      const crash_reporter::ChildExitObserver::TerminationInfo& info);

 private:
  CrashMetricsReporter();
  ~CrashMetricsReporter();

  void NotifyObservers(int rph_id, const ReportedCrashTypeSet& reported_counts);

  scoped_refptr<base::ObserverListThreadSafe<CrashMetricsReporter::Observer>>
      async_observers_;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_METRICS_REPORTER_ANDROID_H_
