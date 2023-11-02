// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_
#define CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_

namespace base {
class CommandLine;
}

enum class RunType {
  // Indicates cleanup is happening before sharded tests are run.
  BEFORE_SHARD,

  // Indicates cleanup is happening after a test subprocess has timed out.
  AFTER_TEST_TIMEOUT,
};

// Logs if there are any always on top windows, and if one is a system dialog
// closes it. |child_command_line|, if non-null, is the command line of the
// test subprocess that timed out. Additionally, if |run_type| is
// AFTER_TEST_TIMEOUT and an output directory is specified via
// --snapshot-output-dir=PATH, a snapshot of the screen is saved for analysis.
void KillAlwaysOnTopWindows(
    RunType run_type,
    const base::CommandLine* child_command_line = nullptr);

#endif  // CHROME_TEST_BASE_ALWAYS_ON_TOP_WINDOW_KILLER_WIN_H_
