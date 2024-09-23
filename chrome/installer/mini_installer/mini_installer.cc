// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// mini_installer.exe is the first exe that is run when chrome is being
// installed or upgraded. It is designed to be extremely small (~5KB with no
// extra resources linked) and it has two main jobs:
//   1) unpack the resources (possibly decompressing some)
//   2) run the real installer (setup.exe) with appropriate flags.
//
// In order to be really small the app doesn't link against the CRT and
// defines the following compiler/linker flags:
//   EnableIntrinsicFunctions="true" compiler: /Oi
//   BasicRuntimeChecks="0"
//   BufferSecurityCheck="false" compiler: /GS-
//   EntryPointSymbol="MainEntryPoint" linker: /ENTRY
//       /ENTRY also stops the CRT from being pulled in and does this more
//       precisely than /NODEFAULTLIB
//   OptimizeForWindows98="1" linker: /OPT:NOWIN98
//   linker: /SAFESEH:NO

#include "chrome/installer/mini_installer/mini_installer.h"

#include <windows.h>

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

#include <sddl.h>
#include <shellapi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <initializer_list>

#include "build/branding_buildflags.h"
#include "chrome/installer/mini_installer/appid.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "chrome/installer/mini_installer/decompress.h"
#include "chrome/installer/mini_installer/delete_with_retry.h"
#include "chrome/installer/mini_installer/enumerate_resources.h"
#include "chrome/installer/mini_installer/memory_range.h"
#include "chrome/installer/mini_installer/mini_file.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/regkey.h"
#include "chrome/installer/mini_installer/write_to_disk.h"

namespace mini_installer {

// Deletes |path|, updating |max_delete_attempts| if more attempts were taken
// than indicated in |max_delete_attempts|.
void DeleteWithRetryAndMetrics(const wchar_t* path, int& max_delete_attempts) {
  int attempts = 0;
  DeleteWithRetry(path, attempts);
  if (attempts > max_delete_attempts) {
    max_delete_attempts = attempts;
  }
}

// TODO(grt): Frame this in terms of whether or not the brand supports
// integration with Omaha, where Google Update is the Google-specific fork of
// the open-source Omaha project.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Opens the Google Update ClientState key for the current install mode.
bool OpenInstallStateKey(const Configuration& configuration, RegKey* key) {
  const HKEY root_key =
      configuration.is_system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const wchar_t* app_guid = configuration.chrome_app_guid();
  const REGSAM key_access = KEY_QUERY_VALUE | KEY_SET_VALUE;

  return OpenClientStateKey(root_key, app_guid, key_access, key) ==
         ERROR_SUCCESS;
}

// Writes install results into the registry where it is read by Google Update.
// Don't write anything if there is already a result present, likely
// written by setup.exe.
void WriteInstallResults(const Configuration& configuration,
                         ProcessExitResult result) {
  // Calls to setup.exe will write a "success" result if everything was good
  // so we don't need to write anything from here.
  if (result.IsSuccess()) {
    return;
  }

  // Write the value in Chrome ClientState key.
  RegKey key;
  DWORD value;
  if (OpenInstallStateKey(configuration, &key)) {
    if (key.ReadDWValue(kInstallerResultRegistryValue, &value) !=
            ERROR_SUCCESS ||
        value == 0) {
      key.WriteDWValue(kInstallerResultRegistryValue,
                       result.exit_code ? 1 /* FAILED_CUSTOM_ERROR */
                                        : 0 /* SUCCESS */);
      key.WriteDWValue(kInstallerErrorRegistryValue, result.exit_code);
      key.WriteDWValue(kInstallerExtraCode1RegistryValue, result.windows_error);
    }
  }
}

// Success metric reporting ----------------------------------------------------

// A single DWORD value may be written to the ExtraCode1 registry value on
// success. This is used to report a sample for a metric of a specific category.

// Categories of metrics written into ExtraCode1 on success. Values should not
// be reordered or reused unless the population reporting such categories
// becomes insiginficant or is filtered out based on release version.
enum MetricCategory : uint16_t {
  // The sample 0 indicates that %TMP% was used to hold the work dir. Active
  // from release 86.0.4237.0 through 88.0.4313.0.
  // kTemporaryDirectoryWithFallback = 1,

  // The sample 0 indicates that CWD was used to hold the work dir. Active from
  // release 86.0.4237.0 through 88.0.4313.0.
  // kTemporaryDirectoryWithoutFallback = 2,

