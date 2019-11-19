// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_

#include "components/tracing/tracing_export.h"

namespace switches {

TRACING_EXPORT extern const char kEnableBackgroundTracing[];
TRACING_EXPORT extern const char kTraceConfigFile[];
TRACING_EXPORT extern const char kTraceStartup[];
TRACING_EXPORT extern const char kTraceStartupDuration[];
TRACING_EXPORT extern const char kTraceStartupFile[];
TRACING_EXPORT extern const char kTraceStartupRecordMode[];
TRACING_EXPORT extern const char kTraceStartupOwner[];
TRACING_EXPORT extern const char kTraceStartupEnablePrivacyFiltering[];
TRACING_EXPORT extern const char kPerfettoDisableInterning[];
TRACING_EXPORT extern const char kPerfettoOutputFile[];
TRACING_EXPORT extern const char kTraceToConsole[];
TRACING_EXPORT extern const char kTraceUploadURL[];

}  // namespace switches

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SWITCHES_H_
