// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
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

base::TimeTicks g_application_start_ticks;

base::TimeTicks g_chrome_main_entry_ticks;

base::TimeTicks g_message_loop_start_ticks;

base::TimeTicks g_browser_window_display_ticks;

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
  // Startup temperature wasn't yet determined, or could not be determined.
  UNDETERMINED_STARTUP_TEMPERATURE = 3,
  // This must be after all meaningful values. All new values should be added
  // above this one.
  STARTUP_TEMPERATURE_COUNT,
};

StartupTemperature g_startup_temperature = UNDETERMINED_STARTUP_TEMPERATURE;

#if BUILDFLAG(IS_WIN)

// These values are taken from the Startup.BrowserMessageLoopStartHardFaultCount
// histogram. The latest revision landed on <5 and >3500 for a good split
// of warm/cold. In between being considered "lukewarm". Full analysis @
// https://docs.google.com/document/d/1haXFN1cQ6XE-NfhKgww-rOP-Wi-gK6AczP3gT4M5_kI
// These values should be reconsidered if either .WarmStartup or .ColdStartup
// distributions of a suffixed histogram becomes unexplainably bimodal.
//
// Maximum number of hard faults tolerated for a startup to be classified as a
// warm start.
constexpr uint32_t kWarmStartHardFaultCountThreshold = 5;
// Minimum number of hard faults (of 4KB pages) expected for a startup to be
// classified as a cold start. The right value for this seems to be between 10%
// and 15% of chrome.dll's size (from anecdata of the two times we did this
// analysis... it was 1200 in M47 back when chrome.dll was 35MB (32-bit and
// split from chrome_child.dll) and was made 3500 in M81 when chrome.dll was
// 126MB).
constexpr uint32_t kColdStartHardFaultCountThreshold = 3500;

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

