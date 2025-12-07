// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
#define CHROME_COMMON_PROFILER_PROCESS_TYPE_H_

#include "components/sampling_profiler/process_type.h"

namespace base {
class CommandLine;
}

sampling_profiler::ProfilerProcessType GetProfilerProcessType(
    const base::CommandLine& command_line);

#endif  // CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
