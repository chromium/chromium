// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include <windows.h>
#include <winternl.h>
#include "base/win/win_util.h"
#endif

// Data from deprecated UMA histograms available at
// https://docs.google.com/document/d/18uYnVwLly7C_ckGsDbqdNs-AgAAt3AmUmn7wYLkyBN0/edit?usp=sharing

namespace startup_metric_utils {

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
volatile bool g_main_window_startup_interrupted = false;

base::TimeTicks g_process_creation_ticks;

base::TimeTicks g_browser_main_entry_point_ticks;

base::TimeTicks g_renderer_main_entry_point_ticks;

base::TimeTicks g_browser_exe_main_entry_point_ticks;

base::TimeTicks g_message_loop_start_ticks;

base::TimeTicks g_browser_window_display_ticks;

base::TimeDelta g_browser_open_tabs_duration = base::TimeDelta::Max();

// An enumeration of startup temperatures. This must be kept in sync with the
// UMA StartupType enumeration defined in histograms.xml.
enum StartupTemperature {
  // The startup was a cold start: nearly all of the binaries and resources were
  // brought into memory using hard faults.
  COLD_STARTUP_TEMPERATURE = 0,
  // The startup was a warm start: the binaries and resources were mostly
  // already resident in memory and effectively no hard faults were observed.
  WARM_STARTUP_TEMPERATURE = 1,
  // The startup type couldn't quite be classified as warm or cold, but rather
  // was somewhere in between.
  LUKEWARM_STARTUP_TEMPERATURE = 2,
  // This must be after all meaningful values. All new values should be added
  // above this one.
  STARTUP_TEMPERATURE_COUNT,
  // Startup temperature wasn't yet determined.
  UNDETERMINED_STARTUP_TEMPERATURE
};

StartupTemperature g_startup_temperature = UNDETERMINED_STARTUP_TEMPERATURE;

#if defined(OS_WIN)

// These values are taken from the Startup.BrowserMessageLoopStartHardFaultCount
// histogram. If the cold start histogram starts looking strongly bimodal it may
// be because the binary/resource sizes have grown significantly larger than
// when these values were set. In this case the new values need to be chosen
// from the original histogram.
//
// Maximum number of hard faults tolerated for a startup to be classified as a
// warm start. Set at roughly the 40th percentile of the HardFaultCount
// histogram.
constexpr uint32_t kWarmStartHardFaultCountThreshold = 5;
// Minimum number of hard faults expected for a startup to be classified as a
// cold start. Set at roughly the 60th percentile of the HardFaultCount
// histogram.
constexpr uint32_t kColdStartHardFaultCountThreshold = 1200;

// The struct used to return system process information via the NT internal
// QuerySystemInformation call. This is partially documented at
// http://goo.gl/Ja9MrH and fully documented at http://goo.gl/QJ70rn
// This structure is laid out in the same format on both 32-bit and 64-bit
// systems, but has a different size due to the various pointer-sized fields.
struct SYSTEM_PROCESS_INFORMATION_EX {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize;
  ULONG HardFaultCount;
  BYTE Reserved1[36];
  PVOID Reserved2[3];
  // This is labeled a handle so that it expands to the correct size for 32-bit
  // and 64-bit operating systems. However, under the hood it's a 32-bit DWORD
  // containing the process ID.
  HANDLE UniqueProcessId;
  PVOID Reserved3;
  ULONG HandleCount;
  BYTE Reserved4[4];
  PVOID Reserved5[11];
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  LARGE_INTEGER Reserved6[6];
  // Array of SYSTEM_THREAD_INFORMATION structs follows.
};

// The signature of the NtQuerySystemInformation function.
typedef NTSTATUS (WINAPI *NtQuerySystemInformationPtr)(
    SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

// Gets the hard fault count of the current process through |hard_fault_count|.
// Returns true on success.
bool GetHardFaultCountForCurrentProcess(uint32_t* hard_fault_count) {
  DCHECK(hard_fault_count);

  // Get the function pointer.
  static const NtQuerySystemInformationPtr query_sys_info =
      reinterpret_cast<NtQuerySystemInformationPtr>(::GetProcAddress(
          GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation"));
  if (query_sys_info == nullptr)
    return false;

  // The output of this system call depends on the number of threads and
  // processes on the entire system, and this can change between calls. Retry
  // a small handful of times growing the buffer along the way.
  // NOTE: The actual required size depends entirely on the number of processes
  //       and threads running on the system. The initial guess suffices for
  //       ~100s of processes and ~1000s of threads.
  std::vector<uint8_t> buffer(32 * 1024);
  for (size_t tries = 0; tries < 3; ++tries) {
    ULONG return_length = 0;
    const NTSTATUS status =
        query_sys_info(SystemProcessInformation, buffer.data(),
                       static_cast<ULONG>(buffer.size()), &return_length);
    // Insufficient space in the buffer.
    if (return_length > buffer.size()) {
      buffer.resize(return_length);
      continue;
    }
    if (NT_SUCCESS(status) && return_length <= buffer.size())
      break;
    return false;
  }

  // Look for the struct housing information for the current process.
  const DWORD proc_id = ::GetCurrentProcessId();
  size_t index = 0;
  while (index < buffer.size()) {
    DCHECK_LE(index + sizeof(SYSTEM_PROCESS_INFORMATION_EX), buffer.size());
    SYSTEM_PROCESS_INFORMATION_EX* proc_info =
        reinterpret_cast<SYSTEM_PROCESS_INFORMATION_EX*>(buffer.data() + index);
    if (base::win::HandleToUint32(proc_info->UniqueProcessId) == proc_id) {
      *hard_fault_count = proc_info->HardFaultCount;
      return true;
    }
    // The list ends when NextEntryOffset is zero. This also prevents busy
    // looping if the data is in fact invalid.
    if (proc_info->NextEntryOffset <= 0)
      return false;
    index += proc_info->NextEntryOffset;
  }

  return false;
}
#endif  // defined(OS_WIN)

#define UMA_HISTOGRAM_TIME_IN_MINUTES_MONTH_RANGE(name, sample) \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, 1,                  \
                              base::TimeDelta::FromDays(30).InMinutes(), 50)

// Helper macro for splitting out an UMA histogram based on startup temperature.
// |type| is the histogram type, and corresponds to an UMA macro like
// UMA_HISTOGRAM_LONG_TIMES. It must itself be a macro that only takes two
// parameters.
// |basename| is the basename of the histogram. A histogram of this name will
// always be recorded to. If the startup temperature is known then a value will
// also be recorded to the histogram with name |basename| and suffix
// ".ColdStart", ".WarmStart" or ".LukewarmStartup" as appropriate.
// |value_expr| is an expression evaluating to the value to be recorded. This
// will be evaluated exactly once and cached, so side effects are not an issue.
// A metric logged using this macro must have an affected-histogram entry in the
// definition of the StartupTemperature suffix in histograms.xml.
// This macro must only be used in code that runs after |g_startup_temperature|
// has been initialized.
#define UMA_HISTOGRAM_WITH_TEMPERATURE(type, basename, value_expr)            \
  do {                                                                        \
    const auto value = value_expr;                                            \
    /* Always record to the base histogram. */                                \
    type(basename, value);                                                    \
    /* Record to the cold/warm/lukewarm suffixed histogram as appropriate. */ \
    switch (g_startup_temperature) {                                          \
      case COLD_STARTUP_TEMPERATURE:                                          \
        type(basename ".ColdStartup", value);                                 \
        break;                                                                \
      case WARM_STARTUP_TEMPERATURE:                                          \
        type(basename ".WarmStartup", value);                                 \
        break;                                                                \
      case LUKEWARM_STARTUP_TEMPERATURE:                                      \
        type(basename ".LukewarmStartup", value);                             \
        break;                                                                \
      case UNDETERMINED_STARTUP_TEMPERATURE:                                  \
        break;                                                                \
      case STARTUP_TEMPERATURE_COUNT:                                         \
        NOTREACHED();                                                         \
        break;                                                                \
    }                                                                         \
  } while (0)

#define UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(type, basename, begin_ticks, \
                                                 end_ticks)                   \
  do {                                                                        \
    UMA_HISTOGRAM_WITH_TEMPERATURE(type, basename, end_ticks - begin_ticks);  \
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(                                  \
        "startup", basename, 0, begin_ticks, "Temperature",                   \
        g_startup_temperature);                                               \
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP1(                                    \
        "startup", basename, 0, end_ticks, "Temperature",                     \
        g_startup_temperature);                                               \
  } while (0)

// Returns the system uptime on process launch.
base::TimeDelta GetSystemUptimeOnProcessLaunch() {
  // Process launch time is not available on Android.
  if (g_process_creation_ticks.is_null())
    return base::TimeDelta();

  // base::SysInfo::Uptime returns the time elapsed between system boot and now.
  // Substract the time elapsed between process launch and now to get the time
  // elapsed between system boot and process launch.
  return base::SysInfo::Uptime() -
         (base::TimeTicks::Now() - g_process_creation_ticks);
}

void RecordSystemUptimeHistogram() {
  const base::TimeDelta system_uptime_on_process_launch =
      GetSystemUptimeOnProcessLaunch();
  if (system_uptime_on_process_launch.is_zero())
    return;

  UMA_HISTOGRAM_WITH_TEMPERATURE(UMA_HISTOGRAM_LONG_TIMES_100,
                                 "Startup.SystemUptime",
                                 GetSystemUptimeOnProcessLaunch());
}

// On Windows, records the number of hard-faults that have occurred in the
// current chrome.exe process since it was started. This is a nop on other
// platforms.
void RecordHardFaultHistogram() {
#if defined(OS_WIN)
  uint32_t hard_fault_count = 0;

  // Don't record histograms if unable to get the hard fault count.
  if (!GetHardFaultCountForCurrentProcess(&hard_fault_count))
    return;

  // Hard fault counts are expected to be in the thousands range,
  // corresponding to faulting in ~10s of MBs of code ~10s of KBs at a time.
  // (Observed to vary from 1000 to 10000 on various test machines and
  // platforms.)
  UMA_HISTOGRAM_CUSTOM_COUNTS("Startup.BrowserMessageLoopStartHardFaultCount",
                              hard_fault_count, 1, 40000, 50);

  // Determine the startup type based on the number of observed hard faults.
  DCHECK_EQ(UNDETERMINED_STARTUP_TEMPERATURE, g_startup_temperature);
  if (hard_fault_count < kWarmStartHardFaultCountThreshold) {
    g_startup_temperature = WARM_STARTUP_TEMPERATURE;
  } else if (hard_fault_count >= kColdStartHardFaultCountThreshold) {
    g_startup_temperature = COLD_STARTUP_TEMPERATURE;
  } else {
    g_startup_temperature = LUKEWARM_STARTUP_TEMPERATURE;
  }

  // Record the startup 'temperature'.
  UMA_HISTOGRAM_ENUMERATION("Startup.Temperature", g_startup_temperature,
                            STARTUP_TEMPERATURE_COUNT);
#endif  // defined(OS_WIN)
}

// Converts a base::Time value to a base::TimeTicks value. The conversion isn't
// exact, but by capturing Time::Now() as early as possible, the likelihood of a
// clock change between it and process start is as low as possible. There is
// also the time taken to synchronously resolve base::Time::Now() and
// base::TimeTicks::Now() at play, but in practice it is pretty much instant
// compared to multi-seconds startup timings.
base::TimeTicks StartupTimeToTimeTicks(base::Time time) {
// First get a base which represents the same point in time in both units.
// Bump the priority of this thread while doing this as the wall clock time it
// takes to resolve these two calls affects the precision of this method and
// bumping the priority reduces the likelihood of a context switch interfering
// with this computation.

// Enabling this logic on OS X causes a significant performance regression.
// https://crbug.com/601270
#if !defined(OS_MACOSX)
  static bool statics_initialized = false;

  base::ThreadPriority previous_priority = base::ThreadPriority::NORMAL;
  if (!statics_initialized) {
    previous_priority = base::PlatformThread::GetCurrentThreadPriority();
    base::PlatformThread::SetCurrentThreadPriority(
        base::ThreadPriority::DISPLAY);
  }
#endif

  static const base::Time time_base = base::Time::Now();
  static const base::TimeTicks trace_ticks_base = base::TimeTicks::Now();

#if !defined(OS_MACOSX)
  if (!statics_initialized) {
    base::PlatformThread::SetCurrentThreadPriority(previous_priority);
  }
  statics_initialized = true;
#endif

  // Then use the TimeDelta common ground between the two units to make the
  // conversion.
  const base::TimeDelta delta_since_base = time_base - time;
  return trace_ticks_base - delta_since_base;
}

void RecordRendererMainEntryHistogram() {
  if (!g_browser_main_entry_point_ticks.is_null() &&
      !g_renderer_main_entry_point_ticks.is_null()) {
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES_100, "Startup.BrowserMainToRendererMain",
        g_browser_main_entry_point_ticks, g_renderer_main_entry_point_ticks);
  }
}

void AddStartupEventsForTelemetry()
{
  DCHECK(!g_browser_main_entry_point_ticks.is_null());

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0("startup",
                                      "Startup.BrowserMainEntryPoint", 0,
                                      g_browser_main_entry_point_ticks);
}

bool ShouldLogStartupHistogram() {
  return !WasMainWindowStartupInterrupted() &&
         !g_process_creation_ticks.is_null();
}

}  // namespace

