// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_utils.h"

#include <wincred.h>  // For <ntsecapi.h>
#include <windows.h>
#include <winternl.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For LsaLookupAuthenticationPackage()
#include <sddl.h>      // For ConvertSidToStringSid()
#include <security.h>  // For NEGOSSP_NAME_A
#include <wbemidl.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include <iomanip>
#include <memory>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

const wchar_t kDefaultProfilePictureFileExtension[] = L".jpg";

namespace {

// Minimum supported version of Chrome for GCPW.
constexpr char kMinimumSupportedChromeVersionStr[] = "77.0.3865.65";

constexpr char kSentinelFilename[] = "gcpw_startup.sentinel";
constexpr base::FilePath::CharType kCredentialProviderFolder[] =
    L"Credential Provider";
constexpr int64_t kMaxConsecutiveCrashCount = 5;

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

base::FilePath GetStartupSentinelLocation() {
  base::FilePath sentienal_path;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &sentienal_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_COMMON_APP_DATA) hr=" << putHR(hr);
    return base::FilePath();
  }

  sentienal_path = sentienal_path.Append(GetInstallParentDirectoryName())
                       .Append(kCredentialProviderFolder);

  return sentienal_path.Append(TEXT(CHROME_VERSION_STRING))
      .AppendASCII(kSentinelFilename);
}

const base::win::i18n::LanguageSelector& GetLanguageSelector() {
  static base::NoDestructor<base::win::i18n::LanguageSelector> instance(
      base::string16(), kLanguageOffsetPairs);
  return *instance;
}

// Opens |path| with options that prevent the file from being read or written
// via another handle. As long as the returned object is alive, it is guaranteed
// that |path| isn't in use. It can however be deleted.
base::File GetFileLock(const base::FilePath& path) {
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_EXCLUSIVE_READ |
                              base::File::FLAG_EXCLUSIVE_WRITE |
                              base::File::FLAG_SHARE_DELETE);
}

// Deletes a specific GCP version from the disk.
void DeleteVersionDirectory(const base::FilePath& version_path) {
  // Lock all exes and dlls for exclusive access while allowing deletes.  Mark
  // the files for deletion and release them, causing them to actually be
  // deleted.  This allows the deletion of the version path itself.
  std::vector<base::File> locks;
  const int types = base::FileEnumerator::FILES;
  base::FileEnumerator enumerator_version(version_path, false, types,
                                          FILE_PATH_LITERAL("*"));
  bool all_deletes_succeeded = true;
  for (base::FilePath path = enumerator_version.Next(); !path.empty();
       path = enumerator_version.Next()) {
    if (!path.MatchesExtension(FILE_PATH_LITERAL(".exe")) &&
        !path.MatchesExtension(FILE_PATH_LITERAL(".dll"))) {
      continue;
    }

    // Open the file for exclusive access while allowing deletes.
    locks.push_back(GetFileLock(path));
    if (!locks.back().IsValid()) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "Could not lock " << path << " hr=" << putHR(hr);
      all_deletes_succeeded = false;
      continue;
    }

    // Mark the file for deletion.
    HRESULT hr = base::DeleteFile(path, false);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Could not delete " << path;
      all_deletes_succeeded = false;
    }
  }

  // Release the locks, actually deleting the files.  It is now possible to
  // delete the version path.
  locks.clear();
  if (all_deletes_succeeded && !base::DeleteFile(version_path, true))
    LOGFN(ERROR) << "Could not delete version " << version_path.BaseName();
}

}  // namespace

base::FilePath GetInstallDirectory() {
  base::FilePath dest_path;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES, &dest_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_PROGRAM_FILES) hr=" << putHR(hr);
    return base::FilePath();
  }

  dest_path = dest_path.Append(GetInstallParentDirectoryName())
                  .Append(kCredentialProviderFolder);

  return dest_path;
}

void DeleteVersionsExcept(const base::FilePath& gcp_path,
                          const base::string16& product_version) {
  base::FilePath version = base::FilePath(product_version);
  const int types = base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator enumerator(gcp_path, false, types,
                                  FILE_PATH_LITERAL("*"));
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    base::FilePath basename = name.BaseName();
    if (version == basename)
      continue;

    // Found an older version on the machine that can be deleted.  This is
    // best effort only.  If any errors occurred they are logged by
    // DeleteVersionDirectory().
    DeleteVersionDirectory(gcp_path.Append(basename));
  }
}

