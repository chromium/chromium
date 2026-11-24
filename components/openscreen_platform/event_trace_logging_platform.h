// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_

#include "base/containers/flat_map.h"
#include "base/trace_event/trace_session_observer.h"
#include "third_party/openscreen/src/platform/api/trace_logging_platform.h"

namespace openscreen_platform {

constexpr char kOpenscreenTraceLoggingCategory[] = "openscreen";

// Implementation of a trace logging platform for Open Screen that is backed
// by Chrome's TRACE_EVENT macros. Note that the openscreen::Clock::time_point
// values are ignored in favor of using Chrome's internal time points. This
// helps ensure that values are in line with other Chrome trace logging
// categories. Flow events are the exception: they use the time that Open
// Screen recorded, which may be earlier than now. Chrome's openscreen::Clock
// is based on base::TimeTicks, so these times line up too.
//
// Open Screen sees this platform only while a trace session records the
// "openscreen" category. At all other times Open Screen has no tracing
// destination, so its trace macros skip all work.
class EventTraceLoggingPlatform
    : public openscreen::TraceLoggingPlatform,
      public base::trace_event::TraceSessionObserver {
 public:
  EventTraceLoggingPlatform();
  EventTraceLoggingPlatform(const EventTraceLoggingPlatform&) = delete;
  EventTraceLoggingPlatform(EventTraceLoggingPlatform&&) = delete;
  EventTraceLoggingPlatform& operator=(const EventTraceLoggingPlatform&) =
      delete;
  EventTraceLoggingPlatform& operator=(EventTraceLoggingPlatform&&) = delete;
  ~EventTraceLoggingPlatform() override;

  // Returns the trace logging platform for the current process, creating it on
  // first use. Chrome code that uses Open Screen calls this to set up tracing.
  // Open Screen itself never calls it: the platform registers itself with
  // Open Screen through openscreen::StartTracing() while a trace session
  // records the category. Open Screen supports only one platform at a time,
  // so code should use this instead of creating another one.
  static EventTraceLoggingPlatform& GetInstance();

  // TraceLoggingPlatform implementation.
  bool IsTraceLoggingEnabled(openscreen::TraceCategory category) override;
  void LogTrace(openscreen::TraceEvent event,
                openscreen::Clock::time_point end_time) override;
  void LogAsyncStart(openscreen::TraceEvent event) override;
  void LogAsyncEnd(openscreen::TraceEvent event) override;
  void LogFlow(openscreen::TraceEvent event,
               openscreen::FlowType type) override;

  // base::trace_event::TraceSessionObserver implementation.
  void OnStart(const perfetto::DataSourceBase::StartArgs& args) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs& args) override;

 private:
  // Registers this object with Open Screen if a trace session is recording the
  // Open Screen category.
  void MaybeStartTracing();
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_