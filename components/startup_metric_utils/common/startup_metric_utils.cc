// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/common/startup_metric_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/trace_event/trace_event.h"

namespace startup_metric_utils {

CommonStartupMetricRecorder& GetCommon() {
  // If this ceases to be true, Get{Common,Browser} need to be changed to use
  // base::NoDestructor.
  static_assert(
      std::is_trivially_destructible<CommonStartupMetricRecorder>::value,
      "Startup metric recorder classes must be trivially destructible.");

  static CommonStartupMetricRecorder instance;
  return instance;
}

void CommonStartupMetricRecorder::RecordStartupProcessCreationTime(
    base::Time time) {
  // In certain cases, getting the process creation can fail, and no metrics
  // should be emitted in that case.
  bool time_is_valid = time != base::Time();
  if (time_is_valid) {
    RecordStartupProcessCreationTime(StartupTimeToTimeTicks(time));
  }
  base::UmaHistogramBoolean("Startup.LoadTime.RecordedProcessCreation",
                            time_is_valid);
}

void CommonStartupMetricRecorder::RecordStartupProcessCreationTime(
    base::TimeTicks ticks) {
  DCHECK(process_creation_ticks_.is_null());
  process_creation_ticks_ = ticks;
  DCHECK(!process_creation_ticks_.is_null());
}

void CommonStartupMetricRecorder::RecordApplicationStartTime(
    base::TimeTicks ticks) {
  DCHECK(application_start_ticks_.is_null());
  application_start_ticks_ = ticks;
  DCHECK(!application_start_ticks_.is_null());
}

void CommonStartupMetricRecorder::RecordChromeMainEntryTime(
    base::TimeTicks ticks) {
  DCHECK(chrome_main_entry_ticks_.is_null());
  chrome_main_entry_ticks_ = ticks;
  DCHECK(!chrome_main_entry_ticks_.is_null());
}

void CommonStartupMetricRecorder::RecordPreReadTime(base::TimeTicks start_ticks,
                                                    base::TimeTicks end_ticks) {
  preread_begin_ticks_ = start_ticks;
  preread_end_ticks_ = end_ticks;
  // These aren't necessarily non-null after setting, because if the process in
  // question did not PreRead (if `--no-pre-read-main-dll` was passed, for
  // example), they will be null.
}

void CommonStartupMetricRecorder::ResetSessionForTesting() {
#if DCHECK_IS_ON()
  GetSessionLog().clear();
#endif  // DCHECK_IS_ON()
}

base::TimeTicks CommonStartupMetricRecorder::MainEntryPointTicks() const {
  return chrome_main_entry_ticks_;
}

base::TimeTicks CommonStartupMetricRecorder::StartupTimeToTimeTicks(
    base::Time time) {
  // First get a base which represents the same point in time in both units.
  // Bump the priority of this thread while doing this as the wall clock time it
  // takes to resolve these two calls affects the precision of this method and
  // bumping the priority reduces the likelihood of a context switch interfering
  // with this computation.
  std::optional<base::ScopedBoostPriority> scoped_boost_priority;

  // Enabling this logic on OS X causes a significant performance regression.
  // TODO(crbug.com/40464036): Remove IS_APPLE ifdef once utility processes
  // set their desired main thread priority.
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

void CommonStartupMetricRecorder::AddStartupEventsForTelemetry() {
  // Record the event only if RecordChromeMainEntryTime() was called, which is
  // not the case for some tests.
  if (chrome_main_entry_ticks_.is_null()) {
    return;
  }

  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "startup", "Startup.BrowserMainEntryPoint", 0, chrome_main_entry_ticks_);
}

#if DCHECK_IS_ON()
base::flat_set<const void*>& CommonStartupMetricRecorder::GetSessionLog() {
  static base::NoDestructor<base::flat_set<const void*>> session_log;
  return *session_log;
}
#endif  // DCHECK_IS_ON()

// DCHECKs that this is the first time |method_id| is passed to this assertion
// in this session (a session is typically process-lifetime but this can be
// reset in tests via ResetSessionForTesting()). Callers should use FROM_HERE as
// a unique id.

void CommonStartupMetricRecorder::AssertFirstCallInSession(
    base::Location from_here) {
#if DCHECK_IS_ON()
  DCHECK(GetSessionLog().insert(from_here.program_counter()).second);
#endif  // DCHECK_IS_ON()
}

void CommonStartupMetricRecorder::EmitHistogramWithTraceEvent(
    HistogramTimeFunction* histogram_function,
    const char* name,
    base::TimeTicks begin_ticks,
    base::TimeTicks end_ticks) {
  (*histogram_function)(name, end_ticks - begin_ticks);
  EmitTraceEvent(name, begin_ticks, end_ticks);
}

void CommonStartupMetricRecorder::EmitTraceEvent(const char* name,
                                                 base::TimeTicks begin_ticks,
                                                 base::TimeTicks end_ticks) {
  // TODO(325307453): Emit the trace events asynchronously onto a
  // perfetto::Track. Ordering is a problem if doing it simplistically as
  // the async events are emitted out-of-order with identical start times
  // and thus Perfetto is unable to sort them in a reasonable parent-child
  // relationship.
  TRACE_EVENT_BEGIN("startup", perfetto::StaticString(name),
                    perfetto::Track(reinterpret_cast<uintptr_t>(name)),
                    begin_ticks);
  TRACE_EVENT_END("startup", perfetto::Track(reinterpret_cast<uintptr_t>(name)),
                  end_ticks);
}

void CommonStartupMetricRecorder::EmitInstantEvent(const char* name) {
  TRACE_EVENT_INSTANT("startup", perfetto::StaticString(name),
                      perfetto::Track(reinterpret_cast<uintptr_t>(name)));
}

}  // namespace startup_metric_utils
