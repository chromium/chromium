// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// gcp_installer.exe is the GCPW installer run by Google Update or by device
// management tools. It unpacks the GCPW files (gcp.7z, embedded by
// gcp_installer.rc) into a new temp directory that other users can't write to,
// runs gcp_setup.exe from there with the same arguments, and returns its exit
// code.

#include <windows.h>

#include <shlobj.h>  // For IsUserAnAdmin().
#include <stdint.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/installer/util/lzma_util.h"

namespace {

// Exit codes for failures that happen before gcp_setup.exe runs. They are only
// used if there is no Windows error code for the failure, since callers such as
// Google Update show the exit code as a Windows error message.
enum ExitCode {
  kArchiveNotFound = 1,
  kCreateTempDirFailed = 2,
  kUnpackFailed = 3,
  kRunSetupFailed = 4,
};

// Returns `error`, or `fallback` if `error` is ERROR_SUCCESS, so that a failure
// is never reported as a success.
int ExitCodeFor(DWORD error, ExitCode fallback) {
  return error != ERROR_SUCCESS ? static_cast<int>(error) : fallback;
}

// Returns the GCPW archive embedded in `module`, or an empty span (with the
// error in ::GetLastError()) if it can't be found.
base::span<const uint8_t> GetArchive(HMODULE module) {
  HRSRC resource = ::FindResource(module, L"GCP_ARCHIVE", L"B7");
  HGLOBAL handle = resource ? ::LoadResource(module, resource) : nullptr;
  const void* data = handle ? ::LockResource(handle) : nullptr;
  if (!data) {
    return {};
  }
  // SAFETY: The resource is `SizeofResource()` bytes long and stays mapped
  // until `module` is unloaded.
  return UNSAFE_BUFFERS(base::span<const uint8_t>(
      static_cast<const uint8_t*>(data), ::SizeofResource(module, resource)));
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      wchar_t* /*lpCmdLine*/,
                      int /*nCmdShow*/) {
  // Same as EnableSecureDllLoading() in setup_utils.cc.
#if defined(COMPONENT_BUILD)
  ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
  ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif

  // Return before unpacking anything unless running as an administrator
  // (elevated, if UAC is on) or as SYSTEM, which is a member of the
  // Administrators group too. gcp_setup.exe needs administrative privileges to
  // install or uninstall GCPW, and does the same check.
  if (!::IsUserAnAdmin()) {
    return ERROR_ACCESS_DENIED;
  }

  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);

  // Don't log to a file. By default, base would create a debug.log file next
  // to the executable.
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_NONE;
  logging::InitLogging(logging_settings);

  const base::span<const uint8_t> archive = GetArchive(hInstance);
  if (archive.empty()) {
    return ExitCodeFor(::GetLastError(), kArchiveNotFound);
  }

  // Unpack into a new directory that other users can't write to: when running
  // as SYSTEM (e.g. when run by Google Update), it is created under SystemTemp
  // instead of C:\Windows\Temp. See base::GetSecureTempDirectory(). The
  // directory is deleted when `temp_dir` goes out of scope.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir(FILE_PATH_LITERAL("gcp"))) {
    return ExitCodeFor(::GetLastError(), kCreateTempDirFailed);
  }
  // Use LzmaUtilImpl rather than UnPackArchiveBuffer(), which doesn't return
  // the Windows error code of a failure.
  LzmaUtilImpl lzma_util;
  if (lzma_util.OpenArchive(archive) != UNPACK_NO_ERROR ||
      lzma_util.UnPack(temp_dir.GetPath()) != UNPACK_NO_ERROR) {
    return ExitCodeFor(lzma_util.GetErrorCode().value_or(ERROR_SUCCESS),
                       kUnpackFailed);
  }

  // Run gcp_setup.exe with this installer's arguments.
  base::CommandLine setup_command_line(
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("gcp_setup.exe")));
  setup_command_line.AppendArguments(*base::CommandLine::ForCurrentProcess(),
                                     /*include_program=*/false);
  base::LaunchOptions options;
  options.current_directory = temp_dir.GetPath();
  base::Process process = base::LaunchProcess(setup_command_line, options);
  int exit_code = 0;
  if (!process.IsValid() || !process.WaitForExit(&exit_code)) {
    return ExitCodeFor(::GetLastError(), kRunSetupFailed);
  }

  // gcp_setup.exe starts its crash handler from `temp_dir`, which can keep the
  // directory in use for a moment after gcp_setup.exe exits, so retry deleting
  // it for a few seconds.
  const base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(5);
  while (!temp_dir.Delete() && base::TimeTicks::Now() < deadline) {
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }
  return exit_code;
}
