// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/event_trace_logging_platform.h"

#include <chrono>
#include <limits>
#include <sstream>

#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace openscreen_platform {

// static
void EventTraceLoggingPlatform::EnsureInstance() {
  static base::NoDestructor<EventTraceLoggingPlatform> g_platform;
}

bool EventTraceLoggingPlatform::IsTraceLoggingEnabled(
    openscreen::TraceCategory category) {
  // Chrome controls what categories are enabled by use of the subsystem field
  // in the trace event macro.
  return true;
}

EventTraceLoggingPlatform::EventTraceLoggingPlatform() {
  openscreen::StartTracing(this);
}

EventTraceLoggingPlatform::~EventTraceLoggingPlatform() {
  openscreen::StopTracing();
}

void EventTraceLoggingPlatform::LogTrace(
    openscreen::TraceEvent event,
    openscreen::Clock::time_point end_time) {
  const size_t total_runtime =
      std::chrono::microseconds(end_time - event.start_time).count();

  // We only get two fields in Chrome's trace logging system, so we elect to
  // track the duration for profiling reasons as well as the error code, and
  // ignore the trace ID hierarchy (which is difficult to parse as a user
  // anyway).
  TRACE_EVENT_INSTANT2(
      kOpenscreenTraceLoggingCategory, event.name, TRACE_EVENT_SCOPE_THREAD,
      "event", event.ToString(), "duration",
      base::StrCat({base::NumberToString(total_runtime), "µs"}));
}

void EventTraceLoggingPlatform::LogAsyncStart(openscreen::TraceEvent event) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kOpenscreenTraceLoggingCategory, event.name,
                                    TRACE_ID_LOCAL(event.ids.current), "event",
                                    event.ToString());
}

void EventTraceLoggingPlatform::LogAsyncEnd(openscreen::TraceEvent event) {
  TRACE_EVENT_NESTABLE_ASYNC_END1(kOpenscreenTraceLoggingCategory, event.name,
                                  TRACE_ID_LOCAL(event.ids.current), "event",
                                  event.ToString());
}

}  // namespace openscreen_platform