bool WasMainWindowStartupInterrupted() {
  return g_main_window_startup_interrupted;
}

void SetNonBrowserUIDisplayed() {
  g_main_window_startup_interrupted = true;
}

void SetBackgroundModeEnabled() {
  g_main_window_startup_interrupted = true;
}

void RecordStartupProcessCreationTime(base::Time time) {
  DCHECK(g_process_creation_ticks.is_null());
  g_process_creation_ticks = StartupTimeToTimeTicks(time);
  DCHECK(!g_process_creation_ticks.is_null());
}

void RecordMainEntryPointTime(base::TimeTicks ticks) {
  DCHECK(g_browser_main_entry_point_ticks.is_null());
  g_browser_main_entry_point_ticks = ticks;
  DCHECK(!g_browser_main_entry_point_ticks.is_null());
}

void RecordExeMainEntryPointTicks(base::TimeTicks ticks) {
  DCHECK(g_browser_exe_main_entry_point_ticks.is_null());
  g_browser_exe_main_entry_point_ticks = ticks;
  DCHECK(!g_browser_exe_main_entry_point_ticks.is_null());
}

void RecordMessageLoopStartTicks(base::TimeTicks ticks) {
  DCHECK(g_message_loop_start_ticks.is_null());
  g_message_loop_start_ticks = ticks;
  DCHECK(!g_message_loop_start_ticks.is_null());
}

