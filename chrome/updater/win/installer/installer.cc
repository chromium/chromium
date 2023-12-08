// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GoogleUpdateSetup.exe is the first exe that is run when chrome is being
// installed. It has two main jobs:
//   1) unpack the resources (possibly decompressing some)
//   2) run the real installer (updater.exe) with appropriate flags (--install).
//
// All files needed by the updater are archived together as an uncompressed
// LZMA file, which is further compressed as one file, and inserted as a
// binary resource in the resource section of the setup program.

#include "chrome/updater/win/installer/installer.h"

#include <shellapi.h>
#include <shlobj.h>

#include <optional>
#include <string>

// TODO(crbug.com/1128529): remove the dependencies on //base/ to reduce the
// code size.
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/installer/configuration.h"
#include "chrome/updater/win/installer/installer_constants.h"
#include "chrome/updater/win/installer/pe_resource.h"

namespace updater {

using PathString = StackString<MAX_PATH>;

namespace {

// Returns the tag if the tag can be extracted. The tag is read from the
// program file image used to create this process. The implementation of this
// function only handles UTF8 tags.
std::string ExtractTag() {
  PathString path;
  return (::GetModuleFileName(nullptr, path.get(), path.capacity()) > 0 &&
          ::GetLastError() == ERROR_SUCCESS)
             ? tagging::ExeReadTag(base::FilePath(path.get()))
             : std::string();
}

}  // namespace

// This structure passes data back and forth for the processing
// of resource callbacks.
struct Context {
  // Input to the call back method. Specifies the dir to save resources into.
  const wchar_t* base_path = nullptr;

  // First output from call back method. Specifies the path of resource archive.
  raw_ptr<PathString> updater_resource_path = nullptr;
};

// Calls CreateProcess with good default parameters and waits for the process to
// terminate returning the process exit code. In case of CreateProcess failure,
// returns a results object with the provided codes as follows:
// - ERROR_FILE_NOT_FOUND: (file_not_found_code, attributes of setup.exe).
// - ERROR_PATH_NOT_FOUND: (path_not_found_code, attributes of setup.exe).
// - Otherwise: (generic_failure_code, CreateProcess error code).
// In case of error waiting for the process to exit, returns a results object
// with (WAIT_FOR_PROCESS_FAILED, last error code). Otherwise, returns a results
// object with the subprocess's exit code.
ProcessExitResult RunProcessAndWait(const wchar_t* exe_path, wchar_t* cmdline) {
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(exe_path, cmdline, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    // Split specific failure modes. If the process couldn't be launched because
    // its file/path couldn't be found, report its attributes in ExtraCode1.
    // This will help diagnose the prevalence of launch failures due to Image
    // File Execution Options tampering. See https://crbug.com/672813 for more
    // details.
    const DWORD last_error = ::GetLastError();
    const DWORD attributes = ::GetFileAttributes(exe_path);
    switch (last_error) {
      case ERROR_FILE_NOT_FOUND:
        return ProcessExitResult(RUN_SETUP_FAILED_FILE_NOT_FOUND, attributes);
      case ERROR_PATH_NOT_FOUND:
        return ProcessExitResult(RUN_SETUP_FAILED_PATH_NOT_FOUND, attributes);
      default:
        break;
    }
    // Lump all other errors into a distinct failure bucket.
    return ProcessExitResult(RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS,
                             last_error);
  }

  ::CloseHandle(pi.hThread);

  DWORD exit_code = SUCCESS_EXIT_CODE;
  DWORD wr = ::WaitForSingleObject(pi.hProcess, INFINITE);
  if (WAIT_OBJECT_0 != wr || !::GetExitCodeProcess(pi.hProcess, &exit_code)) {
    // Note:  We've assumed that WAIT_OBJCT_0 != wr means a failure.  The call
    // could return a different object but since we never spawn more than one
    // sub-process at a time that case should never happen.
    return ProcessExitResult(WAIT_FOR_PROCESS_FAILED, ::GetLastError());
  }

  ::CloseHandle(pi.hProcess);

  return ProcessExitResult(exit_code);
}

// Windows defined callback used in the EnumResourceNames call. For each
// matching resource found, the callback is invoked and at this point we write
// it to disk. We expect resource names to start with the 'updater' prefix.
// Any other name is treated as an error.
BOOL CALLBACK OnResourceFound(HMODULE module,
                              const wchar_t* type,
                              wchar_t* name,
                              LONG_PTR context) {
  Context* ctx = reinterpret_cast<Context*>(context);
  if (!ctx) {
    return FALSE;
  }

  if (!StrStartsWith(name, kUpdaterArchivePrefix)) {
    return FALSE;
  }

  PEResource resource(name, type, module);
  if (!resource.IsValid() || resource.Size() < 1) {
    return FALSE;
  }

  PathString full_path;
  if (!full_path.assign(ctx->base_path) || !full_path.append(name) ||
      !resource.WriteToDisk(full_path.get())) {
    return FALSE;
  }

  if (!ctx->updater_resource_path->assign(full_path.get())) {
    return FALSE;
  }

  return TRUE;
}

std::optional<base::FilePath> FindOfflineDir(
    const base::FilePath& unpack_path) {
  const base::FilePath base_offline_dir =
      unpack_path.Append(L"bin").Append(L"Offline");
  if (!base::PathExists(base_offline_dir)) {
    return std::nullopt;
  }
  base::FileEnumerator file_enumerator(base_offline_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    if (IsGuid(path.BaseName().value())) {
      return path;
    }
  }
  return std::nullopt;
}

// Finds and writes to disk resources of type 'B7' (7zip archive). Returns false
// if there is a problem in writing any resource to disk.
ProcessExitResult UnpackBinaryResources(const Configuration& configuration,
                                        HMODULE module,
                                        const wchar_t* base_path,
                                        PathString* archive_path) {
  // Prepare the input to OnResourceFound method that needs a location where
  // it will write all the resources.
  Context context = {base_path, archive_path};

  // Get the resources of type 'B7' (7zip archive).
  if (!::EnumResourceNames(module, kLZMAResourceType, OnResourceFound,
                           reinterpret_cast<LONG_PTR>(&context))) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_ARCHIVE, ::GetLastError());
  }

  if (archive_path->length() == 0) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_ARCHIVE);
  }

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  return exit_code;
}

