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
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/openscreen/src/platform/base/error.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace openscreen_platform {

namespace {

// Converts an Open Screen time point to a base::TimeTicks. This is the inverse
// of openscreen::Clock::now() in time.cc, which counts the microseconds since
// the base::TimeTicks origin.
base::TimeTicks ToTimeTicks(openscreen::Clock::time_point time) {
  return base::TimeTicks() +
         base::Microseconds(
             std::chrono::duration_cast<std::chrono::microseconds>(
                 time.time_since_epoch())
                 .count());
}

}  // namespace

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
  TRACE_EVENT_INSTANT(
      kOpenscreenTraceLoggingCategory, perfetto::StaticString(event.name),
      "event", event.ToString(), "duration",
      base::StrCat({base::NumberToString(total_runtime), "µs"}));
}

void EventTraceLoggingPlatform::LogAsyncStart(openscreen::TraceEvent event) {
  TRACE_EVENT_BEGIN(
      kOpenscreenTraceLoggingCategory, perfetto::StaticString(event.name),
      perfetto::Track(event.ids.current), "event", event.ToString());
}

void EventTraceLoggingPlatform::LogAsyncEnd(openscreen::TraceEvent event) {
  TRACE_EVENT_END(kOpenscreenTraceLoggingCategory,
                  perfetto::Track(event.ids.current), "event",
                  event.ToString());
}

void EventTraceLoggingPlatform::LogFlow(openscreen::TraceEvent event,
                                        openscreen::FlowType type) {
  // Open Screen chooses the flow IDs, for example from the RTP timestamp of a
  // frame. They are used as global IDs without change, so that tools can match
  // them with the IDs that Open Screen itself logs. The event is written at the
  // time that Open Screen recorded, which may be earlier than now (for
  // example, the capture time of a frame).
  TRACE_EVENT_INSTANT(
      kOpenscreenTraceLoggingCategory, perfetto::StaticString(event.name),
      ToTimeTicks(event.start_time),
      [&event, type](perfetto::EventContext ctx) {
        for (const uint64_t flow_id : event.flow_ids) {
          if (type == openscreen::FlowType::kFlowEnd) {
            perfetto::TerminatingFlow::Global(flow_id)(ctx);
          } else {
            perfetto::Flow::Global(flow_id)(ctx);
          }
          ctx.AddDebugAnnotation("flow_id", flow_id);
        }
        // Open Screen only passes string literals as argument names, so they
        // can be interned like static strings.
        for (const auto& [name, value] : event.arguments) {
          ctx.AddDebugAnnotation(name, value);
        }
        if (event.result != openscreen::Error::Code::kNone) {
          ctx.AddDebugAnnotation("result", openscreen::ToString(event.result));
        }
      });
}

}  // namespace openscreen_platform