// StdParentHandles ///////////////////////////////////////////////////////////

StdParentHandles::StdParentHandles() {}

StdParentHandles::~StdParentHandles() {}

// ScopedStartupInfo //////////////////////////////////////////////////////////

ScopedStartupInfo::ScopedStartupInfo() {
  memset(&info_, 0, sizeof(info_));
  info_.hStdInput = INVALID_HANDLE_VALUE;
  info_.hStdOutput = INVALID_HANDLE_VALUE;
  info_.hStdError = INVALID_HANDLE_VALUE;
  info_.cb = sizeof(info_);
}

ScopedStartupInfo::ScopedStartupInfo(const wchar_t* desktop)
    : ScopedStartupInfo() {
  DCHECK(desktop);
  desktop_.assign(desktop);
  info_.lpDesktop = const_cast<wchar_t*>(desktop_.c_str());
}

ScopedStartupInfo::~ScopedStartupInfo() {
  Shutdown();
}

HRESULT ScopedStartupInfo::SetStdHandles(base::win::ScopedHandle* hstdin,
                                         base::win::ScopedHandle* hstdout,
                                         base::win::ScopedHandle* hstderr) {
  if ((info_.dwFlags & STARTF_USESTDHANDLES) == STARTF_USESTDHANDLES) {
    LOGFN(ERROR) << "Already set";
    return E_UNEXPECTED;
  }

  // CreateProcessWithTokenW will fail if any of the std handles provided are
  // invalid and the STARTF_USESTDHANDLES flag is set. So supply the default
  // standard handle if no handle is given for some of the handles. This tells
  // the process it can create its own local handles for these pipes as needed.
  info_.dwFlags |= STARTF_USESTDHANDLES;
  if (hstdin && hstdin->IsValid()) {
    info_.hStdInput = hstdin->Take();
  } else {
    info_.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
  }
  if (hstdout && hstdout->IsValid()) {
    info_.hStdOutput = hstdout->Take();
  } else {
    info_.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
  }
  if (hstderr && hstderr->IsValid()) {
    info_.hStdError = hstderr->Take();
  } else {
    info_.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
  }

  return S_OK;
}

void ScopedStartupInfo::Shutdown() {
  if ((info_.dwFlags & STARTF_USESTDHANDLES) == STARTF_USESTDHANDLES) {
    info_.dwFlags &= ~STARTF_USESTDHANDLES;

    if (info_.hStdInput != ::GetStdHandle(STD_INPUT_HANDLE))
      ::CloseHandle(info_.hStdInput);
    if (info_.hStdOutput != ::GetStdHandle(STD_OUTPUT_HANDLE))
      ::CloseHandle(info_.hStdOutput);
    if (info_.hStdError != ::GetStdHandle(STD_ERROR_HANDLE))
      ::CloseHandle(info_.hStdError);
    info_.hStdInput = INVALID_HANDLE_VALUE;
    info_.hStdOutput = INVALID_HANDLE_VALUE;
    info_.hStdError = INVALID_HANDLE_VALUE;
  }
}

