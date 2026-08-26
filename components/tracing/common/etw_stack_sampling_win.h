// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ETW_STACK_SAMPLING_WIN_H_
#define COMPONENTS_TRACING_COMMON_ETW_STACK_SAMPLING_WIN_H_

#include "build/build_config.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

static_assert(BUILDFLAG(IS_WIN));

namespace tracing {

// Adds details needed for symbolization of Event Tracing for Windows (ETW) call
// stacks (specifically, paths and debug IDs for images of interest) to
// `config`.
//
// Does nothing if:
// * the current process is not the browser
// * `config` doesn't enable ETW stack sampling
// * `config` already contains stack-sampling debug IDs
TRACING_EXPORT void AddEtwStackSamplingDebugIds(
    perfetto::DataSourceConfig* config);

// Calls `AddEtwStackSamplingDebugIds()` above for `perfetto_config`'s
// ETW-data-source config.
TRACING_EXPORT void AddEtwStackSamplingDebugIds(
    perfetto::TraceConfig& perfetto_config);

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ETW_STACK_SAMPLING_WIN_H_
