// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CHILD_LOG_PROCESS_H_
#define CHROMECAST_CAST_CORE_CHILD_LOG_PROCESS_H_

#include <string>

namespace chromecast {

// Forks and runs a separate process specified by `log_process_path`.
//
// Pipes the parent's stderr to the child's stdin.
//
// Arguments are provided by `log_process_args`. If `log_process_args` starts
// and ends with double quotes, it will be stripped. arguments are tokenized by
// spaces.
void ForkAndRunLogProcess(std::string log_process_path,
                          std::string log_process_args);

void ForkAndRunLogProcessIfSpecified(int argc, const char* const* argv);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_CHILD_LOG_PROCESS_H_
