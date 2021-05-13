// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TRACING_H_
#define CHROME_TEST_BASE_TRACING_H_

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"

namespace base {
namespace trace_event {
class TraceConfig;
}
}  // namespace base

namespace tracing {

// Called from UI thread.
// Begin tracing specified category_patterns on the browser.
// |category_patterns| is a comma-delimited list of category wildcards.
// A category pattern can have an optional '-' prefix to make  categories with
// matching categorys excluded. Either all category_patterns must be included
// or all must be excluded.
//
// Example: BeginTracing("test_MyTest*");
// Example: BeginTracing("test_MyTest*,test_OtherStuff");
// Example: BeginTracing("-excluded_category1,-excluded_category2");
//
// See base/trace_event/trace_event.h for documentation of included and excluded
// category_patterns.
bool BeginTracing(const std::string& category_patterns) WARN_UNUSED_RESULT;

// Called from UI thread.
// Begin tracing specified category_patterns on the browser.
// |trace_config| specifies the configuration for tracing. This includes the
// list of categories enabled, tracing modes and memory dumps configuration.
// Once all child processes have acked to the StartTracing request,
// |start_tracing_done_callback| will be called back.
//
// See base/trace_event/trace_config.h for documentation of configurations.
bool BeginTracingWithTraceConfig(
    const base::trace_event::TraceConfig& trace_config) WARN_UNUSED_RESULT;

using StartTracingDoneCallback = base::OnceClosure;
bool BeginTracingWithTraceConfig(
    const base::trace_event::TraceConfig& trace_config,
    StartTracingDoneCallback start_tracing_done_callback) WARN_UNUSED_RESULT;

// Called from UI thread.
// End trace and collect the trace output as a json string.
bool EndTracing(std::string* json_trace_output) WARN_UNUSED_RESULT;

}  // namespace tracing

#endif  // CHROME_TEST_BASE_TRACING_H_
