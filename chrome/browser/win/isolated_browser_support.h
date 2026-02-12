// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_
#define CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_

#include <memory>

#include "base/process/process.h"
#include "base/types/expected.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
}  // namespace base

namespace chrome {

// A convenience class to manage the lifetime of the isolated browser process
// and the job object that manages the process lifetimes.
class IsolatedBrowser {
 public:
  // Attempt to launch an isolated browser process with the command line
  // `command_line`. If successful, an `IsolatedBrowser` is returned, if not
  // then an HRESULT containing the error launching the process.
  static base::expected<std::unique_ptr<IsolatedBrowser>, HRESULT> Launch(
      const base::CommandLine& command_line);

  IsolatedBrowser(IsolatedBrowser&& other) = delete;
  IsolatedBrowser& operator=(IsolatedBrowser&& other) = delete;
  IsolatedBrowser(const IsolatedBrowser&) = delete;
  IsolatedBrowser& operator=(const IsolatedBrowser&) = delete;

  ~IsolatedBrowser();

  // Wait for exit of the isolated browser process, and return its exit code.
  // This should typically be then returned as the exit code of the calling
  // process.
  int WaitForExit() const;

 private:
  IsolatedBrowser(base::Process process, base::win::ScopedHandle job);

  // The job object holds all the processes including the original parent
  // process, ensuring that if the launcher terminates then any isolated browser
  // also does. This means that the isolated browser can guarantee that its
  // lifetime always exceeds the lifetime of its parent process.
  const base::win::ScopedHandle job_;

  // Handle to the isolated browser process, returned from the elevated service.
  const base::Process process_;
};

// Returns true if the platform configuration indicates that the browser should
// launch isolated.
bool IsIsolationEnabled(const base::CommandLine& command_line);

// Returns true if the currently running browser process is isolated. Note, that
// not all `IsolatedBrowser` returned from the `Launch' function above will be
// fully isolated, so checking the command line alone is not sufficient.
bool IsRunningIsolated();

}  // namespace chrome

#endif  // CHROME_BROWSER_WIN_ISOLATED_BROWSER_SUPPORT_H_