// Returns the hard fault count of the current process, or nullopt if it can't
// be determined.
absl::optional<uint32_t> GetHardFaultCountForCurrentProcess() {
  // Get the function pointer.
  static const NtQuerySystemInformationPtr query_sys_info =
      reinterpret_cast<NtQuerySystemInformationPtr>(::GetProcAddress(
          GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation"));
  if (query_sys_info == nullptr)
    return absl::nullopt;

  // The output of this system call depends on the number of threads and
  // processes on the entire system, and this can change between calls. Retry
  // a small handful of times growing the buffer along the way.
  // NOTE: The actual required size depends entirely on the number of processes
  //       and threads running on the system. The initial guess suffices for
  //       ~100s of processes and ~1000s of threads.
  std::vector<uint8_t> buffer(32 * 1024);
  constexpr int kMaxNumBufferResize = 2;
  int num_buffer_resize = 0;
  for (;;) {
    ULONG return_length = 0;
    const NTSTATUS status =
        query_sys_info(SystemProcessInformation, buffer.data(),
                       static_cast<ULONG>(buffer.size()), &return_length);

    // NtQuerySystemInformation succeeded.
    if (NT_SUCCESS(status)) {
      DCHECK_LE(return_length, buffer.size());
      break;
    }

    // NtQuerySystemInformation failed due to insufficient buffer length.
    if (return_length > buffer.size()) {
      // Abort if a large size is required for the buffer. It is undesirable to
      // fill a large buffer just to record histograms.
      constexpr ULONG kMaxLength = 512 * 1024;
      if (return_length >= kMaxLength)
        return absl::nullopt;

      // Resize the buffer and retry, if the buffer hasn't already been resized
      // too many times.
      if (num_buffer_resize < kMaxNumBufferResize) {
        ++num_buffer_resize;
        buffer.resize(return_length);
        continue;
      }
    }

    // Abort if NtQuerySystemInformation failed for another reason than
    // insufficient buffer length, or if the buffer was resized too many times.
    DCHECK(return_length <= buffer.size() ||
           num_buffer_resize >= kMaxNumBufferResize);
    return absl::nullopt;
  }

  // Look for the struct housing information for the current process.
  const DWORD proc_id = ::GetCurrentProcessId();
  size_t index = 0;
  while (index < buffer.size()) {
    DCHECK_LE(index + sizeof(SYSTEM_PROCESS_INFORMATION_EX), buffer.size());
    SYSTEM_PROCESS_INFORMATION_EX* proc_info =
        reinterpret_cast<SYSTEM_PROCESS_INFORMATION_EX*>(buffer.data() + index);
    if (base::win::HandleToUint32(proc_info->UniqueProcessId) == proc_id)
      return proc_info->HardFaultCount;
    // The list ends when NextEntryOffset is zero. This also prevents busy
    // looping if the data is in fact invalid.
    if (proc_info->NextEntryOffset <= 0)
      return absl::nullopt;
    index += proc_info->NextEntryOffset;
  }

  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_WIN)

// Helper function for splitting out an UMA histogram based on startup
// temperature. |histogram_function| is the histogram type, and corresponds to
// an UMA function like base::UmaHistogramLongTimes. It must itself be a
// function that only takes two parameters.
// |basename| is the basename of the histogram. A histogram of this name will
// always be recorded to. If the startup temperature is known then a value will
// also be recorded to the histogram with name |basename| and suffix
// ".ColdStart", ".WarmStart" as appropriate.
// |value_expr| is an expression evaluating to the value to be recorded. This
// will be evaluated exactly once and cached, so side effects are not an issue.
// A metric logged using this function must have an affected-histogram entry in
// the definition of the StartupTemperature suffix in histograms.xml.
// This function must only be used in code that runs after
// |g_startup_temperature| has been initialized.
template <typename T>
void UmaHistogramWithTemperature(
    void (*histogram_function)(const std::string& name, T),
    const std::string& histogram_basename,
    T value) {
  // Always record to the base histogram.
  (*histogram_function)(histogram_basename, value);
  // Record to the cold/warm suffixed histogram as appropriate.
  switch (g_startup_temperature) {
    case COLD_STARTUP_TEMPERATURE:
      (*histogram_function)(histogram_basename + ".ColdStartup", value);
      break;
    case WARM_STARTUP_TEMPERATURE:
      (*histogram_function)(histogram_basename + ".WarmStartup", value);
      break;
    case LUKEWARM_STARTUP_TEMPERATURE:
      // No suffix emitted for lukewarm startups.
      break;
    case UNDETERMINED_STARTUP_TEMPERATURE:
      break;
    case STARTUP_TEMPERATURE_COUNT:
      NOTREACHED();
      break;
  }
}

void UmaHistogramWithTraceAndTemperature(
    void (*histogram_function)(const std::string& name, base::TimeDelta),
    const char* histogram_basename,
    base::TimeTicks begin_ticks,
    base::TimeTicks end_ticks) {
  UmaHistogramWithTemperature(histogram_function, histogram_basename,
                              end_ticks - begin_ticks);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "startup", histogram_basename, TRACE_ID_WITH_SCOPE(histogram_basename, 0),
      begin_ticks, "Temperature", g_startup_temperature);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "startup", histogram_basename, TRACE_ID_WITH_SCOPE(histogram_basename, 0),
      end_ticks);
}