  // Values indicate the maximum number of retries needed to delete a file or
  // directory via DeleteWithRetry. Active from release 88.0.4314.0.
  kMaxDeleteRetryCount = 3,
};

using MetricSample = uint16_t;

// Returns an ExtraCode1 value encoding a sample for a particular category.
constexpr DWORD MetricToExtraCode1(MetricCategory category,
                                   MetricSample sample) {
  return category << 16 | sample;
}

// Writes the value |extra_code_1| into ExtraCode1 for reporting by Omaha.
void WriteExtraCode1(const Configuration& configuration, DWORD extra_code_1) {
  // Write the value in Chrome ClientState key.
  RegKey key;
  if (OpenInstallStateKey(configuration, &key)) {
    key.WriteDWValue(kInstallerExtraCode1RegistryValue, extra_code_1);
  }
}

// This function sets the flag in registry to indicate that Google Update
// should try full installer next time. If the current installer works, this
// flag is cleared by setup.exe at the end of install.
void SetInstallerFlags(const Configuration& configuration) {
  StackString<128> value;

  RegKey key;
  if (!OpenInstallStateKey(configuration, &key)) {
    return;
  }

  // TODO(grt): Trim legacy modifiers (chrome,chromeframe,apphost,applauncher,
  // multi,readymode,stage,migrating,multifail) from the ap value.

  LONG ret = key.ReadSZValue(kApRegistryValue, value.get(), value.capacity());

  // The conditions below are handling two cases:
  // 1. When ap value is present, we want to add the required tag only if it
  //    is not present.
  // 2. When ap value is missing, we are going to create it with the required
  //    tag.
  if ((ret == ERROR_SUCCESS) || (ret == ERROR_FILE_NOT_FOUND)) {
    if (ret == ERROR_FILE_NOT_FOUND) {
      value.clear();
    }

    if (!StrEndsWith(value.get(), kFullInstallerSuffix) &&
        value.append(kFullInstallerSuffix)) {
      key.WriteSZValue(kApRegistryValue, value.get());
    }
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Gets the setup.exe path from Registry by looking at the value of Uninstall
// string.  |size| is measured in wchar_t units.
ProcessExitResult GetSetupExePathForAppGuid(bool system_level,
                                            const wchar_t* app_guid,
                                            const wchar_t* previous_version,
                                            wchar_t* path,
                                            size_t size) {
  const HKEY root_key = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey key;
  LONG result = OpenClientStateKey(root_key, app_guid, KEY_QUERY_VALUE, &key);
  if (result == ERROR_SUCCESS) {
    result = key.ReadSZValue(kUninstallRegistryValue, path, size);
  }
  if (result != ERROR_SUCCESS) {
    return ProcessExitResult(UNABLE_TO_FIND_REGISTRY_KEY, result);
  }

  // Check that the path to the existing installer includes the expected
  // version number.  It's not necessary for accuracy to verify before/after
  // delimiters.
  if (!SearchStringI(path, previous_version)) {
    return ProcessExitResult(PATCH_NOT_FOR_INSTALLED_VERSION);
  }

  // Strip double-quotes surrounding the string, if present.
  if (size >= 1 && path[0] == '\"') {
    size_t path_length = SafeStrLen(path, size);
    if (path_length >= 2 && path[path_length - 1] == '\"') {
      if (!SafeStrCopy(path, size, path + 1)) {
        return ProcessExitResult(PATH_STRING_OVERFLOW);
      }
      path[path_length - 2] = '\0';
    }
  }

  return ProcessExitResult(SUCCESS_EXIT_CODE);
}

// Gets the path to setup.exe of the previous version. The overall path is found
// in the Uninstall string in the registry. A previous version number specified
// in |configuration| is used if available. |size| is measured in wchar_t units.
ProcessExitResult GetPreviousSetupExePath(const Configuration& configuration,
                                          wchar_t* path,
                                          size_t size) {
  // Check Chrome's ClientState key for the path to setup.exe. This will have
  // the correct path for all well-functioning installs.
  return GetSetupExePathForAppGuid(
      configuration.is_system_level(), configuration.chrome_app_guid(),
      configuration.previous_version(), path, size);
}

// Calls CreateProcess with good default parameters and waits for the process to
// terminate returning the process exit code. In case of CreateProcess failure,
// returns a results object with the provided codes as follows:
// - ERROR_FILE_NOT_FOUND: (file_not_found_code, attributes of setup.exe).
// - ERROR_PATH_NOT_FOUND: (path_not_found_code, attributes of setup.exe).
// - Otherwise: (generic_failure_code, CreateProcess error code).
// In case of error waiting for the process to exit, returns a results object
// with (WAIT_FOR_PROCESS_FAILED, last error code). Otherwise, returns a results
// object with the subprocess's exit code.
ProcessExitResult RunProcessAndWait(const wchar_t* exe_path,
                                    wchar_t* cmdline,
                                    DWORD file_not_found_code,
                                    DWORD path_not_found_code,
                                    DWORD generic_failure_code) {
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(exe_path, cmdline, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    // Split specific failure modes. If setup.exe couldn't be launched because
    // its file/path couldn't be found, report its attributes in ExtraCode1.
    // This will help diagnose the prevalence of launch failures due to Image
    // File Execution Options tampering. See https://crbug.com/672813 for more
    // details.
    const DWORD last_error = ::GetLastError();
    const DWORD attributes = ::GetFileAttributes(exe_path);
    switch (last_error) {
      case ERROR_FILE_NOT_FOUND:
        return ProcessExitResult(file_not_found_code, attributes);
      case ERROR_PATH_NOT_FOUND:
        return ProcessExitResult(path_not_found_code, attributes);
      default:
        break;
    }
    // Lump all other errors into a distinct failure bucket.
    return ProcessExitResult(generic_failure_code, last_error);
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

void AppendCommandLineFlags(const wchar_t* command_line,
                            CommandString* buffer) {
  // The program name (the first argument parsed by CommandLineToArgvW) is
  // delimited by whitespace or a double quote based on the first character of
  // the full command line string. Use the same logic here to scan past the
  // program name in the program's command line (obtained during startup from
  // GetCommandLine). See
  // http://www.windowsinspired.com/how-a-windows-programs-splits-its-command-line-into-individual-arguments/
  // for gory details regarding how CommandLineToArgvW works.
  wchar_t a_char = 0;
  if (*command_line == L'"') {
    // Scan forward past the closing double quote.
    ++command_line;
    while (true) {
      a_char = *command_line;
      if (!a_char) {
        break;
      }
      ++command_line;
      if (a_char == L'"') {
        a_char = *command_line;
        break;
      }
    }  // postcondition: |a_char| contains the character at *command_line.
  } else {
    // Scan forward for the first space or tab character.
    while (true) {
      a_char = *command_line;
      if (!a_char || a_char == L' ' || a_char == L'\t') {
        break;
      }
      ++command_line;
    }  // postcondition: |a_char| contains the character at *command_line.
  }

  if (!a_char) {
    return;
  }

  // Append a space if |command_line| doesn't begin with one.
  if (a_char != ' ' && a_char != '\t' && !buffer->append(L" ")) {
    return;
  }
  buffer->append(command_line);
}

namespace {

// A ResourceEnumeratorDelegate that captures the resource name and data range
// for the chrome 7zip archive and the setup.
class ChromeResourceDelegate : public ResourceEnumeratorDelegate {
 public:
  ChromeResourceDelegate(PathString& archive_name,
                         MemoryRange& archive_range,
                         PathString& setup_name,
                         MemoryRange& setup_range,
                         DWORD& error_code)
      : archive_name_(archive_name),
        archive_range_(archive_range),
        setup_name_(setup_name),
        setup_range_(setup_range),
        error_code_(error_code) {}
  bool OnResource(const wchar_t* name, const MemoryRange& data_range) override;

 private:
  PathString& archive_name_;
  MemoryRange& archive_range_;
  PathString& setup_name_;
  MemoryRange& setup_range_;
  DWORD& error_code_;
};

// Returns false to stop enumeration on unexpected resource names, duplicate
// archive resources, or string overflow.
bool ChromeResourceDelegate::OnResource(const wchar_t* name,
                                        const MemoryRange& data_range) {
  if (StrStartsWith(name, kChromeArchivePrefix)) {
    if (!archive_range_.empty()) {
      error_code_ = ERROR_TOO_MANY_NAMES;
      return false;  // Break: duplicate resource name.
    }
    if (!archive_name_.assign(name)) {
      error_code_ = ERROR_FILENAME_EXCED_RANGE;
      return false;  // Break: resource name is too long.
    }
    archive_range_ = data_range;
  } else if (StrStartsWith(name, kSetupPrefix)) {
    if (!setup_range_.empty()) {
      error_code_ = ERROR_TOO_MANY_NAMES;
      return false;  // Break: duplicate resource name.
    }
    if (!setup_name_.assign(name)) {
      error_code_ = ERROR_FILENAME_EXCED_RANGE;
      return false;  // Break: resource name is too long.
    }
    setup_range_ = data_range;
  } else {
    error_code_ = ERROR_INVALID_DATA;
    return false;  // Break: unexpected resource name.
  }
  return true;  // Continue: advance to the next resource.
}

#if defined(COMPONENT_BUILD)
// A ResourceEnumeratorDelegate that writes all resources to disk in a given
// directory (which must end with a path separator).
class ResourceWriterDelegate : public ResourceEnumeratorDelegate {
 public:
  explicit ResourceWriterDelegate(const wchar_t* base_path)
      : base_path_(base_path) {}
  bool OnResource(const wchar_t* name, const MemoryRange& data_range) override;

 private:
  const wchar_t* const base_path_;
};

bool ResourceWriterDelegate::OnResource(const wchar_t* name,
                                        const MemoryRange& data_range) {
  PathString full_path;
  return (!data_range.empty() && full_path.assign(base_path_) &&
          full_path.append(name) && WriteToDisk(data_range, full_path.get()));
}

// A ResourceEnumeratorDelegate that deletes the file corresponding to each
// resource from a given directory (which must end with a path separator).
class ResourceDeleterDelegate : public ResourceEnumeratorDelegate {
 public:
  explicit ResourceDeleterDelegate(const wchar_t* base_path)
      : base_path_(base_path) {}
  bool OnResource(const wchar_t* name, const MemoryRange& data_range) override;

 private:
  const wchar_t* const base_path_;
};

bool ResourceDeleterDelegate::OnResource(const wchar_t* name,
                                         const MemoryRange& data_range) {
  PathString full_path;
  if (full_path.assign(base_path_) && full_path.append(name)) {
    // Do not record metrics for these deletes, as they are not done for release
    // builds.
    int attempts;
    DeleteWithRetry(full_path.get(), attempts);
  }

  return true;  // Continue enumeration.
}
#endif  // defined(COMPONENT_BUILD)

// Applies an differential update to the previous setup.exe provided by
// `patch_path` and produces a new setup.exe at the path `target_path`.
ProcessExitResult PatchSetup(const Configuration& configuration,
                             const PathString& patch_path,
                             const PathString& dest_path,
                             int& max_delete_attempts) {
  CommandString cmd_line;
  PathString exe_path;
  ProcessExitResult exit_code = GetPreviousSetupExePath(
      configuration, exe_path.get(), exe_path.capacity());
  if (!exit_code.IsSuccess()) {
    return exit_code;
  }

  if (!cmd_line.append(L"\"") || !cmd_line.append(exe_path.get()) ||
      !cmd_line.append(L"\" --") || !cmd_line.append(kCmdUpdateSetupExe) ||
      !cmd_line.append(L"=\"") || !cmd_line.append(patch_path.get()) ||
      !cmd_line.append(L"\" --") || !cmd_line.append(kCmdNewSetupExe) ||
      !cmd_line.append(L"=\"") || !cmd_line.append(dest_path.get()) ||
      !cmd_line.append(L"\"")) {
    exit_code = ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  if (!exit_code.IsSuccess()) {
    return exit_code;
  }

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe.
  AppendCommandLineFlags(configuration.command_line(), &cmd_line);

  exit_code = RunProcessAndWait(exe_path.get(), cmd_line.get(),
                                SETUP_PATCH_FAILED_FILE_NOT_FOUND,
                                SETUP_PATCH_FAILED_PATH_NOT_FOUND,
                                SETUP_PATCH_FAILED_COULD_NOT_CREATE_PROCESS);
  DeleteWithRetryAndMetrics(patch_path.get(), max_delete_attempts);

  return exit_code;
}

}  // namespace

ProcessExitResult UnpackBinaryResources(HMODULE module,
                                        const wchar_t* base_path,
                                        PathString& setup_path,
                                        ResourceTypeString& setup_type,
                                        PathString& archive_path,
                                        ResourceTypeString& archive_type,
                                        int& max_delete_attempts) {
  // Generate the setup.exe path where we patch/uncompress setup resource.
  PathString setup_name;
  MemoryRange setup_range;
  PathString archive_name;
  MemoryRange archive_range;

  // Scan through all types of resources looking for the chrome archive (which
  // is expected to be either a B7 chrome.packed.7z or a BN chrome.7z) and
  // installer (which is expected to be a B7 setup_patch.packed.7z, a BL
  // setup.ex_, or a BN setup.exe).
  for (const auto* type :
       {kLZMAResourceType, kLZCResourceType, kBinResourceType}) {
    DWORD error_code = ERROR_SUCCESS;
    // We ignore the result of EnumerateResources here because a non-success
    // does not always indicate an error occurred.
    EnumerateResources(
        ChromeResourceDelegate(archive_name, archive_range, setup_name,
                               setup_range, error_code),
        module, type);
    // `error_code` will have been modified by the delegate in case of error.
    if (error_code != ERROR_SUCCESS) {
      return ProcessExitResult(archive_type.empty()
                                   ? UNABLE_TO_EXTRACT_CHROME_ARCHIVE
                                   : UNABLE_TO_EXTRACT_SETUP_EXE,
                               error_code);
    }
    // If this iteration found either resource, remember its type.
    if (archive_type.empty() && !archive_range.empty()) {
      if (!archive_type.assign(type)) {
        return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP,
                                 UNABLE_TO_EXTRACT_CHROME_ARCHIVE);
      }
    }
    if (setup_type.empty() && !setup_range.empty()) {
      if (!setup_type.assign(type)) {
        return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP, ERROR_INCORRECT_SIZE);
      }
    }
    // Keep searching even if both were found so that an ChromeResourceDelegate
    // will propagate an error from `EnumerateResources` in case of duplicate
    // resources.
  }
  if (archive_range.empty()) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_CHROME_ARCHIVE,
                             ERROR_FILE_NOT_FOUND);
  }
  if (setup_range.empty()) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP_EXE, ERROR_FILE_NOT_FOUND);
  }