ProcessExitResult BuildCommandLineArguments(const wchar_t* cmd_line,
                                            wchar_t* cmd_line_args,
                                            size_t cmd_line_args_capacity) {
  CHECK(cmd_line);
  CHECK(cmd_line_args);
  CHECK(cmd_line_args_capacity);

  *cmd_line_args = '\0';
  CommandString args;

  // Append the command line arguments in `cmd_line` first.
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv(
      ::CommandLineToArgvW(cmd_line, &num_args));
  for (int i = 1; i != num_args; ++i) {
    if (!args.append(L" ") ||
        !args.append(
            base::CommandLine::QuoteForCommandLineToArgvW(argv.get()[i])
                .c_str())) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  }

  // Handle the tag. Use the tag from the --tag command line argument if such
  // argument exists. If --tag is present in `argv`, then it is going to be
  // handed over to the updater, along with the other arguments. Otherwise, try
  // extracting a tag embedded in the program image of the meta installer.
  if (![&argv, num_args] {
        // Returns true if the --tag argument is present on the command line.
        constexpr wchar_t kTagSwitch[] = L"--tag=";
        for (int i = 1; i != num_args; ++i) {
          if (memcmp(argv.get()[i], kTagSwitch, sizeof(kTagSwitch)) == 0) {
            return true;
          }
        }
        return false;
      }()) {
    const std::string tag = ExtractTag();
    if (!tag.empty()) {
      if (!args.append(L" --tag=") ||
          !args.append(base::SysUTF8ToWide(tag).c_str())) {
        return ProcessExitResult(COMMAND_STRING_OVERFLOW);
      }
    }
  }

  // If there is nothing, return an error.
  if (!args.length()) {
    return ProcessExitResult(INVALID_OPTION);
  }

  // Append logging-related arguments for debugging purposes.
  if (!args.append(
          base::SysUTF8ToWide(base::StrCat({" --", kEnableLoggingSwitch, " --",
                                            kLoggingModuleSwitch, "=",
                                            kLoggingModuleSwitchValue}))
              .c_str())) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  SafeStrCopy(cmd_line_args, cmd_line_args_capacity, args.get());
  return ProcessExitResult(SUCCESS_EXIT_CODE);
}

