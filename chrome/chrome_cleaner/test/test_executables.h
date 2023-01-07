// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_EXECUTABLES_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_EXECUTABLES_H_

#include "base/command_line.h"
#include "base/process/process.h"

namespace chrome_cleaner {

// The name of the service executable used for tests.
extern const wchar_t kTestServiceExecutableName[];

// The name of the executable used for tests.
extern const wchar_t kTestProcessExecutableName[];

// Creates a process that will run for a minute, which is long enough to be
// killed by a reasonably fast unit or integration test.
// Populates |command_line| with the used command line if it is not nullptr.
base::Process LongRunningProcess(base::CommandLine* command_line);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_EXECUTABLES_H_