// On Windows, records the number of hard-faults that have occurred in the
// current chrome.exe process since it was started. This is a nop on other
// platforms.
void RecordHardFaultHistogram() {
#if BUILDFLAG(IS_WIN)
  DCHECK_EQ(UNDETERMINED_STARTUP_TEMPERATURE, g_startup_temperature);

  const absl::optional<uint32_t> hard_fault_count =
      GetHardFaultCountForCurrentProcess();

  if (hard_fault_count.has_value()) {
    // Hard fault counts are expected to be in the thousands range,
    // corresponding to faulting in ~10s of MBs of code ~10s of KBs at a time.
    // (Observed to vary from 1000 to 10000 on various test machines and
    // platforms.)
    base::UmaHistogramCustomCounts(
        "Startup.BrowserMessageLoopStartHardFaultCount",
        hard_fault_count.value(), 1, 40000, 50);

    // Determine the startup type based on the number of observed hard faults.
    if (hard_fault_count < kWarmStartHardFaultCountThreshold) {
      g_startup_temperature = WARM_STARTUP_TEMPERATURE;
    } else if (hard_fault_count >= kColdStartHardFaultCountThreshold) {
      g_startup_temperature = COLD_STARTUP_TEMPERATURE;
    } else {
      g_startup_temperature = LUKEWARM_STARTUP_TEMPERATURE;
    }
  } else {
    // |g_startup_temperature| remains UNDETERMINED_STARTUP_TEMPERATURE if the
    // number of hard faults could not be determined.
  }

  // Record the startup 'temperature'.
  base::UmaHistogramEnumeration("Startup.Temperature", g_startup_temperature,
                                STARTUP_TEMPERATURE_COUNT);
#endif  // BUILDFLAG(IS_WIN)
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
  absl::optional<base::ScopedBoostPriority> scoped_boost_priority;

// Enabling this logic on OS X causes a significant performance regression.
// TODO(crbug.com/601270): Remove IS_APPLE ifdef once priority changes are
// ignored on Mac main thread.
#if !BUILDFLAG(IS_APPLE)
  static bool statics_initialized = false;
  if (!statics_initialized) {
    statics_initialized = true;
    scoped_boost_priority.emplace(base::ThreadType::kDisplayCritical);
  }
#endif  // !BUILDFLAG(IS_APPLE)

  static const base::Time time_base = base::Time::Now();
  static const base::TimeTicks trace_ticks_base = base::TimeTicks::Now();

  // Then use the TimeDelta common ground between the two units to make the
  // conversion.
  const base::TimeDelta delta_since_base = time_base - time;
  return trace_ticks_base - delta_since_base;
}

void AddStartupEventsForTelemetry() {
  // Record the event only if RecordChromeMainEntryTime() was called, which is
  // not the case for some tests.
  if (g_chrome_main_entry_ticks.is_null())
    return;

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "startup", "Startup.BrowserMainEntryPoint", 0, g_chrome_main_entry_ticks);
}

bool ShouldLogStartupHistogram() {
  return !WasMainWindowStartupInterrupted();
}

#if DCHECK_IS_ON()
base::flat_set<int>& GetSessionLog() {
  static base::NoDestructor<base::flat_set<int>> session_log;
  return *session_log;
}
#endif  // DCHECK_IS_ON()

// DCHECKs that this is the first time |method_id| is passed to this assertion
// in this session (a session is typically process-lifetime but this can be
// reset in tests via ResetSessionForTesting()). Callers should use __LINE__ as
// a unique id in this file.
void AssertFirstCallInSession(int method_id) {
#if DCHECK_IS_ON()
  DCHECK(GetSessionLog().insert(method_id).second);
#endif  // DCHECK_IS_ON()
}

}  // namespace

void ResetSessionForTesting() {
#if DCHECK_IS_ON()
  GetSessionLog().clear();
#endif  // DCHECK_IS_ON()
  // Reset global ticks that will be recorded multiple times when multiple
  // tests run in the same process.
  g_message_loop_start_ticks = base::TimeTicks();
  g_browser_window_display_ticks = base::TimeTicks();
}

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
  RecordStartupProcessCreationTime(StartupTimeToTimeTicks(time));
}

void RecordStartupProcessCreationTime(base::TimeTicks ticks) {
  DCHECK(g_process_creation_ticks.is_null());
  g_process_creation_ticks = ticks;
  DCHECK(!g_process_creation_ticks.is_null());
}

void RecordApplicationStartTime(base::TimeTicks ticks) {
  DCHECK(g_application_start_ticks.is_null());
  g_application_start_ticks = ticks;
  DCHECK(!g_application_start_ticks.is_null());
}

void RecordChromeMainEntryTime(base::TimeTicks ticks) {
  DCHECK(g_chrome_main_entry_ticks.is_null());
  g_chrome_main_entry_ticks = ticks;
  DCHECK(!g_chrome_main_entry_ticks.is_null());
}

void RecordMessageLoopStartTicks(base::TimeTicks ticks) {
  DCHECK(g_message_loop_start_ticks.is_null());
  g_message_loop_start_ticks = ticks;
  DCHECK(!g_message_loop_start_ticks.is_null());
}