// Executes updater.exe, waits for it to finish and returns the exit code.
ProcessExitResult RunSetup(const wchar_t* setup_path,
                           const wchar_t* cmd_line_args) {
  CHECK(setup_path && *setup_path);
  CHECK(cmd_line_args && *cmd_line_args);

  CommandString cmd_line;

  // Put the quoted path to setup.exe in cmd_line first, then the args.
  if (!cmd_line.assign(
          base::StrCat(
              {base::CommandLine::QuoteForCommandLineToArgvW(setup_path), L" ",
               cmd_line_args})
              .c_str())) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  return RunProcessAndWait(setup_path, cmd_line.get());
}

ProcessExitResult HandleRunElevated(const base::CommandLine& command_line) {
  CHECK(!::IsUserAnAdmin());
  CHECK(!command_line.HasSwitch(kCmdLinePrefersUser));

  if (command_line.HasSwitch(kCmdLineExpectElevated)) {
    VLOG(1) << __func__ << "Unexpected elevation loop! "
            << command_line.GetCommandLineString();
    return ProcessExitResult(UNABLE_TO_ELEVATE_METAINSTALLER);
  }

  // The metainstaller is elevated because unpacking its files and running
  // updater.exe must happen from a secure directory.
  base::CommandLine elevated_command_line = command_line;
  elevated_command_line.AppendSwitchASCII(kCmdLineExpectElevated, {});
  ASSIGN_OR_RETURN(DWORD result,
                   RunElevated(command_line.GetProgram(),
                               elevated_command_line.GetArgumentsString()),
                   [](HRESULT error) {
                     return ProcessExitResult(
                         RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS, error);
                   });
  return ProcessExitResult(result);
}

ProcessExitResult HandleRunDeElevated(const base::CommandLine& command_line) {
  CHECK(::IsUserAnAdmin());

  if (command_line.HasSwitch(kCmdLineExpectDeElevated)) {
    VLOG(1) << __func__ << "Unexpected de-elevation loop! "
            << command_line.GetCommandLineString();
    return ProcessExitResult(UNABLE_TO_DE_ELEVATE_METAINSTALLER);
  }

  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  CHECK(com_initializer.Succeeded());

  // Deelevate the metainstaller.
  HRESULT hr =
      RunDeElevated(command_line.GetProgram().value(), [&command_line] {
        base::CommandLine de_elevate_command_line = command_line;
        de_elevate_command_line.AppendSwitch(kCmdLineExpectDeElevated);
        return de_elevate_command_line.GetArgumentsString();
      }());
  return SUCCEEDED(hr)
             ? ProcessExitResult(SUCCESS_EXIT_CODE)
             : ProcessExitResult(RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS, hr);
}