// Waits for a process to terminate while capturing output from |output_handle|
// to the buffer |output_buffer| of length |buffer_size|. The buffer is expected
// to be relatively small.  The exit code of the process is written to
// |exit_code|.
HRESULT WaitForProcess(base::win::ScopedHandle::Handle process_handle,
                       const StdParentHandles& parent_handles,
                       DWORD* exit_code,
                       char* output_buffer,
                       int buffer_size) {
  LOGFN(INFO);
  DCHECK(exit_code);
  DCHECK_GT(buffer_size, 0);

  output_buffer[0] = 0;

  HANDLE output_handle = parent_handles.hstdout_read.Get();

  for (bool is_done = false; !is_done;) {
    char buffer[80];
    DWORD length = base::size(buffer) - 1;
    HRESULT hr = S_OK;

    const DWORD kThreeMinutesInMs = 3 * 60 * 1000;
    DWORD ret = ::WaitForSingleObject(output_handle,
                                      kThreeMinutesInMs);  // timeout ms
    switch (ret) {
      case WAIT_OBJECT_0: {
        int index = ret - WAIT_OBJECT_0;
        LOGFN(INFO) << "WAIT_OBJECT_" << index;
        if (!::ReadFile(output_handle, buffer, length, &length, nullptr)) {
          hr = HRESULT_FROM_WIN32(::GetLastError());
          if (hr != HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE))
            LOGFN(ERROR) << "ReadFile(" << index << ") hr=" << putHR(hr);
        } else {
          LOGFN(INFO) << "ReadFile(" << index << ") length=" << length;
          buffer[length] = 0;
        }
        break;
      }
      case WAIT_IO_COMPLETION:
        // This is normal.  Just ignore.
        LOGFN(INFO) << "WaitForMultipleObjectsEx WAIT_IO_COMPLETION";
        break;
      case WAIT_TIMEOUT: {
        // User took too long to log in, so kill UI process.
        LOGFN(INFO) << "WaitForMultipleObjectsEx WAIT_TIMEOUT, killing UI";
        ::TerminateProcess(process_handle, kUiecTimeout);
        is_done = true;
        break;
      }
      case WAIT_FAILED:
      default: {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "WaitForMultipleObjectsEx hr=" << putHR(hr);
        is_done = true;
        break;
      }
    }

    // Copy the read buffer to the output buffer. If the pipe was broken,
    // we can break our loop and wait for the process to die.
    if (hr == HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE)) {
      LOGFN(INFO) << "Stop waiting for output buffer";
      break;
    } else {
      strcat_s(output_buffer, buffer_size, buffer);
    }
  }

  // At this point both stdout and stderr have been closed.  Wait on the process
  // handle for the process to terminate, getting the exit code.  If the
  // process does not terminate gracefully, kill it before returning.
  DWORD ret = ::WaitForSingleObject(process_handle, 10000);
  if (ret == 0) {
    if (::GetExitCodeProcess(process_handle, exit_code)) {
      LOGFN(INFO) << "Process terminated with exit code " << *exit_code;
    } else {
      LOGFN(INFO) << "Process terminated without exit code";
      *exit_code = kUiecAbort;
    }
  } else {
    LOGFN(INFO) << "UI did not terminiate within 10 seconds, killing now";
    ::TerminateProcess(process_handle, kUiecKilled);
    *exit_code = kUiecKilled;
  }

  return S_OK;
}