void RecordBrowserMainMessageLoopStart(base::TimeTicks ticks,
                                       bool is_first_run) {
  DCHECK(!g_application_start_ticks.is_null());

  RecordMessageLoopStartTicks(ticks);

  // Keep RecordHardFaultHistogram() near the top of this method (as much as
  // possible) as many other histograms depend on it setting
  // |g_startup_temperature|.
  RecordHardFaultHistogram();

  // Record timing of the browser message-loop start time.
  if (is_first_run) {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes100,
        "Startup.BrowserMessageLoopStartTime.FirstRun",
        g_application_start_ticks, ticks);
  } else {
    UmaHistogramWithTraceAndTemperature(&base::UmaHistogramLongTimes100,
                                        "Startup.BrowserMessageLoopStartTime",
                                        g_application_start_ticks, ticks);
  }

  AddStartupEventsForTelemetry();

  // Record values stored prior to startup temperature evaluation.
  if (ShouldLogStartupHistogram() &&
      !g_browser_window_display_ticks.is_null()) {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes, "Startup.BrowserWindowDisplay",
        g_application_start_ticks, g_browser_window_display_ticks);
  }

  // Process creation to application start. See comment above
  // RecordApplicationStart().
  if (!g_process_creation_ticks.is_null()) {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes,
        "Startup.LoadTime.ProcessCreateToApplicationStart",
        g_process_creation_ticks, g_application_start_ticks);

    // Application start to ChromeMain().
    DCHECK(!g_chrome_main_entry_ticks.is_null());
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes,
        "Startup.LoadTime.ApplicationStartToChromeMain",
        g_application_start_ticks, g_chrome_main_entry_ticks);
  }
}

void RecordBrowserMainLoopFirstIdle(base::TimeTicks ticks) {
  DCHECK(!g_application_start_ticks.is_null());
  AssertFirstCallInSession(__LINE__);

  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(&base::UmaHistogramLongTimes100,
                                      "Startup.BrowserMessageLoopFirstIdle",
                                      g_application_start_ticks, ticks);
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

void RecordFirstWebContentsNonEmptyPaint(
    base::TimeTicks now,
    base::TimeTicks render_process_host_init_time) {
  DCHECK(!g_application_start_ticks.is_null());
  AssertFirstCallInSession(__LINE__);

  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(&base::UmaHistogramLongTimes100,
                                      "Startup.FirstWebContents.NonEmptyPaint3",
                                      g_application_start_ticks, now);

  UmaHistogramWithTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.BrowserMessageLoopStart.To.NonEmptyPaint2",
      now - g_message_loop_start_ticks);
}

void RecordFirstWebContentsMainNavigationStart(base::TimeTicks ticks) {
  DCHECK(!g_application_start_ticks.is_null());
  AssertFirstCallInSession(__LINE__);

  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.FirstWebContents.MainNavigationStart", g_application_start_ticks,
      ticks);
}

void RecordFirstWebContentsMainNavigationFinished(base::TimeTicks ticks) {
  DCHECK(!g_application_start_ticks.is_null());
  AssertFirstCallInSession(__LINE__);

  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.FirstWebContents.MainNavigationFinished",
      g_application_start_ticks, ticks);
}

void RecordBrowserWindowFirstPaint(base::TimeTicks ticks) {
  DCHECK(!g_application_start_ticks.is_null());

  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null())
    return;
  is_first_call = false;
  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(&base::UmaHistogramLongTimes100,
                                      "Startup.BrowserWindow.FirstPaint",
                                      g_application_start_ticks, ticks);
}

base::TimeTicks MainEntryPointTicks() {
  return g_chrome_main_entry_ticks;
}

void RecordExternalStartupMetric(const char* histogram_name,
                                 base::TimeTicks completion_ticks,
                                 bool set_non_browser_ui_displayed) {
  DCHECK(!g_application_start_ticks.is_null());

  if (!ShouldLogStartupHistogram())
    return;

  UmaHistogramWithTraceAndTemperature(&base::UmaHistogramMediumTimes,
                                      histogram_name, g_application_start_ticks,
                                      completion_ticks);

  if (set_non_browser_ui_displayed)
    SetNonBrowserUIDisplayed();
}

void RecordFirstRunSentinelCreation(FirstRunSentinelCreationResult result) {
  base::UmaHistogramEnumeration("FirstRun.Sentinel.Created", result);
}

}  // namespace startup_metric_utils