void RecordBrowserMainMessageLoopStart(base::TimeTicks ticks,
                                       bool is_first_run) {
  RecordMessageLoopStartTicks(ticks);

  // Keep RecordHardFaultHistogram() near the top of this method (as much as
  // possible) as many other histograms depend on it setting
  // |g_startup_temperature|.
  RecordHardFaultHistogram();

  // Record timing of the browser message-loop start time.
  if (!is_first_run && !g_process_creation_ticks.is_null()) {
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES_100, "Startup.BrowserMessageLoopStartTime",
        g_process_creation_ticks, ticks);
  }

  // Record timing between the shared library's main() entry and the browser
  // main message loop start.
  if (is_first_run) {
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES,
        "Startup.BrowserMessageLoopStartTimeFromMainEntry.FirstRun2",
        g_browser_main_entry_point_ticks, ticks);
  } else {
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES,
        "Startup.BrowserMessageLoopStartTimeFromMainEntry3",
        g_browser_main_entry_point_ticks, ticks);
  }

  AddStartupEventsForTelemetry();
  RecordSystemUptimeHistogram();

  // Record values stored prior to startup temperature evaluation.
  if (ShouldLogStartupHistogram()) {
    if (!g_browser_open_tabs_duration.is_max()) {
      UMA_HISTOGRAM_WITH_TEMPERATURE(UMA_HISTOGRAM_LONG_TIMES_100,
                                     "Startup.BrowserOpenTabs",
                                     g_browser_open_tabs_duration);
    }

    if (!g_browser_window_display_ticks.is_null()) {
      UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
          UMA_HISTOGRAM_LONG_TIMES, "Startup.BrowserWindowDisplay",
          g_process_creation_ticks, g_browser_window_display_ticks);
    }
  }

  // Record timings between process creation, the main() in the executable being
  // reached and the main() in the shared library being reached.
  if (!g_process_creation_ticks.is_null() &&
      !g_browser_exe_main_entry_point_ticks.is_null()) {
    // Process create to chrome.exe:main().
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ProcessCreateToExeMain2",
        g_process_creation_ticks, g_browser_exe_main_entry_point_ticks);

    // chrome.exe:main() to chrome.dll:main().
    UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ExeMainToDllMain2",
        g_browser_exe_main_entry_point_ticks, g_browser_main_entry_point_ticks);

    // Process create to chrome.dll:main(). Reported as a histogram only as
    // the other two events above are sufficient for tracing purposes.
    UMA_HISTOGRAM_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES, "Startup.LoadTime.ProcessCreateToDllMain2",
        g_browser_main_entry_point_ticks - g_process_creation_ticks);
  }
}

