// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

// Utility functions to support metric collection for browser-process-specific
// startup. Timings should use TimeTicks whenever possible.

namespace startup_metric_utils {

// Result of an attempt to create the first run sentinel file.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FirstRunSentinelCreationResult {
  // The sentinel file was created without error.
  kSuccess = 0,
  // Obtaining the path to the sentinel file failed. Might indicate issues
  // in determining the user data dir.
  kFailedToGetPath = 1,
  // The sentinel file already exists. Can indicate that a switch to override
  // the first run state was used.
  kFilePathExists = 2,
  // File system error, writing the file failed.
  kFileSystemError = 3,
  kMaxValue = kFileSystemError,
};

class COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
    BrowserStartupMetricRecorder final {
 public:
  // Clears variables which would be set multiple times in the case of tests
  // being run multiple times in the same process.
  void ResetSessionForTesting();

  // Returns true when browser UI was not launched normally: some other UI was
  // shown first or browser was launched in background mode.
  bool WasMainWindowStartupInterrupted() const;

  // Call this when displaying UI that might potentially delay startup events.
  //
  // Note on usage: This function is idempotent and its overhead is low enough
  // in comparison with UI display that it's OK to call it on every
  // UI invocation regardless of whether the browser window has already
  // been displayed or not.
  void SetNonBrowserUIDisplayed();

  // Call this when background mode gets enabled, as it might delay startup
  // events.
  void SetBackgroundModeEnabled();

  // Call this with the time recorded just before the message loop is started.
  // Must be called after RecordApplicationStartTime(), because it computes time
  // deltas based on application start time.
  // |is_first_run| - is the current launch part of a first run.
  void RecordBrowserMainMessageLoopStart(base::TimeTicks ticks,
                                         bool is_first_run);

  // Call this with the time recorded just after the message loop first reaches
  // idle. Must be called after RecordApplicationStartTime(), because it
  // computes time deltas based on application start time.
  void RecordBrowserMainLoopFirstIdle(base::TimeTicks ticks);

  // Call this with the time when the first browser window became visible.
  void RecordBrowserWindowDisplay(base::TimeTicks ticks);

  // Call this with the time when the browser window paints its children for the
  // first time.
  void RecordBrowserWindowFirstPaintTicks(base::TimeTicks ticks);

  // Call this with the time when the Privacy Sandbox Attestations component
  // becomes ready for the first time.
  void RecordPrivacySandboxAttestationsFirstReady(base::TimeTicks ticks);

  // Call this with the time when a Privacy Sandbox API attestation is checked
  // for the first time.
  void RecordPrivacySandboxAttestationFirstCheck(base::TimeTicks ticks);

  // Call this with the time when the first web contents had a non-empty paint,
  // only if the first web contents was unimpeded in its attempt to do so. Must
  // be called after RecordApplicationStartTime(), because it computes time
  // deltas based on application start time.
  void RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks now,
      base::TimeTicks render_process_host_init_time);

  // Call this with the time when the first web contents began navigating its
  // main frame / successfully committed its navigation for the main frame.
  // These functions must be called after RecordApplicationStartTime(), because
  // they compute time deltas based on application start time.
  void RecordFirstWebContentsMainNavigationStart(base::TimeTicks ticks);
  void RecordFirstWebContentsMainNavigationFinished(base::TimeTicks ticks);

  // Call this with the time when the Browser window painted its children for
  // the first time. Must be called after RecordApplicationStartTime(), because
  // it computes time deltas based on application start time.
  void RecordBrowserWindowFirstPaint(base::TimeTicks ticks);

  void RecordFirstRunSentinelCreation(FirstRunSentinelCreationResult result);

  // On Windows, records the number of hard-faults that have occurred in the
  // current chrome.exe process since it was started. This is a nop on other
  // platforms.
  void RecordHardFaultHistogram();

  // Call this to record an arbitrary startup timing histogram with startup
  // temperature and a trace event. Records the time between
  // `completion_ticks` and the application start. See the
  // `StartupTemperature` enum for definition of the startup temperature. A
  // metric logged using this function must have an affected-histogram entry
  // in the definition of the StartupTemperature suffix in histograms.xml. Set
  // `set_non_browser_ui_displayed` to true if the recorded event blocks the
  // browser UI on user input. In this case any future startup histogram
  // timing would be skewed and will not be recorded. This function must be
  // called after RecordApplicationStartTime(), because it computes time
  // deltas based on application start time. `histogram_name` must point to a
  // statically allocated string (such as a string literal) since the pointer
  // will be stored for an indefinite amount of time before being written to a
  // trace (see the "Memory scoping note" in
  // base/trace_event/common/trace_event_common.h.)
  void RecordExternalStartupMetric(const char* histogram_name,
                                   base::TimeTicks completion_ticks,
                                   bool set_non_browser_ui_displayed);

  bool ShouldLogStartupHistogram() const;

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, the time at which the first browser window is opened may not
  // match the application start time mainly because the login screen is often
  // shown first and the user must log in before a browser window can be opened.
  // ChromeOS code can call this if it knows a browser window is being opened to
  // mark the start time for all `Startup.FirstWebContents.*` metrics. This is
  // a no-op if a start time has already been recorded.
  //
  // For all other platforms, `application_start_ticks_` is used for
  // `Startup.FirstWebContents.*` metrics.
  void RecordWebContentsStartTime(base::TimeTicks ticks);
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Returns the globally unique `BrowserStartupMetricRecorder`.
  friend COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
      BrowserStartupMetricRecorder& GetBrowser();

  // Only permit construction from within GetBrowser().
  BrowserStartupMetricRecorder() = default;

#if BUILDFLAG(IS_WIN)
  // Returns the hard fault count of the current process, or nullopt if it can't
  // be determined.
  std::optional<uint32_t> GetHardFaultCountForCurrentProcess();
#endif

  void RecordMessageLoopStartTicks(base::TimeTicks ticks);

  base::TimeTicks GetWebContentsStartTicks() const;

  void EmitHistogramWithTemperatureAndTraceEvent(
      void (*histogram_function)(const std::string& name, base::TimeDelta),
      const char* histogram_basename,
      base::TimeTicks begin_ticks,
      base::TimeTicks end_ticks);

  // Mark as volatile to defensively make sure usage is thread-safe.
  // Note that at the time of this writing, access is only on the UI thread.
  volatile bool main_window_startup_interrupted_ = false;

  base::TimeTicks message_loop_start_ticks_;

  base::TimeTicks browser_window_display_ticks_;

  base::TimeTicks browser_window_first_paint_ticks_;

  // May be null, in which case, fall back to
  // `CommonStartupMetricRecorder.application_start_ticks_`.
  base::TimeTicks web_contents_start_ticks_;

  bool is_privacy_sandbox_attestations_component_ready_recorded_ = false;

  bool is_privacy_sandbox_attestations_first_check_recorded_ = false;
};

COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
BrowserStartupMetricRecorder& GetBrowser();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