HRESULT CreateLogonToken(const wchar_t* domain,
                         const wchar_t* username,
                         const wchar_t* password,
                         bool interactive,
                         base::win::ScopedHandle* token) {
  DCHECK(domain);
  DCHECK(username);
  DCHECK(password);
  DCHECK(token);

  DWORD logon_type =
      interactive ? LOGON32_LOGON_INTERACTIVE : LOGON32_LOGON_BATCH;
  base::win::ScopedHandle::Handle handle;

  if (!::LogonUserW(username, domain, password, logon_type,
                    LOGON32_PROVIDER_DEFAULT, &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "LogonUserW hr=" << putHR(hr);
    return hr;
  }
  base::win::ScopedHandle primary_token(handle);

  if (!::CreateRestrictedToken(primary_token.Get(), DISABLE_MAX_PRIVILEGE, 0,
                               nullptr, 0, nullptr, 0, nullptr, &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateRestrictedToken hr=" << putHR(hr);
    return hr;
  }
  token->Set(handle);
  return S_OK;
}

HRESULT CreateJobForSignin(base::win::ScopedHandle* job) {
  LOGFN(INFO);
  DCHECK(job);

  job->Set(::CreateJobObject(nullptr, nullptr));
  if (!job->IsValid()) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateJobObject hr=" << putHR(hr);
    return hr;
  }

  JOBOBJECT_BASIC_UI_RESTRICTIONS ui;
  ui.UIRestrictionsClass =
      JOB_OBJECT_UILIMIT_DESKTOP |           // Create/switch desktops.
      JOB_OBJECT_UILIMIT_HANDLES |           // Only access own handles.
      JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |  // Cannot set sys params.
      JOB_OBJECT_UILIMIT_WRITECLIPBOARD;     // Cannot write to clipboard.
  if (!::SetInformationJobObject(job->Get(), JobObjectBasicUIRestrictions, &ui,
                                 sizeof(ui))) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "SetInformationJobObject hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

HRESULT CreatePipeForChildProcess(bool child_reads,
                                  bool use_nul,
                                  base::win::ScopedHandle* reading,
                                  base::win::ScopedHandle* writing) {
  // Make sure that all handles created here are inheritable.  It is important
  // that the child side handle is inherited.
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = nullptr;

  if (use_nul) {
    base::win::ScopedHandle h(
        ::CreateFileW(L"nul:", FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                      FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                      &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!h.IsValid()) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreateFile(nul) hr=" << putHR(hr);
      return hr;
    }

    if (child_reads) {
      reading->Set(h.Take());
    } else {
      writing->Set(h.Take());
    }
  } else {
    base::win::ScopedHandle::Handle temp_handle1;
    base::win::ScopedHandle::Handle temp_handle2;
    if (!::CreatePipe(&temp_handle1, &temp_handle2, &sa, 0)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "CreatePipe(reading) hr=" << putHR(hr);
      return hr;
    }
    reading->Set(temp_handle1);
    writing->Set(temp_handle2);

    // Make sure parent side is not inherited.
    if (!::SetHandleInformation(child_reads ? writing->Get() : reading->Get(),
                                HANDLE_FLAG_INHERIT, 0)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "SetHandleInformation(parent) hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT InitializeStdHandles(CommDirection direction,
                             StdHandlesToCreate to_create,
                             ScopedStartupInfo* startupinfo,
                             StdParentHandles* parent_handles) {
  LOGFN(INFO);
  DCHECK(startupinfo);
  DCHECK(parent_handles);

  base::win::ScopedHandle hstdin_read;
  base::win::ScopedHandle hstdin_write;
  if ((to_create & kStdInput) != 0) {
    HRESULT hr = CreatePipeForChildProcess(
        true,                                            // child reads
        direction == CommDirection::kChildToParentOnly,  // use nul
        &hstdin_read, &hstdin_write);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "CreatePipeForChildProcess(stdin) hr=" << putHR(hr);
      return hr;
    }
  }

  base::win::ScopedHandle hstdout_read;
  base::win::ScopedHandle hstdout_write;
  if ((to_create & kStdOutput) != 0) {
    HRESULT hr = CreatePipeForChildProcess(
        false,                                           // child reads
        direction == CommDirection::kParentToChildOnly,  // use nul
        &hstdout_read, &hstdout_write);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "CreatePipeForChildProcess(stdout) hr=" << putHR(hr);
      return hr;
    }
  }

  base::win::ScopedHandle hstderr_read;
  base::win::ScopedHandle hstderr_write;
  if ((to_create & kStdError) != 0) {
    HRESULT hr = CreatePipeForChildProcess(
        false,                                           // child reads
        direction == CommDirection::kParentToChildOnly,  // use nul
        &hstderr_read, &hstderr_write);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "CreatePipeForChildProcess(stderr) hr=" << putHR(hr);
      return hr;
    }
  }

  HRESULT hr =
      startupinfo->SetStdHandles(&hstdin_read, &hstdout_write, &hstderr_write);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "startupinfo->SetStdHandles hr=" << putHR(hr);
    return hr;
  }

  parent_handles->hstdin_write.Set(hstdin_write.Take());
  parent_handles->hstdout_read.Set(hstdout_read.Take());
  parent_handles->hstderr_read.Set(hstderr_read.Take());
  return S_OK;
}