void RecordBrowserWindowDisplay(base::TimeTicks ticks) {
  DCHECK(!ticks.is_null());

  if (!g_browser_window_display_ticks.is_null())
    return;

  // The value will be recorded in appropriate histograms after the startup
  // temperature is evaluated.
  //
  // Note: In some cases (e.g. launching with --silent-launch), the first
  // browser window is displayed after the startup temperature is evaluated. In
  // these cases, the value will not be recorded, which is the desired behavior
  // for a non-conventional launch.
  g_browser_window_display_ticks = ticks;
}

void RecordBrowserOpenTabsDelta(base::TimeDelta delta) {
  DCHECK(g_browser_open_tabs_duration.is_max());
  DCHECK_EQ(g_startup_temperature, UNDETERMINED_STARTUP_TEMPERATURE);
  // The value will be recorded in appropriate histograms after the startup
  // temperature is evaluated.
  g_browser_open_tabs_duration = delta;
}

void RecordRendererMainEntryTime(base::TimeTicks ticks) {
  // Record the renderer main entry time, but don't log the UMA metric
  // immediately because the startup temperature is not known yet.
  if (g_renderer_main_entry_point_ticks.is_null())
    g_renderer_main_entry_point_ticks = ticks;
}

void RecordFirstWebContentsNonEmptyPaint(
    base::TimeTicks now,
    base::TimeTicks render_process_host_init_time) {
  static bool is_first_call = true;
  if (!is_first_call || now.is_null())
    return;
  is_first_call = false;

  // Log Startup.BrowserMainToRendererMain now that the first renderer main
  // entry time and the startup temperature are known.
  RecordRendererMainEntryHistogram();

  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100, "Startup.FirstWebContents.NonEmptyPaint2",
      g_process_creation_ticks, now);
  UMA_HISTOGRAM_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100,
      "Startup.BrowserMessageLoopStart.To.NonEmptyPaint2",
      now - g_message_loop_start_ticks);

  UMA_HISTOGRAM_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100,
      "Startup.FirstWebContents.RenderProcessHostInit.ToNonEmptyPaint",
      now - render_process_host_init_time);
}

