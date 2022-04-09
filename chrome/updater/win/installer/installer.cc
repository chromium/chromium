// Copyright 2019 The Chromium Authors. All rights reserved.
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

#include "base/memory/raw_ptr.h"

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <initializer_list>
#include <string>

// TODO(crbug.com/1128529): remove the dependencies on //base/ to reduce the
// code size.
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/installer/configuration.h"
#include "chrome/updater/win/installer/installer_constants.h"
#include "chrome/updater/win/installer/pe_resource.h"
#include "chrome/updater/win/tag_extractor.h"
#include "chrome/updater/win/win_util.h"

namespace updater {

using PathString = StackString<MAX_PATH>;

namespace {

// If the process is running with Admin privileges, a secure unpack location
// under %ProgramFiles% (a directory that only admins can write to by default)
// is used.
bool CreateSecureTempDir(installer::SelfCleaningTempDir& temp_path) {
  base::FilePath temp_dir;
  if (!base::PathService::Get(
          ::IsUserAnAdmin() ? base::DIR_PROGRAM_FILES : base::DIR_TEMP,
          &temp_dir)) {
    return false;
  }

  temp_dir = temp_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
                 .AppendASCII(PRODUCT_FULLNAME_STRING);

  if (!temp_path.Initialize(temp_dir, L"UPDATER_TEMP_DIR")) {
    PLOG(ERROR) << "Could not create temporary path.";
    return false;
  }

  VLOG(2) << "Created temp path " << temp_path.path().value();
  return true;
}

// Initializes |temp_path| to "Temp" within the target directory, and
// |unpack_path| to a random directory beginning with "source" within
// |temp_path|. Returns false on error.
bool CreateTemporaryAndUnpackDirectories(
    installer::SelfCleaningTempDir& temp_path,
    base::FilePath* unpack_path) {
  DCHECK(unpack_path);

  // Because there is no UpdaterScope yet, ::IsUserAnAdmin() is used to
  // determine whether the process is running with Admin privileges.
  if (!CreateSecureTempDir(temp_path)) {
    return false;
  }

  if (!base::CreateTemporaryDirInDir(temp_path.path(), L"source",
                                     unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    return false;
  }

  return true;
}

// Returns the tag if the tag can be extracted. The tag is read from the
// program file image used to create this process. Google is using UTF8 tags but
// other embedders could use UTF16. The UTF16 tag not only uses a different
// character width, but the tag is inserted in a different way.]
// The implementation of this function only handles UTF8 tags.
std::string ExtractTag() {
  PathString path;
  return (::GetModuleFileName(nullptr, path.get(), path.capacity()) > 0 &&
          ::GetLastError() == ERROR_SUCCESS)
             ? ExtractTagFromFile(path.get(), TagEncoding::kUtf8)
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
  if (!ctx)
    return FALSE;

  if (!StrStartsWith(name, kUpdaterArchivePrefix))
    return FALSE;

  PEResource resource(name, type, module);
  if (!resource.IsValid() || resource.Size() < 1)
    return FALSE;

  PathString full_path;
  if (!full_path.assign(ctx->base_path) || !full_path.append(name) ||
      !resource.WriteToDisk(full_path.get())) {
    return FALSE;
  }

  if (!ctx->updater_resource_path->assign(full_path.get()))
    return FALSE;

  return TRUE;
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

  if (archive_path->length() == 0)
    return ProcessExitResult(UNABLE_TO_EXTRACT_ARCHIVE);

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  return exit_code;
}

ProcessExitResult BuildCommandLineArguments(const wchar_t* cmd_line,
                                            wchar_t* cmd_line_args,
                                            size_t cmd_line_args_capacity) {
  DCHECK(cmd_line);
  DCHECK(cmd_line_args);
  DCHECK(cmd_line_args_capacity);

  *cmd_line_args = '\0';
  CommandString args;

  // Append the command line arguments in `cmd_line` first.
  int num_args = 0;
  wchar_t** const arg_list = ::CommandLineToArgvW(cmd_line, &num_args);
  for (int i = 1; i != num_args; ++i) {
    if (!args.append(L" ") || !args.append(arg_list[i])) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  }

  // Handle the tag. Use the tag from the --tag command line argument if such
  // argument exists. If --tag is present in the arg_list, then it is going
  // to be handed over to the updater, along with the other arguments.
  // Otherwise, try extracting a tag embedded in the program image of the meta
  // installer.
  if (![arg_list, num_args]() {
        // Returns true if the --tag argument is present on the command line.
        constexpr wchar_t kTagSwitch[] = L"--tag=";
        for (int i = 1; i != num_args; ++i) {
          if (memcmp(arg_list[i], kTagSwitch, sizeof(kTagSwitch)) == 0)
            return true;
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

  // Append logging-related arguments for debugging purposes, at least for
  // now.
  if (!args.append(
          L" --enable-logging "
          L"--vmodule=*/chrome/updater/*=2,*/components/winhttp/*=2")) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  SafeStrCopy(cmd_line_args, cmd_line_args_capacity, args.get());
  return ProcessExitResult(SUCCESS_EXIT_CODE);
}

// Executes updater.exe, waits for it to finish and returns the exit code.
ProcessExitResult RunSetup(const wchar_t* setup_path,
                           const wchar_t* cmd_line_args) {
  DCHECK(setup_path && *setup_path);
  DCHECK(cmd_line_args && *cmd_line_args);

  PathString setup_exe;

  if (!setup_exe.assign(setup_path))
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);

  CommandString cmd_line;

  // Put the quoted path to setup.exe in cmd_line first, then the args.
  if (!cmd_line.assign(L"\"") || !cmd_line.append(setup_exe.get()) ||
      !cmd_line.append(L"\"") || !cmd_line.append(cmd_line_args)) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  return RunProcessAndWait(setup_exe.get(), cmd_line.get());
}

// Returns true if the supplied path supports ACLs.
bool IsAclSupportedForPath(const wchar_t* path) {
  PathString volume;
  DWORD flags = 0;
  return ::GetVolumePathName(path, volume.get(),
                             static_cast<DWORD>(volume.capacity())) &&
         ::GetVolumeInformation(volume.get(), nullptr, 0, nullptr, nullptr,
                                &flags, nullptr, 0) &&
         (flags & FILE_PERSISTENT_ACLS);
}

// Retrieves the SID of the default owner for objects created by this user
// token (accounting for different behavior under UAC elevation, etc.).
// NOTE: On success the |sid| parameter must be freed with LocalFree().
bool GetCurrentOwnerSid(wchar_t** sid) {
  HANDLE token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;

  DWORD size = 0;
  bool result = false;
  // We get the TokenOwner rather than the TokenUser because e.g. under UAC
  // elevation we want the admin to own the directory rather than the user.
  ::GetTokenInformation(token, TokenOwner, nullptr, 0, &size);
  if (size && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    if (TOKEN_OWNER* owner =
            reinterpret_cast<TOKEN_OWNER*>(::LocalAlloc(LPTR, size))) {
      if (::GetTokenInformation(token, TokenOwner, owner, size, &size))
        result = !!::ConvertSidToStringSid(owner->Owner, sid);
      ::LocalFree(owner);
    }
  }
  ::CloseHandle(token);
  return result;
}

// Populates |sd| suitable for use when creating directories within |path| with
// ACLs allowing access to only the current owner, admin, and system.
// NOTE: On success the |sd| parameter must be freed with LocalFree().
bool SetSecurityDescriptor(const wchar_t* path, PSECURITY_DESCRIPTOR* sd) {
  *sd = nullptr;
  // We succeed without doing anything if ACLs aren't supported.
  if (!IsAclSupportedForPath(path))
    return true;

  wchar_t* sid = nullptr;
  if (!GetCurrentOwnerSid(&sid))
    return false;

  // The largest SID is under 200 characters, so 300 should give enough slack.
  StackString<300> sddl;
  bool result = sddl.append(
                    L"D:PAI"         // Protected, auto-inherited DACL.
                    L"(A;;FA;;;BA)"  // Admin: Full control.
                    L"(A;OIIOCI;GA;;;BA)"
                    L"(A;;FA;;;SY)"  // System: Full control.
                    L"(A;OIIOCI;GA;;;SY)"
                    L"(A;OIIOCI;GA;;;CO)"  // Owner: Full control.
                    L"(A;;FA;;;") &&
                sddl.append(sid) && sddl.append(L")");
  if (result) {
    result = !!::ConvertStringSecurityDescriptorToSecurityDescriptor(
        sddl.get(), SDDL_REVISION_1, sd, nullptr);
  }

  ::LocalFree(sid);
  return result;
}

// Creates a temporary directory under |base_path| and returns the full path
// of created directory in |work_dir|. If successful return true, otherwise
// false.  When successful, the returned |work_dir| will always have a trailing
// backslash and this function requires that |base_path| always includes a
// trailing backslash as well.
// We do not use GetTempFileName here to avoid running into AV software that
// might hold on to the temp file as soon as we create it and then we can't
// delete it and create a directory in its place.  So, we use our own mechanism
// for creating a directory with a hopefully-unique name.  In the case of a
// collision, we retry a few times with a new name before failing.
bool CreateWorkDir(const wchar_t* base_path,
                   PathString* work_dir,
                   ProcessExitResult* exit_code) {
  *exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);
  if (!work_dir->assign(base_path) || !work_dir->append(kTempPrefix))
    return false;

  // Store the location where we'll append the id.
  size_t end = work_dir->length();

  // Check if we'll have enough buffer space to continue.
  // The name of the directory will use up 11 chars and then we need to append
  // the trailing backslash and a terminator.  We've already added the prefix
  // to the buffer, so let's just make sure we've got enough space for the rest.
  if ((work_dir->capacity() - end) < (_countof("fffff.tmp") + 1))
    return false;

  // Add an ACL if supported by the filesystem. Otherwise system-level installs
  // are potentially vulnerable to file squatting attacks.
  SECURITY_ATTRIBUTES sa = {};
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  if (!SetSecurityDescriptor(base_path, &sa.lpSecurityDescriptor)) {
    *exit_code =
        ProcessExitResult(UNABLE_TO_SET_DIRECTORY_ACL, ::GetLastError());
    return false;
  }

  unsigned int id;
  *exit_code = ProcessExitResult(UNABLE_TO_GET_WORK_DIRECTORY);
  for (int max_attempts = 10; max_attempts; --max_attempts) {
    ::RtlGenRandom(&id, sizeof(id));  // Try a different name.

    // This converts 'id' to a string in the format "78563412" on windows
    // because of little endianness, but we don't care since it's just
    // a name. Since we checked capaity at the front end, we don't need to
    // duplicate it here.
    HexEncode(&id, sizeof(id), work_dir->get() + end,
              work_dir->capacity() - end);

    // We only want the first 5 digits to remain within the 8.3 file name
    // format (compliant with previous implementation).
    work_dir->truncate_at(end + 5);

    // for consistency with the previous implementation which relied on
    // GetTempFileName, we append the .tmp extension.
    work_dir->append(L".tmp");

    if (::CreateDirectory(work_dir->get(),
                          sa.lpSecurityDescriptor ? &sa : nullptr)) {
      // Yay!  Now let's just append the backslash and we're done.
      work_dir->append(L"\\");
      *exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);
      break;
    }
  }

  if (sa.lpSecurityDescriptor)
    LocalFree(sa.lpSecurityDescriptor);
  return exit_code->IsSuccess();
}

// Creates and returns a temporary directory in |work_dir| that can be used to
// extract updater payload. |work_dir| ends with a path separator.
bool GetWorkDir(HMODULE module,
                PathString* work_dir,
                ProcessExitResult* exit_code) {
  PathString base_path;
  // Use the current module directory as a secure base path.
  DWORD len = ::GetModuleFileName(module, base_path.get(),
                                  static_cast<DWORD>(base_path.capacity()));
  if (len >= base_path.capacity() || !len)
    return false;  // Can't even get current directory? Return an error.

  wchar_t* name = GetNameFromPathExt(base_path.get(), len);
  if (name == base_path.get())
    return false;  // There was no directory in the string!  Bail out.

  *name = L'\0';

  *exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);
  return CreateWorkDir(base_path.get(), work_dir, exit_code);
}

// Returns true for ".." and "." directories.
bool IsCurrentOrParentDirectory(const wchar_t* dir) {
  return dir && dir[0] == L'.' &&
         (dir[1] == L'\0' || (dir[1] == L'.' && dir[2] == L'\0'));
}

ProcessExitResult HandleRunElevated(const base::CommandLine& command_line) {
  DCHECK(!::IsUserAnAdmin());
  DCHECK(!command_line.HasSwitch(kCmdLinePrefersUser));

  if (command_line.HasSwitch(kCmdLineExpectElevated)) {
    VLOG(1) << __func__ << "Unexpected elevation loop! "
            << command_line.GetCommandLineString();
    return ProcessExitResult(UNABLE_TO_ELEVATE_METAINSTALLER);
  }

  // The metainstaller is elevated because unpacking its files and running
  // updater.exe must happen from a secure directory.
  DWORD exit_code = 0;
  HRESULT hr = RunElevated(
      command_line.GetProgram(),
      [&command_line]() {
        base::CommandLine elevate_command_line = command_line;
        elevate_command_line.AppendSwitchASCII(kCmdLineExpectElevated, {});
        return elevate_command_line.GetArgumentsString();
      }(),
      &exit_code);
  return SUCCEEDED(hr)
             ? ProcessExitResult(exit_code)
             : ProcessExitResult(RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS, hr);
}

ProcessExitResult WMain(HMODULE module) {
  CommandString cmd_line_args;
  ProcessExitResult args_result = BuildCommandLineArguments(
      ::GetCommandLineW(), cmd_line_args.get(), cmd_line_args.capacity());
  if (args_result.exit_code != SUCCESS_EXIT_CODE)
    return args_result;

  if (CommandString cmd_line;
      !::IsUserAnAdmin() && cmd_line.assign(L"updater.exe") &&
      cmd_line.append(cmd_line_args.get()) &&
      GetUpdaterScopeForCommandLine(base::CommandLine::FromString(
          cmd_line.get())) == UpdaterScope::kSystem) {
    ProcessExitResult run_elevated_result =
        HandleRunElevated(base::CommandLine::FromString(::GetCommandLineW()));
    if (run_elevated_result.exit_code !=
        RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS) {
      return run_elevated_result;
    }

    // Could not elevate. So fall through to install as a per-user app.
    if (!cmd_line_args.append(L" --") ||
        !cmd_line_args.append(
            base::SysUTF8ToWide(kCmdLinePrefersUser).c_str())) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  }

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  // Parse configuration from the command line and resources.
  Configuration configuration;
  if (!configuration.Initialize(module))
    return ProcessExitResult(GENERIC_INITIALIZATION_FAILURE, ::GetLastError());

  // Exit early if an invalid switch was found on the command line.
  if (configuration.has_invalid_switch())
    return ProcessExitResult(INVALID_OPTION);

  // First get a path where we can extract the resource payload, which is
  // a compressed LZMA archive of a single file.
  base::ScopedTempDir base_path_owner;
  PathString base_path;
  if (!GetWorkDir(module, &base_path, &exit_code))
    return exit_code;
  if (!base_path_owner.Set(base::FilePath(base_path.get()))) {
    ::DeleteFile(base_path.get());
    return ProcessExitResult(static_cast<DWORD>(installer::TEMP_DIR_FAILED));
  }

  PathString compressed_archive;
  exit_code = UnpackBinaryResources(configuration, module, base_path.get(),
                                    &compressed_archive);

  // Create a temp folder where the archives are unpacked.
  base::FilePath unpack_path;
  installer::SelfCleaningTempDir temp_path;
  if (!CreateTemporaryAndUnpackDirectories(temp_path, &unpack_path))
    return ProcessExitResult(static_cast<DWORD>(installer::TEMP_DIR_FAILED));

  // Unpack the compressed archive to extract the uncompressed archive file.
  UnPackStatus unpack_status =
      UnPackArchive(base::FilePath(compressed_archive.get()), unpack_path,
                    /*output_file=*/nullptr);
  if (unpack_status != UNPACK_NO_ERROR)
    return ProcessExitResult(static_cast<DWORD>(installer::UNPACKING_FAILED));

  // Unpack the uncompressed archive to extract the updater files.
  base::FilePath uncompressed_archive =
      unpack_path.Append(FILE_PATH_LITERAL("updater.7z"));
  unpack_status =
      UnPackArchive(uncompressed_archive, unpack_path, /*output_file=*/nullptr);
  if (unpack_status != UNPACK_NO_ERROR)
    return ProcessExitResult(static_cast<DWORD>(installer::UNPACKING_FAILED));

  // While unpacking the binaries, we paged in a whole bunch of memory that
  // we don't need anymore.  Let's give it back to the pool before running
  // setup.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), static_cast<SIZE_T>(-1),
                             static_cast<SIZE_T>(-1));

  PathString setup_path;
  if (!setup_path.assign(unpack_path.value().c_str()) ||
      !setup_path.append(L"\\bin\\updater.exe")) {
    exit_code = ProcessExitResult(PATH_STRING_OVERFLOW);
  }

  if (exit_code.IsSuccess())
    exit_code = RunSetup(setup_path.get(), cmd_line_args.get());

  return exit_code;
}

}  // namespace updater
