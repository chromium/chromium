// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <type_traits>
#include <vector>

#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/trace_event/trace_event.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include <winternl.h>
#include "base/win/win_util.h"

namespace {

// The signature of the NtQuerySystemInformation function.
typedef NTSTATUS(WINAPI* NtQuerySystemInformationPtr)(SYSTEM_INFORMATION_CLASS,
                                                      PVOID,
                                                      ULONG,
                                                      PULONG);

// These values are taken from the
// Startup.BrowserMessageLoopStartHardFaultCount histogram. The latest
// revision landed on <5 and >3500 for a good split of warm/cold. In between
// being considered "lukewarm". Full analysis @
// https://docs.google.com/document/d/1haXFN1cQ6XE-NfhKgww-rOP-Wi-gK6AczP3gT4M5_kI
// These values should be reconsidered if either .WarmStartup or .ColdStartup
// distributions of a suffixed histogram becomes unexplainably bimodal.
//
// Maximum number of hard faults tolerated for a startup to be classified as a
// warm start.
constexpr uint32_t kWarmStartHardFaultCountThreshold = 5;

// Minimum number of hard faults (of 4KB pages) expected for a startup to be
// classified as a cold start. The right value for this seems to be between
// 10% and 15% of chrome.dll's size (from anecdata of the two times we did
// this analysis... it was 1200 in M47 back when chrome.dll was 35MB (32-bit
// and split from chrome_child.dll) and was made 3500 in M81 when chrome.dll
// was 126MB).
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
  // This is labeled a handle so that it expands to the correct size for
  // 32-bit and 64-bit operating systems. However, under the hood it's a
  // 32-bit DWORD containing the process ID.
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

}  // namespace
#endif

namespace {
const char kProcessType[] = "type";

// An enumeration of startup temperatures. This must be kept in sync with
// the UMA StartupType enumeration defined in histograms.xml.
enum StartupTemperature {
  // The startup was a cold start: nearly all of the binaries and resources
  // were
  // brought into memory using hard faults.
  COLD_STARTUP_TEMPERATURE = 0,
  // The startup was a warm start: the binaries and resources were mostly
  // already resident in memory and effectively no hard faults were
  // observed.
  WARM_STARTUP_TEMPERATURE = 1,
  // The startup type couldn't quite be classified as warm or cold, but
  // rather
  // was somewhere in between.
  LUKEWARM_STARTUP_TEMPERATURE = 2,
  // Startup temperature wasn't yet determined, or could not be determined.
  UNDETERMINED_STARTUP_TEMPERATURE = 3,
  // This must be after all meaningful values. All new values should be
  // added
  // above this one.
  STARTUP_TEMPERATURE_COUNT,
};

StartupTemperature g_startup_temperature = UNDETERMINED_STARTUP_TEMPERATURE;

// Helper function for splitting out an UMA histogram based on startup
// temperature. |histogram_function| is the histogram type, and corresponds to
// an UMA function like base::UmaHistogramLongTimes. It must itself be a
// function that only takes two parameters.
// |basename| is the basename of the histogram. A histogram of this name will
// always be recorded to. If the startup temperature is known then a value
// will also be recorded to the histogram with name |basename| and suffix
// ".ColdStart", ".WarmStart" as appropriate.
// |value_expr| is an expression evaluating to the value to be recorded. This
// will be evaluated exactly once and cached, so side effects are not an
// issue. A metric logged using this function must have an affected-histogram
// entry in the definition of the StartupTemperature suffix in histograms.xml.
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

}  // namespace

namespace startup_metric_utils {

BrowserStartupMetricRecorder& GetBrowser() {
  // If this ceases to be true, Get{Common,Browser} need to be changed to use
  // base::NoDestructor.
  static_assert(
      std::is_trivially_destructible<BrowserStartupMetricRecorder>::value,
      "Startup metric recorder classes must be trivially destructible.");

  // This guard prevents non-browser processes from reporting browser process
  // metrics.
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(kProcessType));
  static BrowserStartupMetricRecorder instance;
  return instance;
}

#if BUILDFLAG(IS_WIN)
// Returns the hard fault count of the current process, or nullopt if it can't
// be determined.
absl::optional<uint32_t>
BrowserStartupMetricRecorder::GetHardFaultCountForCurrentProcess() {
  // Get the function pointer.
  static const NtQuerySystemInformationPtr query_sys_info =
      reinterpret_cast<NtQuerySystemInformationPtr>(::GetProcAddress(
          GetModuleHandle(L"ntdll.dll"), "NtQuerySystemInformation"));
  if (query_sys_info == nullptr) {
    return absl::nullopt;
  }

  // The output of this system call depends on the number of threads and
  // processes on the entire system, and this can change between calls. Retry
  // a small handful of times growing the buffer along the way.
  // NOTE: The actual required size depends entirely on the number of
  // processes
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
      // Abort if a large size is required for the buffer. It is undesirable
      // to fill a large buffer just to record histograms.
      constexpr ULONG kMaxLength = 512 * 1024;
      if (return_length >= kMaxLength) {
        return absl::nullopt;
      }

      // Resize the buffer and retry, if the buffer hasn't already been
      // resized too many times.
      if (num_buffer_resize < kMaxNumBufferResize) {
        ++num_buffer_resize;
        buffer.resize(return_length);
        continue;
      }
    }

    // Abort if NtQuerySystemInformation failed for another reason than
    // insufficient buffer length, or if the buffer was resized too many
    // times.
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
    if (base::win::HandleToUint32(proc_info->UniqueProcessId) == proc_id) {
      return proc_info->HardFaultCount;
    }
    // The list ends when NextEntryOffset is zero. This also prevents busy
    // looping if the data is in fact invalid.
    if (proc_info->NextEntryOffset <= 0) {
      return absl::nullopt;
    }
    index += proc_info->NextEntryOffset;
  }

  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_WIN)

