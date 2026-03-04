// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_
#define CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_

#include "base/process/process.h"
#include "base/types/expected.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
}  // namespace base

namespace chrome {

// Attempt to launch an isolated browser process with the command line
// `command_line`. If successful, a `base::Process` is returned, if not then an
// HRESULT containing the error launching the process.
base::expected<base::Process, HRESULT> LaunchIsolatedBrowser(
    const base::CommandLine& command_line);

// Returns true if the platform configuration indicates that the browser should
// launch isolated.
bool IsIsolationEnabled(const base::CommandLine& command_line);

// Returns true if the currently running browser process is isolated. Note, that
// not all `IsolatedBrowser` returned from the `Launch' function above will be
// fully isolated, so checking the command line alone is not sufficient.
bool IsRunningIsolated();

}  // namespace chrome

#endif  // CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_
