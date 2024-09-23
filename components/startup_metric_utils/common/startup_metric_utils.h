// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_COMMON_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_COMMON_STARTUP_METRIC_UTILS_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/time/time.h"

// Utility functions to support metric collection for startup common across
// processes. Timings should use TimeTicks whenever possible. OS-provided
// timings are still received as Time out of cross-platform support necessity
// but are converted to TimeTicks as soon as possible in an attempt to reduce
// the potential skew between the two basis. See crbug.com/544131 for reasoning.

namespace startup_metric_utils {

class BrowserStartupMetricRecorder;
class GpuStartupMetricRecorder;
class RendererStartupMetricRecorder;

class COMPONENT_EXPORT(STARTUP_METRIC_UTILS) CommonStartupMetricRecorder final {
 public:
  // Call this with the creation time of the startup (initial/main) process.
  void RecordStartupProcessCreationTime(base::Time time);
  void RecordStartupProcessCreationTime(base::TimeTicks ticks);

  // Call this with a time recorded as early as possible in the startup process.
  // On Android, the application start is the time at which the Java code
  // starts. On Windows, the application start is sampled from chrome.exe:main,
  // before chrome.dll is loaded.
  void RecordApplicationStartTime(base::TimeTicks ticks);

  // Call this with the time when the executable is loaded and the ChromeMain()
  // function is invoked.
  void RecordChromeMainEntryTime(base::TimeTicks ticks);

  // Call this with the time immediately before/after base::PreReadFile() is
  // called on the main DLL on startup.
  void RecordPreReadTime(base::TimeTicks start_ticks,
                         base::TimeTicks end_ticks);

  // Returns the TimeTicks corresponding to main entry as recorded by
  // |RecordMainEntryPointTime|. Returns a null TimeTicks if a value has not
  // been recorded yet. This method is expected to be called from the UI
  // thread.
  base::TimeTicks MainEntryPointTicks() const;

 private:
  friend class BrowserStartupMetricRecorder;
  friend class GpuStartupMetricRecorder;
  friend class RendererStartupMetricRecorder;
  friend COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
      CommonStartupMetricRecorder& GetCommon();

  // Only permit construction from within GetCommon().
  CommonStartupMetricRecorder() = default;

  // Converts a base::Time value to a base::TimeTicks value. The conversion
  // isn't exact, but by capturing Time::Now() as early as possible, the
  // likelihood of a clock change between it and process start is as low as
  // possible. There is also the time taken to synchronously resolve
  // base::Time::Now() and base::TimeTicks::Now() at play, but in practice it is
  // pretty much instant compared to multi-seconds startup timings.
  base::TimeTicks StartupTimeToTimeTicks(base::Time time);

  void AddStartupEventsForTelemetry();

  // Call BrowserStartupMetricRecorder::ResetSessionForTesting.
  void ResetSessionForTesting();

#if DCHECK_IS_ON()
  base::flat_set<const void*>& GetSessionLog();
#endif  // DCHECK_IS_ON()

  // DCHECKs that this is the first time |method_id| is passed to this assertion
  // in this session (a session is typically process-lifetime but this can be
  // reset in tests via ResetSessionForTesting()). Callers should use FROM_HERE
  // as a unique id.
  void AssertFirstCallInSession(base::Location from_here);

  // Common helper to report "startup" category trace events as well as a
  // histogram of the same name. Example call:
  //     EmitHistogramWithTraceEvent(
  //         &base::UmaHistogramLongTimes,
  //         "Startup.LoadTime.ApplicationStartToChromeMain",
  //         GetCommon().application_start_ticks_,
  //         GetCommon().chrome_main_entry_ticks_);
  using HistogramTimeFunction = void(const char* name, base::TimeDelta);
  void EmitHistogramWithTraceEvent(HistogramTimeFunction* histogram_function,
                                   const char* name,
                                   base::TimeTicks begin_ticks,
                                   base::TimeTicks end_ticks);

  // Emit a "startup" category event.
  void EmitTraceEvent(const char* name,
                      base::TimeTicks begin_ticks,
                      base::TimeTicks end_ticks);

  // Emit info to the "startup" category in the form of an instant event.
  void EmitInstantEvent(const char* name);

  base::TimeTicks process_creation_ticks_;

  base::TimeTicks application_start_ticks_;

  base::TimeTicks chrome_main_entry_ticks_;

  base::TimeTicks preread_begin_ticks_;
  base::TimeTicks preread_end_ticks_;
};

COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
CommonStartupMetricRecorder& GetCommon();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_COMMON_STARTUP_METRIC_UTILS_H_
