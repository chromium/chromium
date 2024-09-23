// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_

#include "components/tracing/tracing_export.h"

namespace switches {

TRACING_EXPORT extern const char kEnableBackgroundTracing[];
TRACING_EXPORT extern const char kEnableLegacyBackgroundTracing[];
TRACING_EXPORT extern const char kTraceConfigFile[];
TRACING_EXPORT extern const char kTraceStartup[];
TRACING_EXPORT extern const char kTraceConfigHandle[];
TRACING_EXPORT extern const char kEnableTracing[];
TRACING_EXPORT extern const char kTraceStartupDuration[];
TRACING_EXPORT extern const char kTraceStartupFile[];
TRACING_EXPORT extern const char kEnableTracingOutput[];
TRACING_EXPORT extern const char kTraceStartupRecordMode[];
TRACING_EXPORT extern const char kTraceStartupFormat[];
TRACING_EXPORT extern const char kEnableTracingFormat[];
TRACING_EXPORT extern const char kTraceStartupOwner[];
TRACING_EXPORT extern const char kPerfettoDisableInterning[];
TRACING_EXPORT extern const char kTraceToConsole[];
TRACING_EXPORT extern const char kBackgroundTracingOutputPath[];
TRACING_EXPORT extern const char kTraceSmbSize[];
TRACING_EXPORT extern const char kDefaultTraceBufferSizeLimitInKb[];

}  // namespace switches

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_
