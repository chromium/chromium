// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_

#include "base/time/time.h"

// Utility functions to support metric collection for browser startup. Timings
// should use TimeTicks whenever possible. OS-provided timings are still
// received as Time out of cross-platform support necessity but are converted to
// TimeTicks as soon as possible in an attempt to reduce the potential skew
// between the two basis. See crbug.com/544131 for reasoning.

namespace startup_metric_utils {

// Resets this process's session to allow recording one-time-only metrics again
// when a process is reused for multiple tests.
void ResetSessionForTesting();

// Returns true when browser UI was not launched normally: some other UI was
// shown first or browser was launched in background mode.
bool WasMainWindowStartupInterrupted();

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

// Call this with the creation time of the startup (initial/main) process.
void RecordStartupProcessCreationTime(base::Time time);
void RecordStartupProcessCreationTime(base::TimeTicks ticks);

// Call this with a time recorded as early as possible in the startup process.
// On Android, the application start is the time at which the Java code starts.
// On Windows, the application start is sampled from chrome.exe:main, before
// chrome.dll is loaded.
void RecordApplicationStartTime(base::TimeTicks ticks);

// Call this with the time when the executable is loaded and the ChromeMain()
// function is invoked.
void RecordChromeMainEntryTime(base::TimeTicks ticks);

// Call this with the time recorded just before the message loop is started.
// Must be called after RecordApplicationStartTime(), because it computes time
// deltas based on application start time.
// |is_first_run| - is the current launch part of a first run.
void RecordBrowserMainMessageLoopStart(base::TimeTicks ticks,
                                       bool is_first_run);

// Call this with the time recorded just after the message loop first reaches
// idle. Must be called after RecordApplicationStartTime(), because it computes
// time deltas based on application start time.
void RecordBrowserMainLoopFirstIdle(base::TimeTicks ticks);

// Call this with the time when the first browser window became visible.
void RecordBrowserWindowDisplay(base::TimeTicks ticks);

// Call this with the time when the first web contents had a non-empty paint,
// only if the first web contents was unimpeded in its attempt to do so. Must be
// called after RecordApplicationStartTime(), because it computes time deltas
// based on application start time.
void RecordFirstWebContentsNonEmptyPaint(
    base::TimeTicks now,
    base::TimeTicks render_process_host_init_time);

// Call this with the time when the first web contents began navigating its main
// frame / successfully committed its navigation for the main frame. These
// functions must be called after RecordApplicationStartTime(), because they
// compute time deltas based on application start time.
void RecordFirstWebContentsMainNavigationStart(base::TimeTicks ticks);
void RecordFirstWebContentsMainNavigationFinished(base::TimeTicks ticks);

// Call this with the time when the Browser window painted its children for the
// first time. Must be called after RecordApplicationStartTime(), because it
// computes time deltas based on application start time.
void RecordBrowserWindowFirstPaint(base::TimeTicks ticks);

// Returns the TimeTicks corresponding to main entry as recorded by
// |RecordMainEntryPointTime|. Returns a null TimeTicks if a value has not been
// recorded yet. This method is expected to be called from the UI thread.
base::TimeTicks MainEntryPointTicks();

// Call this to record an arbitrary startup timing histogram with startup
// temperature and a trace event. Records the time between `completion_ticks`
// and the application start.
// See the `StartupTemperature` enum for definition of the startup temperature.
// A metric logged using this function must have an affected-histogram entry in
// the definition of the StartupTemperature suffix in histograms.xml.
// Set `set_non_browser_ui_displayed` to true if the recorded event blocks the
// browser UI on user input. In this case any future startup histogram timing
// would be skewed and will not be recorded.
// This function must be called after RecordApplicationStartTime(), because it
// computes time deltas based on application start time.
// `histogram_name` must point to a statically allocated string (such as a
// string literal) since the pointer will be stored for an indefinite amount of
// time before being written to a trace (see the "Memory scoping note" in
// base/trace_event/common/trace_event_common.h.)
void RecordExternalStartupMetric(const char* histogram_name,
                                 base::TimeTicks completion_ticks,
                                 bool set_non_browser_ui_displayed);

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