void BrowserStartupMetricRecorder::ResetSessionForTesting() {
  GetCommon().ResetSessionForTesting();
  // Reset global ticks that will be recorded multiple times when multiple
  // tests run in the same process.
  main_window_startup_interrupted_ = false;
  message_loop_start_ticks_ = base::TimeTicks();
  browser_window_display_ticks_ = base::TimeTicks();
  browser_window_first_paint_ticks_ = base::TimeTicks();
  is_privacy_sandbox_attestations_histogram_recorded_ = false;
}

bool BrowserStartupMetricRecorder::WasMainWindowStartupInterrupted() const {
  return main_window_startup_interrupted_;
}

void BrowserStartupMetricRecorder::SetNonBrowserUIDisplayed() {
  main_window_startup_interrupted_ = true;
}

void BrowserStartupMetricRecorder::SetBackgroundModeEnabled() {
  main_window_startup_interrupted_ = true;
}

void BrowserStartupMetricRecorder::RecordMessageLoopStartTicks(
    base::TimeTicks ticks) {
  DCHECK(message_loop_start_ticks_.is_null());
  message_loop_start_ticks_ = ticks;
  DCHECK(!message_loop_start_ticks_.is_null());
}

void BrowserStartupMetricRecorder::RecordBrowserMainMessageLoopStart(
    base::TimeTicks ticks,
    bool is_first_run) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());

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
        GetCommon().application_start_ticks_, ticks);
  } else {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes100, "Startup.BrowserMessageLoopStartTime",
        GetCommon().application_start_ticks_, ticks);
  }

  GetCommon().AddStartupEventsForTelemetry();

  // Record values stored prior to startup temperature evaluation.
  if (ShouldLogStartupHistogram() && !browser_window_display_ticks_.is_null()) {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes, "Startup.BrowserWindowDisplay",
        GetCommon().application_start_ticks_, browser_window_display_ticks_);
  }

  // Process creation to application start. See comment above
  // RecordApplicationStart().
  if (!GetCommon().process_creation_ticks_.is_null()) {
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes,
        "Startup.LoadTime.ProcessCreateToApplicationStart",
        GetCommon().process_creation_ticks_,
        GetCommon().application_start_ticks_);

    // Application start to ChromeMain().
    DCHECK(!GetCommon().chrome_main_entry_ticks_.is_null());
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes,
        "Startup.LoadTime.ApplicationStartToChromeMain",
        GetCommon().application_start_ticks_,
        GetCommon().chrome_main_entry_ticks_);
  }
}

void BrowserStartupMetricRecorder::RecordBrowserMainLoopFirstIdle(
    base::TimeTicks ticks) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());
  GetCommon().AssertFirstCallInSession(FROM_HERE);

  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100, "Startup.BrowserMessageLoopFirstIdle",
      GetCommon().application_start_ticks_, ticks);
}

void BrowserStartupMetricRecorder::RecordBrowserWindowDisplay(
    base::TimeTicks ticks) {
  DCHECK(!ticks.is_null());

  if (!browser_window_display_ticks_.is_null()) {
    return;
  }

  // The value will be recorded in appropriate histograms after the startup
  // temperature is evaluated.
  //
  // Note: In some cases (e.g. launching with --silent-launch), the first
  // browser window is displayed after the startup temperature is evaluated. In
  // these cases, the value will not be recorded, which is the desired behavior
  // for a non-conventional launch.
  browser_window_display_ticks_ = ticks;
}

void BrowserStartupMetricRecorder::RecordBrowserWindowFirstPaintTicks(
    base::TimeTicks ticks) {
  DCHECK(!ticks.is_null());

  if (!browser_window_first_paint_ticks_.is_null()) {
    return;
  }

  browser_window_first_paint_ticks_ = ticks;
}

void BrowserStartupMetricRecorder::RecordFirstWebContentsNonEmptyPaint(
    base::TimeTicks now,
    base::TimeTicks render_process_host_init_time) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());
  GetCommon().AssertFirstCallInSession(FROM_HERE);

  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(&base::UmaHistogramLongTimes100,
                                      "Startup.FirstWebContents.NonEmptyPaint3",
                                      GetCommon().application_start_ticks_,
                                      now);

  UmaHistogramWithTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.BrowserMessageLoopStart.To.NonEmptyPaint2",
      now - message_loop_start_ticks_);
}