  // Write the archive to disk.
  if (!archive_path.assign(base_path) ||
      !archive_path.append(archive_name.get())) {
    return ProcessExitResult(PATH_STRING_OVERFLOW);
  }
  if (!WriteToDisk(archive_range, archive_path.get())) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_CHROME_ARCHIVE,
                             ::GetLastError());
  }

  // Extract directly to "setup.exe" if the resource is not compressed.
  if (!setup_path.assign(base_path) ||
      !setup_path.append(setup_type.compare(kBinResourceType) == 0
                             ? kSetupExe
                             : setup_name.get())) {
    return ProcessExitResult(PATH_STRING_OVERFLOW);
  }

  // Write the setup binary, possibly compressed, to disk.
  if (!WriteToDisk(setup_range, setup_path.get())) {
    return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP, ::GetLastError());
  }

  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  if (setup_type.compare(kLZCResourceType) == 0) {
    PathString setup_dest_path;
    if (!setup_dest_path.assign(base_path) ||
        !setup_dest_path.append(kSetupExe)) {
      return ProcessExitResult(PATH_STRING_OVERFLOW);
    }
    bool success =
        mini_installer::Expand(setup_path.get(), setup_dest_path.get());
    DeleteWithRetryAndMetrics(setup_path.get(), max_delete_attempts);

    if (!success) {
      exit_code = ProcessExitResult(UNABLE_TO_EXTRACT_SETUP_EXE);
    }
    setup_path.assign(setup_dest_path);
  }

