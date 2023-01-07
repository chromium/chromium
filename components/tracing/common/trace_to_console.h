// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACE_TO_CONSOLE_H_
#define COMPONENTS_TRACING_COMMON_TRACE_TO_CONSOLE_H_

#include "base/trace_event/trace_config.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

base::trace_event::TraceConfig TRACING_EXPORT GetConfigForTraceToConsole();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_TRACE_TO_CONSOLE_H_
