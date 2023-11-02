// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
#define CHROME_COMMON_PROFILER_PROCESS_TYPE_H_

#include "components/metrics/call_stack_profile_params.h"

namespace base {
class CommandLine;
}

metrics::CallStackProfileParams::Process GetProfileParamsProcess(
    const base::CommandLine& command_line);

#endif  // CHROME_COMMON_PROFILER_PROCESS_TYPE_H_