#if defined(COMPONENT_BUILD)
  if (exit_code.IsSuccess()) {
    // Extract the modules in component build required by setup.exe.
    if (!EnumerateResources(ResourceWriterDelegate(base_path), module,
                            kDepResourceType)) {
      return ProcessExitResult(UNABLE_TO_EXTRACT_SETUP, ::GetLastError());
    }
  }
#endif  // defined(COMPONENT_BUILD)

  return exit_code;
}

// Executes setup.exe, waits for it to finish and returns the exit code.
ProcessExitResult RunSetup(const Configuration& configuration,
                           const wchar_t* archive_path,
                           const wchar_t* setup_path,
                           bool compressed_archive) {
  // Get the path to setup.exe.
  PathString setup_exe;

  if (*setup_path != L'\0') {
    if (!setup_exe.assign(setup_path)) {
      return ProcessExitResult(COMMAND_STRING_OVERFLOW);
    }
  } else {
    ProcessExitResult exit_code = GetPreviousSetupExePath(
        configuration, setup_exe.get(), setup_exe.capacity());
    if (!exit_code.IsSuccess()) {
      return exit_code;
    }
  }

  // There could be three full paths in the command line for setup.exe (path
  // to exe itself, path to archive and path to log file), so we declare
  // total size as three + one additional to hold command line options.
  CommandString cmd_line;
  // Put the quoted path to setup.exe in cmd_line first.
  if (!cmd_line.assign(L"\"") || !cmd_line.append(setup_exe.get()) ||
      !cmd_line.append(L"\"")) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  // Append the command line param for chrome archive file.
  const wchar_t* const archive_switch =
      compressed_archive ? kCmdInstallArchive : kCmdUncompressedArchive;
  if (!cmd_line.append(L" --") || !cmd_line.append(archive_switch) ||
      !cmd_line.append(L"=\"") || !cmd_line.append(archive_path) ||
      !cmd_line.append(L"\"")) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  // Append the command line param for the previous version of Chrome.
  if (configuration.previous_version() &&
      (!cmd_line.append(L" --") || !cmd_line.append(kCmdPreviousVersion) ||
       !cmd_line.append(L"=\"") ||
       !cmd_line.append(configuration.previous_version()) ||
       !cmd_line.append(L"\""))) {
    return ProcessExitResult(COMMAND_STRING_OVERFLOW);
  }

  // Get any command line option specified for mini_installer and pass them
  // on to setup.exe
  AppendCommandLineFlags(configuration.command_line(), &cmd_line);

  return RunProcessAndWait(setup_exe.get(), cmd_line.get(),
                           RUN_SETUP_FAILED_FILE_NOT_FOUND,
                           RUN_SETUP_FAILED_PATH_NOT_FOUND,
                           RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS);
}