void RecordFirstWebContentsMainNavigationStart(base::TimeTicks ticks,
                                               WebContentsWorkload workload) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100,
      "Startup.FirstWebContents.MainNavigationStart", g_process_creation_ticks,
      ticks);

  // Log extra information about this startup's workload. Only added to this
  // histogram as this extra suffix can help making it less noisy but isn't
  // worth tripling the number of startup histograms either.
  if (workload == WebContentsWorkload::SINGLE_TAB) {
    UMA_HISTOGRAM_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES_100,
        "Startup.FirstWebContents.MainNavigationStart.SingleTab",
        ticks - g_process_creation_ticks);
  } else {
    UMA_HISTOGRAM_WITH_TEMPERATURE(
        UMA_HISTOGRAM_LONG_TIMES_100,
        "Startup.FirstWebContents.MainNavigationStart.MultiTabs",
        ticks - g_process_creation_ticks);
  }
}

void RecordFirstWebContentsMainNavigationFinished(base::TimeTicks ticks) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100,
      "Startup.FirstWebContents.MainNavigationFinished",
      g_process_creation_ticks, ticks);
}

void RecordBrowserWindowFirstPaint(base::TimeTicks ticks) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(UMA_HISTOGRAM_LONG_TIMES_100,
                                           "Startup.BrowserWindow.FirstPaint",
                                           g_process_creation_ticks, ticks);
}

void RecordBrowserWindowFirstPaintCompositingEnded(
    const base::TimeTicks ticks) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_LONG_TIMES_100,
      "Startup.BrowserWindow.FirstPaint.CompositingEnded",
      g_process_creation_ticks, ticks);
}

base::TimeTicks MainEntryPointTicks() {
  return g_browser_main_entry_point_ticks;
}

void RecordWebFooterDidFirstVisuallyNonEmptyPaint(base::TimeTicks ticks) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_MEDIUM_TIMES,
      "Startup.WebFooterExperiment.DidFirstVisuallyNonEmptyPaint",
      g_process_creation_ticks, ticks);
}

void RecordWebFooterCreation(base::TimeTicks ticks) {
  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UMA_HISTOGRAM_AND_TRACE_WITH_TEMPERATURE(
      UMA_HISTOGRAM_MEDIUM_TIMES,
      "Startup.WebFooterExperiment.WebFooterCreation", g_process_creation_ticks,
      ticks);
}

}  // namespace startup_metric_utils