ProcessExitResult InstallerMain(HMODULE module) {
  CHECK(EnableSecureDllLoading());
  EnableProcessHeapMetadataProtection();

  if (base::win::GetVersion() < base::win::Version::WIN10) {
    return ProcessExitResult(UNSUPPORTED_WINDOWS_VERSION);
  }

  CommandString cmd_line_args;
  ProcessExitResult args_result = BuildCommandLineArguments(
      ::GetCommandLineW(), cmd_line_args.get(), cmd_line_args.capacity());
  if (args_result.exit_code != SUCCESS_EXIT_CODE) {
    return args_result;
  }

  // Both `RunElevated` and `RunDeElevated` use shell APIs to run the process,
  // which can have issues with relative paths. So we use the full exe path for
  // the program in the command line.
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    return ProcessExitResult(UNABLE_TO_GET_EXE_PATH);
  }
  const base::CommandLine command_line =
      base::CommandLine::FromString(base::StrCat(
          {base::CommandLine::QuoteForCommandLineToArgvW(exe_path.value()),
           L" ", cmd_line_args.get()}));

  const UpdaterScope scope = GetUpdaterScopeForCommandLine(command_line);

  if (!::IsUserAnAdmin() && IsSystemInstall(scope)) {
    ProcessExitResult run_elevated_result = HandleRunElevated(command_line);
    if (run_elevated_result.exit_code !=
            RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS ||
        !IsPrefersForCommandLine(command_line)) {
      return run_elevated_result;
    }

    // "needsadmin=prefers" case: Could not elevate. So fall through to
    // install as a per-user app.
    if (!cmd_line_args.append(L" --") ||
        !cmd_line_args.append(
            base::SysUTF8ToWide(kCmdLinePrefersUser).c_str())) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  } else if (::IsUserAnAdmin() && !IsSystemInstall(scope) && IsUACOn()) {
    return HandleRunDeElevated(command_line);
  }

  base::CommandLine::Init(0, nullptr);
  *base::CommandLine::ForCurrentProcess() = command_line;
  InitLogging(scope);
  VLOG(1) << command_line.GetCommandLineString();

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  // Parse configuration from the command line and resources.
  Configuration configuration;
  if (!configuration.Initialize(module)) {
    return ProcessExitResult(GENERIC_INITIALIZATION_FAILURE, ::GetLastError());
  }

  // Exit early if an invalid switch was found on the command line.
  if (configuration.has_invalid_switch()) {
    return ProcessExitResult(INVALID_OPTION);
  }

  // First get a path where we can extract the resource payload, which is
  // a compressed LZMA archive of a single file.
  std::optional<base::ScopedTempDir> base_path_owner = CreateSecureTempDir();
  if (!base_path_owner) {
    return ProcessExitResult(TEMP_DIR_FAILED);
  }

  PathString base_path;
  if (!base_path.assign(
          base_path_owner->GetPath().AsEndingWithSeparator().value().c_str())) {
    return ProcessExitResult(PATH_STRING_OVERFLOW);
  }

  PathString compressed_archive;
  exit_code = UnpackBinaryResources(configuration, module, base_path.get(),
                                    &compressed_archive);

  // Create a temp folder where the archives are unpacked.
  std::optional<base::ScopedTempDir> temp_path = CreateSecureTempDir();
  if (!temp_path) {
    return ProcessExitResult(TEMP_DIR_FAILED);
  }

  const base::FilePath unpack_path = temp_path->GetPath();

  // Unpack the compressed archive to extract the uncompressed archive file.
  UnPackStatus unpack_status =
      UnPackArchive(base::FilePath(compressed_archive.get()), unpack_path,
                    /*output_file=*/nullptr);
  if (unpack_status != UNPACK_NO_ERROR) {
    return ProcessExitResult(UNPACKING_FAILED);
  }

  // Unpack the uncompressed archive to extract the updater files.
  base::FilePath uncompressed_archive =
      unpack_path.Append(FILE_PATH_LITERAL("updater.7z"));
  unpack_status =
      UnPackArchive(uncompressed_archive, unpack_path, /*output_file=*/nullptr);
  if (unpack_status != UNPACK_NO_ERROR) {
    return ProcessExitResult(UNPACKING_FAILED);
  }

  // While unpacking the binaries, we paged in a whole bunch of memory that
  // we don't need anymore.  Let's give it back to the pool before running
  // setup.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), static_cast<SIZE_T>(-1),
                             static_cast<SIZE_T>(-1));

  // Determine if an offlinedir is embedded and, if it is, add an
  // --offlinedir={GUID} switch to indicate that an offline install should
  // be performed.
  const std::optional<base::FilePath> offline_dir = FindOfflineDir(unpack_path);
  if (offline_dir.has_value()) {
    if (!cmd_line_args.append(L" --") ||
        !cmd_line_args.append(base::SysUTF8ToWide(kOfflineDirSwitch).c_str()) ||
        !cmd_line_args.append(L"=") ||
        !cmd_line_args.append(offline_dir->BaseName().value().c_str())) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  }

  PathString setup_path;
  if (!setup_path.assign(unpack_path.value().c_str()) ||
      !setup_path.append(L"\\bin\\updater.exe")) {
    exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);
  }

  if (exit_code.IsSuccess()) {
    exit_code = RunSetup(setup_path.get(), cmd_line_args.get());

    // Because there is a race condition between the exit of the setup process
    // and deleting its program file, the scoped temporary directory may fail
    // to clean up the directory because the directory is not empty. To avoid
    // this race condition, delete the program file before returning from the
    // function.
    base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(5);
    while (base::TimeTicks::Now() < deadline) {
      if (base::DeleteFile(base::FilePath(setup_path.get()))) {
        return exit_code;
      }
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }
    VLOG(1) << "Setup file can leak on file system: " << setup_path.get();
  }

  return exit_code;
}

ProcessExitResult WMain(HMODULE module) {
  const updater::ProcessExitResult result = InstallerMain(module);
  VLOG(1) << "Metainstaller WMain returned: " << result.exit_code
          << ", Windows error: " << result.windows_error;
  return result;
}

}  // namespace updater