void BrowserStartupMetricRecorder::RecordFirstWebContentsMainNavigationStart(
    base::TimeTicks ticks) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());
  GetCommon().AssertFirstCallInSession(FROM_HERE);

  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.FirstWebContents.MainNavigationStart",
      GetCommon().application_start_ticks_, ticks);
}

void BrowserStartupMetricRecorder::RecordFirstWebContentsMainNavigationFinished(
    base::TimeTicks ticks) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());
  GetCommon().AssertFirstCallInSession(FROM_HERE);

  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100,
      "Startup.FirstWebContents.MainNavigationFinished",
      GetCommon().application_start_ticks_, ticks);
}

void BrowserStartupMetricRecorder::RecordBrowserWindowFirstPaint(
    base::TimeTicks ticks) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());

  static bool is_first_call = true;
  if (!is_first_call || ticks.is_null()) {
    return;
  }
  is_first_call = false;
  RecordBrowserWindowFirstPaintTicks(ticks);
  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramLongTimes100, "Startup.BrowserWindow.FirstPaint",
      GetCommon().application_start_ticks_, ticks);
}

void BrowserStartupMetricRecorder::RecordFirstRunSentinelCreation(
    FirstRunSentinelCreationResult result) {
  base::UmaHistogramEnumeration("FirstRun.Sentinel.Created", result);
}

void BrowserStartupMetricRecorder::RecordHardFaultHistogram() {
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
    // |g_startup_temperature| remains
    // UNDETERMINED_STARTUP_TEMPERATURE if the number of hard faults could not
    // be determined.
  }

  // Record the startup 'temperature'.
  base::UmaHistogramEnumeration("Startup.Temperature", g_startup_temperature,
                                STARTUP_TEMPERATURE_COUNT);
#endif  // BUILDFLAG(IS_WIN)
}

bool BrowserStartupMetricRecorder::ShouldLogStartupHistogram() const {
  return !WasMainWindowStartupInterrupted();
}

void BrowserStartupMetricRecorder::RecordExternalStartupMetric(
    const char* histogram_name,
    base::TimeTicks completion_ticks,
    bool set_non_browser_ui_displayed) {
  DCHECK(!GetCommon().application_start_ticks_.is_null());

  if (!ShouldLogStartupHistogram()) {
    return;
  }

  UmaHistogramWithTraceAndTemperature(
      &base::UmaHistogramMediumTimes, histogram_name,
      GetCommon().application_start_ticks_, completion_ticks);

  if (set_non_browser_ui_displayed) {
    SetNonBrowserUIDisplayed();
  }
}

// There are two possible callers of `ComponentReady()`:
// a) Component registration, when there is existing component file on disk.
// b) Component installation, when the component is downloaded.
//
// There are several factors that affect the timing of `ComponentReady()`:
// 1. Feature `kPrivacySandboxAttestationsHigherComponentRegistrationPriority`
// controls whether a higher task priority is used for component registration.
// - When feature on, registration takes place almost immediately after opening
// the browser.
// - When feature off, registration takes place in a few seconds after opening
// the browser.
// 2. Non-browser UI during startup, for example, profile picker.
// - When the above feature is off, and the user stays at the profile picker
// indefinitely. The registration takes place in around 4 minutes after opening
// the browser.
//
// The purpose of this metric is to understand the time gap between the time
// users are able to navigate and the time the Privacy Sandbox attestations map
// is ready. If navigation to sites that use Privacy Sandbox APIs takes place
// during this gap, the API calls may be rejected because the attestations map
// has not been ready yet.
//
// To reduce the noise introduced by non-browser UI, we measure from the first
// browser window paint if it has been recorded. If it is not recorded, the
// measurement is taken from application start.
void BrowserStartupMetricRecorder::RecordPrivacySandboxAttestationsFirstReady(
    base::TimeTicks ticks) {
  DCHECK(!ticks.is_null());

  // This metric should be recorded at most once for each Chrome session.
  if (is_privacy_sandbox_attestations_histogram_recorded_) {
    return;
  }

  // The first browser window paint has been recorded.
  if (!browser_window_first_paint_ticks_.is_null()) {
    is_privacy_sandbox_attestations_histogram_recorded_ = true;
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes100,
        privacy_sandbox::kComponentReadyFromBrowserWindowFirstPaintUMA,
        browser_window_first_paint_ticks_, ticks);
    return;
  }

  // Otherwise, this implies the component is installed before first browser
  // window paint.
  is_privacy_sandbox_attestations_histogram_recorded_ = true;
  if (WasMainWindowStartupInterrupted()) {
    // The durations should be a few minutes.
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes100,
        privacy_sandbox::kComponentReadyFromApplicationStartWithInterruptionUMA,
        GetCommon().application_start_ticks_, ticks);
  } else {
    // The durations should be a few milliseconds.
    UmaHistogramWithTraceAndTemperature(
        &base::UmaHistogramLongTimes100,
        privacy_sandbox::kComponentReadyFromApplicationStartUMA,
        GetCommon().application_start_ticks_, ticks);
  }
}

}  // namespace startup_metric_utils