HRESULT GetPathToDllFromHandle(HINSTANCE dll_handle,
                               base::FilePath* path_to_dll) {
  wchar_t path[MAX_PATH];
  DWORD length = base::size(path);
  length = ::GetModuleFileName(dll_handle, path, length);
  if (length == 0 || length >= base::size(path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetModuleFileNameW hr=" << putHR(hr);
    return hr;
  }

  *path_to_dll = base::FilePath(base::StringPiece16(path, length));
  return S_OK;
}

HRESULT GetEntryPointArgumentForRunDll(HINSTANCE dll_handle,
                                       const wchar_t* entrypoint,
                                       base::string16* entrypoint_arg) {
  DCHECK(entrypoint);
  DCHECK(entrypoint_arg);

  entrypoint_arg->clear();

  // rundll32 expects the first command line argument to be the path to the
  // DLL, followed by a comma and the name of the function to call.  There can
  // be no spaces around the comma.  There can be no spaces in the path.  It
  // is recommended to use the short path name of the DLL.
  base::FilePath path_to_dll;
  HRESULT hr = GetPathToDllFromHandle(dll_handle, &path_to_dll);
  if (FAILED(hr))
    return hr;

  wchar_t short_path[MAX_PATH];
  DWORD short_length = base::size(short_path);
  short_length =
      ::GetShortPathName(path_to_dll.value().c_str(), short_path, short_length);
  if (short_length >= base::size(short_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetShortPathNameW hr=" << putHR(hr);
    return hr;
  }

  *entrypoint_arg =
      base::string16(base::StringPrintf(L"%ls,%ls", short_path, entrypoint));

  // In tests, the current module is the unittest exe, not the real dll.
  // The unittest exe does not expose entrypoints, so return S_FALSE as a hint
  // that this will not work.  The command line is built anyway though so
  // tests of the command line construction can be written.
  return wcsicmp(wcsrchr(path_to_dll.value().c_str(), L'.'), L".dll") == 0
             ? S_OK
             : S_FALSE;
}

HRESULT GetCommandLineForEntrypoint(HINSTANCE dll_handle,
                                    const wchar_t* entrypoint,
                                    base::CommandLine* command_line) {
  DCHECK(entrypoint);
  DCHECK(command_line);

  // Build the full path to rundll32.
  base::FilePath system_dir;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_dir))
    return HRESULT_FROM_WIN32(::GetLastError());

  command_line->SetProgram(
      system_dir.Append(FILE_PATH_LITERAL("rundll32.exe")));

  base::string16 entrypoint_arg;
  HRESULT hr =
      GetEntryPointArgumentForRunDll(dll_handle, entrypoint, &entrypoint_arg);
  if (SUCCEEDED(hr))
    command_line->AppendArgNative(entrypoint_arg);

  return hr;
}

HRESULT LookupLocalizedNameBySid(PSID sid, base::string16* localized_name) {
  DCHECK(localized_name);
  std::vector<wchar_t> localized_name_buffer;
  DWORD group_name_size = 0;
  std::vector<wchar_t> domain_buffer;
  DWORD domain_size = 0;
  SID_NAME_USE use;

  // Get the localized name of the local users group. The function
  // NetLocalGroupAddMembers only accepts the name of the group and it
  // may be localized on the system.
  if (!::LookupAccountSidW(nullptr, sid, nullptr, &group_name_size, nullptr,
                           &domain_size, &use)) {
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "LookupAccountSidW hr=" << putHR(hr);
      return hr;
    }

    localized_name_buffer.resize(group_name_size);
    domain_buffer.resize(domain_size);
    if (!::LookupAccountSidW(nullptr, sid, localized_name_buffer.data(),
                             &group_name_size, domain_buffer.data(),
                             &domain_size, &use)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "LookupAccountSidW hr=" << putHR(hr);
      return hr;
    }
  }

  if (localized_name_buffer.empty()) {
    LOGFN(ERROR) << "Empty localized name";
    return E_UNEXPECTED;
  }
  *localized_name = base::string16(localized_name_buffer.data(),
                                   localized_name_buffer.size() - 1);

  return S_OK;
}

HRESULT LookupLocalizedNameForWellKnownSid(WELL_KNOWN_SID_TYPE sid_type,
                                           base::string16* localized_name) {
  BYTE well_known_sid[SECURITY_MAX_SID_SIZE];
  DWORD size_local_users_group_sid = base::size(well_known_sid);

  // Get the sid for the well known local users group.
  if (!::CreateWellKnownSid(sid_type, nullptr, well_known_sid,
                            &size_local_users_group_sid)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateWellKnownSid hr=" << putHR(hr);
    return hr;
  }

  return LookupLocalizedNameBySid(well_known_sid, localized_name);
}

bool VerifyStartupSentinel() {
  // Always try to write to the startup sentinel file. If writing or opening
  // fails for any reason (file locked, no access etc) consider this a failure.
  // If no sentinel file path can be found this probably means that we are
  // running in a unit test so just let the verification pass in this case.
  base::FilePath startup_sentinel_path = GetStartupSentinelLocation();
  if (!startup_sentinel_path.empty()) {
    base::FilePath startup_sentinel_directory = startup_sentinel_path.DirName();
    if (!base::DirectoryExists(startup_sentinel_directory)) {
      base::File::Error error;
      if (!base::CreateDirectoryAndGetError(startup_sentinel_directory,
                                            &error)) {
        LOGFN(ERROR) << "Could not create sentinel directory='"
                     << startup_sentinel_directory << "' error=" << error;
        return false;
      }
    }
    base::File startup_sentinel(
        startup_sentinel_path,
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);

    // Keep writing to the sentinel file until we have reached
    // |kMaxConsecutiveCrashCount| at which point it is assumed that GCPW
    // is crashing continuously and should be disabled.
    if (!startup_sentinel.IsValid() ||
        startup_sentinel.GetLength() >= kMaxConsecutiveCrashCount) {
      return false;
    }

    return startup_sentinel.WriteAtCurrentPos("0", 1) == 1;
  }

  return true;
}

