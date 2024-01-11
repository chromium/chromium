// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_RESULT_H_

// TODO(b/304553492): Use this for recording per-command metrics.
enum class CommandResult {
  // The command did not see any unexpected errors.
  kSuccess,
  // An unexpected failure occurred, which should possibly result in chirp
  // alerts.
  kFailure
};

// TODO(dmurph): Put utilities for recording command metrics into this file.
// b/304553492

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_RESULT_H_