// Deletes the files extracted by UnpackBinaryResources and the work directory
// created by GetWorkDir.
void DeleteExtractedFiles(HMODULE module,
                          const PathString& archive_path,
                          const PathString& setup_path,
                          const PathString& base_path,
                          int& max_delete_attempts) {
  if (!archive_path.empty()) {
    DeleteWithRetryAndMetrics(archive_path.get(), max_delete_attempts);
  }
  if (!setup_path.empty()) {
    DeleteWithRetryAndMetrics(setup_path.get(), max_delete_attempts);
  }

#if defined(COMPONENT_BUILD)
  // Delete the modules in a component build extracted for use by setup.exe.
  EnumerateResources(ResourceDeleterDelegate(base_path.get()), module,
                     kDepResourceType);
#endif  // defined(COMPONENT_BUILD)

  // Delete the temp dir (if it is empty, otherwise fail).
  DeleteWithRetryAndMetrics(base_path.get(), max_delete_attempts);
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
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
    return false;
  }

  DWORD size = 0;
  bool result = false;
  // We get the TokenOwner rather than the TokenUser because e.g. under UAC
  // elevation we want the admin to own the directory rather than the user.
  ::GetTokenInformation(token, TokenOwner, nullptr, 0, &size);
  if (size && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    if (TOKEN_OWNER* owner =
            reinterpret_cast<TOKEN_OWNER*>(::LocalAlloc(LPTR, size))) {
      if (::GetTokenInformation(token, TokenOwner, owner, size, &size)) {
        result = !!::ConvertSidToStringSid(owner->Owner, sid);
      }
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
  if (!IsAclSupportedForPath(path)) {
    return true;
  }

  wchar_t* sid = nullptr;
  if (!GetCurrentOwnerSid(&sid)) {
    return false;
  }

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

bool GetModuleDir(HMODULE module, PathString* directory) {
  DWORD len = ::GetModuleFileName(module, directory->get(),
                                  static_cast<DWORD>(directory->capacity()));
  if (!len || len >= directory->capacity()) {
    return false;  // Failed to get module path.
  }

  // Chop off the basename of the path.
  wchar_t* name = GetNameFromPathExt(directory->get(), len);
  if (name == directory->get()) {
    return false;  // No path separator found.
  }

  *name = L'\0';

  return true;
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
  if (!work_dir->assign(base_path) || !work_dir->append(kTempPrefix)) {
    return false;
  }

  // Store the location where we'll append the id.
  size_t end = work_dir->length();

  // Check if we'll have enough buffer space to continue.
  // The name of the directory will use up 11 chars and then we need to append
  // the trailing backslash and a terminator.  We've already added the prefix
  // to the buffer, so let's just make sure we've got enough space for the rest.
  if ((work_dir->capacity() - end) < (_countof("fffff.tmp") + 1)) {
    return false;
  }

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

  if (sa.lpSecurityDescriptor) {
    LocalFree(sa.lpSecurityDescriptor);
  }

  return exit_code->IsSuccess();
}

// Creates and returns a temporary directory in |work_dir| that can be used to
// extract mini_installer payload. |work_dir| ends with a path separator.
// Returns true if |work_dir| is available for use, or false in case of error
// (indicated by |exit_code|).
bool GetWorkDir(HMODULE module,
                PathString* work_dir,
                ProcessExitResult* exit_code) {
  PathString base_path;

  // Create a directory next to the current module.
  return GetModuleDir(module, &base_path) &&
         CreateWorkDir(base_path.get(), work_dir, exit_code);
}

ProcessExitResult WMain(HMODULE module) {
  ProcessExitResult exit_code = ProcessExitResult(SUCCESS_EXIT_CODE);

  // Parse configuration from the command line and resources.
  Configuration configuration;
  if (!configuration.Initialize(module)) {
    return ProcessExitResult(GENERIC_INITIALIZATION_FAILURE, ::GetLastError());
  }

  // Exit early if an invalid switch (e.g., "--chrome-frame") was found on the
  // command line.
  if (configuration.has_invalid_switch()) {
    return ProcessExitResult(INVALID_OPTION);
  }

  // First get a path where we can extract payload
  PathString base_path;
  if (!GetWorkDir(module, &base_path, &exit_code)) {
    return exit_code;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set the magic suffix in registry to try full installer next time. We ignore
  // any errors here and we try to set the suffix for user level unless
  // GoogleUpdateIsMachine=1 is present in the environment or --system-level is
  // on the command line in which case we set it for system level instead. This
  // only applies to the Google Chrome distribution.
  SetInstallerFlags(configuration);
#endif

  int max_delete_attempts = 0;
  PathString setup_path;
  ResourceTypeString setup_type;
  PathString archive_path;
  ResourceTypeString archive_type;

  exit_code =
      UnpackBinaryResources(module, base_path.get(), setup_path, setup_type,
                            archive_path, archive_type, max_delete_attempts);

  // If a compressed setup patch was found, run the previous setup.exe to
  // patch and generate the new setup.exe.
  if (exit_code.IsSuccess() && setup_type.compare(kLZMAResourceType) == 0) {
    PathString setup_dest_path;
    if (!setup_dest_path.assign(base_path.get()) ||
        !setup_dest_path.append(kSetupExe)) {
      return ProcessExitResult(PATH_STRING_OVERFLOW);
    }
    exit_code = PatchSetup(configuration, setup_path, setup_dest_path,
                           max_delete_attempts);
    if (exit_code.IsSuccess()) {
      setup_path.assign(setup_dest_path);
    } else {
      setup_path.clear();
    }
  }

  // While unpacking the binaries, we paged in a whole bunch of memory that
  // we don't need anymore.  Let's give it back to the pool before running
  // setup.
  ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

  if (exit_code.IsSuccess()) {
    exit_code = RunSetup(configuration, archive_path.get(), setup_path.get(),
                         archive_type.compare(kLZMAResourceType) == 0);
  }

  if (configuration.should_delete_extracted_files()) {
    DeleteExtractedFiles(module, archive_path, setup_path, base_path,
                         max_delete_attempts);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (exit_code.IsSuccess()) {
    // Send up a signal in ExtraCode1 upon successful install indicating the
    // maximum number of retries needed to delete a file or directory by
    // DeleteWithRetry; see https://crbug.com/1138157.
    MetricSample max_retries =
        (max_delete_attempts > 1 ? max_delete_attempts - 1 : 0);
    WriteExtraCode1(configuration,
                    MetricToExtraCode1(kMaxDeleteRetryCount, max_retries));
  } else {
    WriteInstallResults(configuration, exit_code);
  }
#endif

  return exit_code;
}

}  // namespace mini_installer