void DeleteStartupSentinel() {
  base::FilePath startup_sentinel_path = GetStartupSentinelLocation();
  if (base::PathExists(startup_sentinel_path) &&
      !base::DeleteFile(startup_sentinel_path, false)) {
    LOGFN(ERROR) << "Failed to delete sentinel file: " << startup_sentinel_path;
  }
}

base::string16 GetStringResource(int base_message_id) {
  base::string16 localized_string;

  int message_id = base_message_id + GetLanguageSelector().offset();
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    localized_string = base::string16(image->achString, image->nLength);
  } else {
    NOTREACHED() << "Unable to find resource id " << message_id;
  }

  return localized_string;
}

base::string16 GetSelectedLanguage() {
  return GetLanguageSelector().matched_candidate();
}

void SecurelyClearDictionaryValue(base::Optional<base::Value>* value) {
  SecurelyClearDictionaryValueWithKey(value, kKeyPassword);
}

void SecurelyClearDictionaryValueWithKey(base::Optional<base::Value>* value,
                                         const std::string& password_key) {
  if (!value || !(*value) || !((*value)->is_dict()))
    return;

  const std::string* password_value = (*value)->FindStringKey(password_key);
  if (password_value) {
    SecurelyClearString(*const_cast<std::string*>(password_value));
  }

  (*value).reset();
}

void SecurelyClearString(base::string16& str) {
  SecurelyClearBuffer(const_cast<wchar_t*>(str.data()),
                      str.size() * sizeof(decltype(str[0])));
}

void SecurelyClearString(std::string& str) {
  SecurelyClearBuffer(const_cast<char*>(str.data()), str.size());
}

void SecurelyClearBuffer(void* buffer, size_t length) {
  if (buffer)
    ::RtlSecureZeroMemory(buffer, length);
}

std::string SearchForKeyInStringDictUTF8(
    const std::string& json_string,
    const std::initializer_list<base::StringPiece>& path) {
  DCHECK(path.size() > 0);

  base::Optional<base::Value> json_obj =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json_obj || !json_obj->is_dict()) {
    LOGFN(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    return std::string();
  }
  const std::string* value =
      json_obj->FindStringPath(base::JoinString(path, "."));
  return value ? *value : std::string();
}

base::string16 GetDictString(const base::Value& dict, const char* name) {
  DCHECK(name);
  DCHECK(dict.is_dict());
  auto* value = dict.FindKey(name);
  return value && value->is_string() ? base::UTF8ToUTF16(value->GetString())
                                     : base::string16();
}

base::string16 GetDictString(const std::unique_ptr<base::Value>& dict,
                             const char* name) {
  return GetDictString(*dict, name);
}

std::string GetDictStringUTF8(const base::Value& dict, const char* name) {
  DCHECK(name);
  DCHECK(dict.is_dict());
  auto* value = dict.FindKey(name);
  return value && value->is_string() ? value->GetString() : std::string();
}

std::string GetDictStringUTF8(const std::unique_ptr<base::Value>& dict,
                              const char* name) {
  return GetDictStringUTF8(*dict, name);
}

base::FilePath::StringType GetInstallParentDirectoryName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return FILE_PATH_LITERAL("Google");
#else
  return FILE_PATH_LITERAL("Chromium");
#endif
}

base::string16 GetWindowsVersion() {
  wchar_t release_id[32];
  ULONG length = base::size(release_id);
  HRESULT hr =
      GetMachineRegString(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                          L"ReleaseId", release_id, &length);
  if (SUCCEEDED(hr))
    return release_id;

  return L"Unknown";
}

base::Version GetMinimumSupportedChromeVersion() {
  return base::Version(kMinimumSupportedChromeVersionStr);
}

FakesForTesting::FakesForTesting() {}

FakesForTesting::~FakesForTesting() {}

}  // namespace credential_provider
