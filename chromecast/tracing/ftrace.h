// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_TRACING_FTRACE_H_
#define CHROMECAST_TRACING_FTRACE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/files/scoped_file.h"

namespace chromecast {
namespace tracing {

// Returns true if |category| is valid for system tracing.
bool IsValidCategory(std::string_view category);

// Starts ftrace for the specified categories.
//
// This must be paired with StopFtrace() or the kernel will continue tracing
// indefinitely. Returns false if an error occurs writing to tracefs - this
// usually indicates a configuration issue (e.g. tracefs not mounted).
bool StartFtrace(const std::vector<std::string>& categories);

// Writes time synchronization marker.
//
// This is used by trace-viewer to align ftrace clock with userspace
// tracing. Since CLOCK_MONOTONIC is used in both cases, this merely
// writes a zero offset. Call it at the end of tracing right before
// StopFtrace(). Returns false if an error occurs writing to tracefs.
bool WriteFtraceTimeSyncMarker();

// Stops ftrace.
//
// This is safe to call even if tracing isn't started. Returns false if an error
// occurs writing to tracefs.
bool StopFtrace();

// Opens ftrace buffer for reading.
base::ScopedFD GetFtraceData();

// Clears ftrace buffer. Returns false if an error occurs writing to tracefs.
bool ClearFtrace();

}  // namespace tracing
}  // namespace chromecast

#endif  // CHROMECAST_TRACING_FTRACE_H_
