// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
#define CHROME_COMMON_PROFILER_PROCESS_TYPE_H_

#include "base/profiler/process_type.h"

namespace base {
class CommandLine;
}

// TODO(crbug.com/354124876): Move this function to base/profiler/process_type.h
// once the factory is implemented to handle //chrome dependencies.
base::ProfilerProcessType GetProfilerProcessType(
    const base::CommandLine& command_line);

#endif  // CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
