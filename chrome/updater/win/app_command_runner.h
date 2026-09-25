// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_APP_COMMAND_RUNNER_H_
#define CHROME_UPDATER_WIN_APP_COMMAND_RUNNER_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

// AppCommandRunner loads and runs a pre-registered command line from the
// registry.
class AppCommandRunner : public base::RefCountedThreadSafe<AppCommandRunner> {
 public:
  explicit AppCommandRunner(const std::wstring& app_id);

  // Creates an instance of `AppCommandRunner` object corresponding to `app_id`
  // and `command_id`.
  static HResultOr<scoped_refptr<AppCommandRunner>> LoadAppCommand(
      UpdaterScope scope,
      const std::wstring& app_id,
      const std::wstring& command_id);

  // Loads and returns a vector of `AppCommandRunner` objects corresponding to
  // "AutoRunOnOsUpgradeAppCommands" for `app_id`.
  static std::vector<scoped_refptr<AppCommandRunner>>
  LoadAutoRunOnOsUpgradeAppCommands(UpdaterScope scope,
                                    const std::wstring& app_id);

  // Runs the AppCommand with the provided `substitutions` and populates
  // `process` if successful. Returns `E_INVALIDARG` if the command line
  // contains `%CALLER_SID%`. The AppCommand is monitored for up to
  // `kWaitForAppInstaller`; if it has not exited by then, monitoring stops but
  // the process is not terminated, and `exit_code()` remains `std::nullopt`.
  HRESULT Run(base::span<const std::wstring> substitutions,
              base::Process& process);

  // Runs the AppCommand with the provided `substitutions` and `get_caller_sid`
  // and populates `process` if successful. If the command line contains
  // `%CALLER_SID%`, `get_caller_sid` is invoked to obtain the caller's SID; if
  // it returns an empty string, `Run` fails and returns `E_INVALIDARG`. The
  // AppCommand is monitored as described above.
  HRESULT Run(base::span<const std::wstring> substitutions,
              base::FunctionRef<std::wstring()> get_caller_sid,
              base::Process& process);

  // Returns the output that the AppCommand generated, if any.
  std::string output();

  // Returns the exit code of the most recent `Run`, if the AppCommand exited
  // while it was being monitored. Returns `std::nullopt` if `Run` has not been
  // called or failed, if the AppCommand is still being monitored, or if
  // monitoring timed out. Once `TimedWait` returns true, the value returned
  // here is final for that `Run`, even if the process exits later.
  std::optional<DWORD> exit_code() const;

  // Returns true if monitoring of the AppCommand has finished, either because
  // the AppCommand exited or because the monitoring timeout elapsed (in which
  // case the AppCommand may still be running). Returns false otherwise. Blocks
  // for up to `wait_delta` for monitoring to finish, returning as soon as it
  // does; the default `wait_delta` does not block.
  bool TimedWait(base::TimeDelta wait_delta = {});

  // Waits for the monitoring of the AppCommand execution to finish. The wait is
  // bounded by the monitoring timeout, rounded up to the one-second interval at
  // which the AppCommand is polled. Must only be called after a successful
  // `Run`.
  void Wait();

  // Overrides the monitoring timeout used by subsequent calls to `Run`. Must
  // not be called concurrently with `Run`.
  void SetTimeoutForTesting(base::TimeDelta timeout);

 protected:
  friend class base::RefCountedThreadSafe<AppCommandRunner>;
  virtual ~AppCommandRunner();

 private:
  // Separates a command line in `command_format` into an `executable` and
  // `parameters`. `executable` needs to be an absolute path, and additionally
  // needs to be under %programfiles% for System `scope`. Parameters on the
  // command line can be either hardcoded or placeholders from `%1` to `%9`.
  static HRESULT GetAppCommandFormatComponents(
      UpdaterScope scope,
      std::wstring command_format,
      base::FilePath& executable,
      std::vector<std::wstring>& parameters);

  // Formats a single `parameter` using
  // `base::internal::DoReplaceStringPlaceholders`. Any placeholder `%N` in
  // `parameter` is replaced with substitutions[N - 1]. Any literal `%` needs to
  // be escaped with a `%`. `%CALLER_SID%` (case-sensitive) is replaced with the
  // value returned by `get_caller_sid`.
  //
  // Returns `std::nullopt` if:
  // * a placeholder %N is encountered where N > substitutions.size().
  // * a literal `%` is not escaped with a `%`.
  // * `%CALLER_SID%` is encountered and `get_caller_sid` returns an empty
  //   string.
  static std::optional<std::wstring> FormatParameter(
      std::wstring_view parameter,
      base::span<const std::wstring> substitutions,
      base::FunctionRef<std::wstring()> get_caller_sid);

  // Formats a vector of `parameters` using the provided `substitutions` and
  // returns a resultant command line. Any placeholder `%N` in `parameters` is
  // replaced with substitutions[N - 1]. Any literal `%` needs to be escaped
  // with a `%`. `%CALLER_SID%` (case-sensitive) is replaced with the value
  // returned by `get_caller_sid`.
  //
  // The parameters are quoted after substitution if necessary so that each
  // parameter will be interpreted as a single command-line parameter according
  // to the rules for ::CommandLineToArgvW.
  //
  // Returns `std::nullopt` if:
  // * a placeholder %N is encountered where N > substitutions.size().
  // * a literal `%` is not escaped with a `%`.
  // * `%CALLER_SID%` is encountered and `get_caller_sid` returns an empty
  //   string.
  static std::optional<std::wstring> FormatAppCommandLine(
      const std::vector<std::wstring>& parameters,
      base::span<const std::wstring> substitutions,
      base::FunctionRef<std::wstring()> get_caller_sid);

  const std::wstring app_id_;
  base::FilePath executable_;
  std::vector<std::wstring> parameters_;
  base::WaitableEvent command_completed_event_;

  // How long `Run` monitors the AppCommand for. Defaults to
  // `kWaitForAppInstaller`.
  base::TimeDelta timeout_;

  // Access to the following object members must be serialized by using the
  // lock.
  mutable base::Lock lock_;
  std::string output_ GUARDED_BY(lock_);
  std::optional<DWORD> exit_code_ GUARDED_BY(lock_);

  friend class base::RefCountedThreadSafe<AppCommandRunner>;
  FRIEND_TEST_ALL_PREFIXES(AppCommandFormatComponentsInvalidPathsTest,
                           TestCases);
  FRIEND_TEST_ALL_PREFIXES(AppCommandFormatComponentsProgramFilesPathsTest,
                           TestCases);
  FRIEND_TEST_ALL_PREFIXES(AppCommandFormatParameterTest, TestCases);
  FRIEND_TEST_ALL_PREFIXES(AppCommandFormatComponentsAndCommandLineTest,
                           TestCases);
  FRIEND_TEST_ALL_PREFIXES(AppCommandExecuteTest, TestCases);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_APP_COMMAND_RUNNER_H_
