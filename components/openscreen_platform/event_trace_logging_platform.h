// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_

#include "third_party/openscreen/src/platform/api/trace_logging_platform.h"

#include "base/containers/flat_map.h"

namespace openscreen_platform {

constexpr char kOpenscreenTraceLoggingCategory[] = "openscreen";

// Implementation of a trace logging platform for Open Screen that is backed
// by Chrome's TRACE_EVENT macros. Note that the openscreen::Clock::time_point
// values are ignored in favor of using Chrome's internal time points. This
// helps ensure that values are in line with other Chrome trace logging
// categories.
class EventTraceLoggingPlatform : public openscreen::TraceLoggingPlatform {
 public:
  EventTraceLoggingPlatform();
  EventTraceLoggingPlatform(const EventTraceLoggingPlatform&) = delete;
  EventTraceLoggingPlatform(EventTraceLoggingPlatform&&) = delete;
  EventTraceLoggingPlatform& operator=(const EventTraceLoggingPlatform&) =
      delete;
  EventTraceLoggingPlatform& operator=(EventTraceLoggingPlatform&&) = delete;
  ~EventTraceLoggingPlatform() override;

  // Ensures that a trace logging platform exists for the current process.
  static void EnsureInstance();

  // TraceLoggingPlatform implementation.
  bool IsTraceLoggingEnabled(openscreen::TraceCategory category) override;
  void LogTrace(openscreen::TraceEvent event,
                openscreen::Clock::time_point end_time) override;
  void LogAsyncStart(openscreen::TraceEvent event) override;
  void LogAsyncEnd(openscreen::TraceEvent event) override;
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_EVENT_TRACE_LOGGING_PLATFORM_H_