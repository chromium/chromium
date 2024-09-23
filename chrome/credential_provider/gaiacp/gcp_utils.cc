// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/gcp_utils.h"

#include <windows.h>
#include <winsock2.h>

#include <iphlpapi.h>
#include <wincred.h>  // For <ntsecapi.h>
#include <winternl.h>

#include <string>
#include <string_view>

#include "base/values.h"

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <malloc.h>
#include <memory.h>
#include <ntsecapi.h>  // For LsaLookupAuthenticationPackage()
#include <sddl.h>      // For ConvertSidToStringSid()
#include <security.h>  // For NEGOSSP_NAME_A
#include <stdlib.h>
#include <wbemidl.h>

#include <algorithm>
#include <iomanip>
#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/current_module.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "base/win/win_util.h"
#include "base/win/wmi.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/token_generator.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/re2/src/re2/re2.h"

namespace credential_provider {

constexpr wchar_t kDefaultProfilePictureFileExtension[] = L".jpg";

constexpr base::FilePath::CharType kCredentialProviderFolder[] =
    L"Credential Provider";

constexpr wchar_t kDefaultMdmUrl[] =
    L"https://deviceenrollmentforwindows.googleapis.com/v1/discovery";

constexpr int kMaxNumConsecutiveUploadDeviceFailures = 3;

// The following staleness time limits are set to 5 days to prevent file fetch
// operations unnecessarily by GCPW when machine is offline during weekends and
// holidays. These files are also updated by GCPW extension Windows NT service
// regularly when the device is online.
constexpr base::TimeDelta kMaxTimeDeltaSinceLastUserPolicyRefresh =
    base::Days(5);
constexpr base::TimeDelta kMaxTimeDeltaSinceLastExperimentsFetch =
    base::Days(5);

constexpr wchar_t kGcpwExperimentsDirectory[] = L"Experiments";
constexpr wchar_t kGcpwUserExperimentsFileName[] = L"ExperimentsFetchResponse";

namespace {

// Overridden in tests to fake serial number extraction.
bool g_use_test_serial_number = false;
std::wstring& TestSerialNumber() {
  static base::NoDestructor<std::wstring> value;
  return *value;
}

// Overridden in tests to fake MAC address extraction.
bool g_use_test_mac_addresses = false;
std::vector<std::string>& TestMacAddresses() {
  static base::NoDestructor<std::vector<std::string>> value;
  return *value;
}

// Overridden in tests to fake OS version.
bool g_use_test_os_version = false;
std::string& TestOSVersion() {
  static base::NoDestructor<std::string> value;
  return *value;
}

// Overridden in tests to fake installed Chrome path.
bool g_use_test_chrome_path = false;
base::FilePath& TestChromePath() {
  static base::NoDestructor<base::FilePath> value;
  return *value;
}

constexpr wchar_t kKernelLibFile[] = L"kernel32.dll";
constexpr wchar_t kOsRegistryPath[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
constexpr wchar_t kOsMajorName[] = L"CurrentMajorVersionNumber";
constexpr wchar_t kOsMinorName[] = L"CurrentMinorVersionNumber";
constexpr wchar_t kOsBuildName[] = L"CurrentBuildNumber";
constexpr int kVersionStringSize = 128;

// Minimum supported version of Chrome for GCPW.
constexpr char kMinimumSupportedChromeVersionStr[] = "77.0.3865.65";

constexpr char kSentinelFilename[] = "gcpw_startup.sentinel";
constexpr int64_t kMaxConsecutiveCrashCount = 5;

// L$ prefix means this secret can only be accessed locally.
constexpr wchar_t kLsaKeyDMTokenPrefix[] = L"L$GCPW-DM-Token-";

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

base::FilePath GetStartupSentinelLocation(const std::wstring& version) {
  base::FilePath sentienal_path;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &sentienal_path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_COMMON_APP_DATA) hr=" << putHR(hr);
    return base::FilePath();
  }

  sentienal_path = sentienal_path.Append(GetInstallParentDirectoryName())
                       .Append(kCredentialProviderFolder);

  return sentienal_path.Append(version).AppendASCII(kSentinelFilename);
}

const base::win::i18n::LanguageSelector& GetLanguageSelector() {
  static base::NoDestructor<base::win::i18n::LanguageSelector> instance(
      std::wstring(), kLanguageOffsetPairs);
  return *instance;
}

// Opens |path| with options that prevent the file from being read or written
// via another handle. As long as the returned object is alive, it is guaranteed
// that |path| isn't in use. It can however be deleted.
base::File GetFileLock(const base::FilePath& path) {
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_READ |
                              base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                              base::File::FLAG_WIN_SHARE_DELETE);
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
    HRESULT hr = base::DeleteFile(path);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Could not delete " << path;
      all_deletes_succeeded = false;
    }
  }

  // Release the locks, actually deleting the files.  It is now possible to
  // delete the version path.
  locks.clear();
  if (all_deletes_succeeded && !base::DeletePathRecursively(version_path))
    LOGFN(ERROR) << "Could not delete version " << version_path.BaseName();
}

// Reads the dm token for |sid| from lsa store and writes into |token| output
// parameter. If |refresh| is true, token is re-generated before returning.
HRESULT GetGCPWDmTokenInternal(const std::wstring& sid,
                               std::wstring* token,
                               bool refresh) {
  DCHECK(token);

  std::wstring store_key = kLsaKeyDMTokenPrefix + sid;

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  if (!policy) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ScopedLsaPolicy::Create hr=" << putHR(hr);
    return hr;
  }

  if (refresh) {
    if (policy->PrivateDataExists(store_key.c_str())) {
      HRESULT hr = policy->RemovePrivateData(store_key.c_str());
      if (FAILED(hr)) {
        LOGFN(ERROR) << "ScopedLsaPolicy::RemovePrivateData hr=" << putHR(hr);
        return hr;
      }
    }

    std::wstring new_token =
        base::UTF8ToWide(TokenGenerator::Get()->GenerateToken());

    HRESULT hr = policy->StorePrivateData(store_key.c_str(), new_token.c_str());
    if (FAILED(hr)) {
      LOGFN(ERROR) << "ScopedLsaPolicy::StorePrivateData hr=" << putHR(hr);
      return hr;
    }

    *token = new_token;
  } else {
    wchar_t dm_token_lsa_data[1024];
    HRESULT hr = policy->RetrievePrivateData(
        store_key.c_str(), dm_token_lsa_data, std::size(dm_token_lsa_data));
    if (FAILED(hr)) {
      LOGFN(ERROR) << "ScopedLsaPolicy::RetrievePrivateData hr=" << putHR(hr);
      return hr;
    }

    *token = dm_token_lsa_data;
  }

  return S_OK;
}

// Get the path to the directory under DIR_COMMON_APP_DATA with the given |sid|
// and |file_dir|.
base::FilePath GetDirectoryFilePath(const std::wstring& sid,
                                    const std::wstring& file_dir) {
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_COMMON_APP_DATA, &path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "PathService::Get(DIR_COMMON_APP_DATA) hr=" << putHR(hr);
    return base::FilePath();
  }
  path = path.Append(GetInstallParentDirectoryName())
             .Append(kCredentialProviderFolder)
             .Append(file_dir)
             .Append(sid);
  return path;
}

}  // namespace

// GoogleRegistrationDataForTesting //////////////////////////////////////////

GoogleRegistrationDataForTesting::GoogleRegistrationDataForTesting(
    std::wstring serial_number) {
  g_use_test_serial_number = true;
  TestSerialNumber() = serial_number;
}

GoogleRegistrationDataForTesting::~GoogleRegistrationDataForTesting() {
  g_use_test_serial_number = false;
  TestSerialNumber() = L"";
}

// GoogleRegistrationDataForTesting //////////////////////////////////////////

// GemDeviceDetailsForTesting //////////////////////////////////////////

GemDeviceDetailsForTesting::GemDeviceDetailsForTesting(
    std::vector<std::string>& mac_addresses,
    std::string os_version) {
  g_use_test_mac_addresses = true;
  g_use_test_os_version = true;
  TestMacAddresses() = mac_addresses;
  TestOSVersion() = os_version;
}

GemDeviceDetailsForTesting::~GemDeviceDetailsForTesting() {
  g_use_test_mac_addresses = false;
  g_use_test_os_version = false;
}

// GemDeviceDetailsForTesting //////////////////////////////////////////

// GoogleChromePathForTesting ////////////////////////////////////////////////

GoogleChromePathForTesting::GoogleChromePathForTesting(
    base::FilePath file_path) {
  g_use_test_chrome_path = true;
  TestChromePath() = file_path;
}

GoogleChromePathForTesting::~GoogleChromePathForTesting() {
  g_use_test_chrome_path = false;
  TestChromePath() = base::FilePath(L"");
}

// GoogleChromePathForTesting /////////////////////////////////////////////////

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
                          const std::wstring& product_version) {
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
    DeleteStartupSentinelForVersion(basename.value());
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
  LOGFN(VERBOSE);
  DCHECK(exit_code);
  DCHECK_GT(buffer_size, 0);

  output_buffer[0] = 0;

  HANDLE output_handle = parent_handles.hstdout_read.Get();

  for (bool is_done = false; !is_done;) {
    char buffer[80];
    DWORD length = std::size(buffer) - 1;
    HRESULT hr = S_OK;

    const DWORD kThreeMinutesInMs = 3 * 60 * 1000;
    DWORD ret = ::WaitForSingleObject(output_handle,
                                      kThreeMinutesInMs);  // timeout ms
    switch (ret) {
      case WAIT_OBJECT_0: {
        int index = ret - WAIT_OBJECT_0;
        LOGFN(VERBOSE) << "WAIT_OBJECT_" << index;
        if (!::ReadFile(output_handle, buffer, length, &length, nullptr)) {
          hr = HRESULT_FROM_WIN32(::GetLastError());
          if (hr != HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE))
            LOGFN(ERROR) << "ReadFile(" << index << ") hr=" << putHR(hr);
        } else {
          LOGFN(VERBOSE) << "ReadFile(" << index << ") length=" << length;
          buffer[length] = 0;
        }
        break;
      }
      case WAIT_IO_COMPLETION:
        // This is normal.  Just ignore.
        LOGFN(VERBOSE) << "WaitForMultipleObjectsEx WAIT_IO_COMPLETION";
        break;
      case WAIT_TIMEOUT: {
        // User took too long to log in, so kill UI process.
        LOGFN(VERBOSE) << "WaitForMultipleObjectsEx WAIT_TIMEOUT, killing UI";
        ::TerminateProcess(process_handle, kUiecTimeout);
        is_done = true;
        break;
      }
      case WAIT_FAILED:
      default: {
        HRESULT last_error_hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "WaitForMultipleObjectsEx hr=" << putHR(last_error_hr);
        is_done = true;
        break;
      }
    }

    // Copy the read buffer to the output buffer. If the pipe was broken,
    // we can break our loop and wait for the process to die.
    if (hr == HRESULT_FROM_WIN32(ERROR_BROKEN_PIPE)) {
      LOGFN(VERBOSE) << "Stop waiting for output buffer";
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
      LOGFN(VERBOSE) << "Process terminated with exit code " << *exit_code;
    } else {
      LOGFN(WARNING) << "Process terminated without exit code";
      *exit_code = kUiecAbort;
    }
  } else {
    LOGFN(WARNING) << "UI did not terminiate within 10 seconds, killing now";
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
  LOGFN(VERBOSE);
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
  LOGFN(VERBOSE);
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
  DWORD length = std::size(path);
  length = ::GetModuleFileName(dll_handle, path, length);
  if (length == 0 || length >= std::size(path)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetModuleFileNameW hr=" << putHR(hr);
    return hr;
  }

  *path_to_dll = base::FilePath(std::wstring_view(path, length));
  return S_OK;
}

HRESULT GetEntryPointArgumentForRunDll(HINSTANCE dll_handle,
                                       const wchar_t* entrypoint,
                                       std::wstring* entrypoint_arg) {
  DCHECK(entrypoint);
  DCHECK(entrypoint_arg);

  entrypoint_arg->clear();

  // rundll32 expects the first command line argument to be the path to the
  // DLL, followed by a comma and the name of the function to call.  There can
  // be no spaces around the comma. The dll path is quoted because short names
  // may be disabled in the system and path can not have space otherwise. It is
  // recommended to use the short path name of the DLL.
  base::FilePath path_to_dll;
  HRESULT hr = GetPathToDllFromHandle(dll_handle, &path_to_dll);
  if (FAILED(hr))
    return hr;

  wchar_t short_path[MAX_PATH];
  DWORD short_length = std::size(short_path);
  short_length =
      ::GetShortPathName(path_to_dll.value().c_str(), short_path, short_length);
  if (short_length >= std::size(short_path)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "GetShortPathNameW hr=" << putHR(hr);
    return hr;
  }

  *entrypoint_arg = base::StrCat({L"\"", short_path, L"\",", entrypoint});

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

  std::wstring entrypoint_arg;
  HRESULT hr =
      GetEntryPointArgumentForRunDll(dll_handle, entrypoint, &entrypoint_arg);
  if (SUCCEEDED(hr))
    command_line->AppendArgNative(entrypoint_arg);

  return hr;
}

// Gets localized name for builtin administrator account. Extracting
// localized name for builtin administrator account requires DomainSid
// to be passed onto the CreateWellKnownSid function unlike any other
// WellKnownSid as per microsoft documentation. That's why we need to
// first extract the DomainSid (even for local accounts) and pass it as
// a parameter to the CreateWellKnownSid function call.
HRESULT GetLocalizedNameBuiltinAdministratorAccount(
    std::wstring* builtin_localized_admin_name) {
  LSA_HANDLE PolicyHandle;
  LSA_OBJECT_ATTRIBUTES oa = {sizeof(oa)};
  NTSTATUS status =
      LsaOpenPolicy(0, &oa, POLICY_VIEW_LOCAL_INFORMATION, &PolicyHandle);
  if (status >= 0) {
    PPOLICY_ACCOUNT_DOMAIN_INFO ppadi;
    status = LsaQueryInformationPolicy(
        PolicyHandle, PolicyAccountDomainInformation, (void**)&ppadi);
    if (status >= 0) {
      BYTE well_known_sid[SECURITY_MAX_SID_SIZE];
      DWORD size_local_users_group_sid = std::size(well_known_sid);
      if (CreateWellKnownSid(::WinAccountAdministratorSid, ppadi->DomainSid,
                             well_known_sid, &size_local_users_group_sid)) {
        return LookupLocalizedNameBySid(well_known_sid,
                                        builtin_localized_admin_name);
      } else {
        status = GetLastError();
      }
      LsaFreeMemory(ppadi);
    }
    LsaClose(PolicyHandle);
  }
  return status >= 0 ? S_OK : E_FAIL;
}

HRESULT LookupLocalizedNameBySid(PSID sid, std::wstring* localized_name) {
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
  *localized_name = std::wstring(localized_name_buffer.data(),
                                 localized_name_buffer.size() - 1);

  return S_OK;
}

HRESULT LookupLocalizedNameForWellKnownSid(WELL_KNOWN_SID_TYPE sid_type,
                                           std::wstring* localized_name) {
  BYTE well_known_sid[SECURITY_MAX_SID_SIZE];
  DWORD size_local_users_group_sid = std::size(well_known_sid);

  // Get the sid for the well known local users group.
  if (!::CreateWellKnownSid(sid_type, nullptr, well_known_sid,
                            &size_local_users_group_sid)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "CreateWellKnownSid hr=" << putHR(hr);
    return hr;
  }

  return LookupLocalizedNameBySid(well_known_sid, localized_name);
}

bool WriteToStartupSentinel() {
  LOGFN(VERBOSE);
  // Always try to write to the startup sentinel file. If writing or opening
  // fails for any reason (file locked, no access etc) consider this a failure.
  // If no sentinel file path can be found this probably means that we are
  // running in a unit test so just let the verification pass in this case.
  // Each process will only write once to startup sentinel file.

  static volatile long sentinel_initialized = 0;
  if (::InterlockedCompareExchange(&sentinel_initialized, 1, 0))
    return true;

  base::FilePath startup_sentinel_path =
      GetStartupSentinelLocation(TEXT(CHROME_VERSION_STRING));
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
    if (!startup_sentinel.IsValid()) {
      LOGFN(ERROR) << "Could not open the sentinel path "
                   << startup_sentinel_path.value();
      return false;
    }

    if (startup_sentinel.GetLength() >= kMaxConsecutiveCrashCount) {
      LOGFN(ERROR) << "Sentinel file length indicates "
                   << startup_sentinel.GetLength() << " possible crashes";
      return false;
    }

    LOGFN(VERBOSE) << "Writing to sentinel. Current length="
                   << startup_sentinel.GetLength();
    return startup_sentinel.WriteAtCurrentPosAndCheck(
        base::byte_span_from_cstring("0"));
  }

  return true;
}

void DeleteStartupSentinel() {
  DeleteStartupSentinelForVersion(TEXT(CHROME_VERSION_STRING));
}

void DeleteStartupSentinelForVersion(const std::wstring& version) {
  LOGFN(VERBOSE) << "Deleting sentinel for version " << version;
  base::FilePath startup_sentinel_path = GetStartupSentinelLocation(version);
  if (base::PathExists(startup_sentinel_path) &&
      !base::DeleteFile(startup_sentinel_path)) {
    LOGFN(ERROR) << "Failed to delete sentinel file: " << startup_sentinel_path;
  }
}

std::wstring GetStringResource(UINT base_message_id) {
  std::wstring localized_string;

  UINT message_id =
      static_cast<UINT>(base_message_id + GetLanguageSelector().offset());
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    localized_string = std::wstring(image->achString, image->nLength);
  } else {
    NOTREACHED_IN_MIGRATION() << "Unable to find resource id " << message_id;
  }

  return localized_string;
}

std::wstring GetStringResource(UINT base_message_id,
                               const std::vector<std::wstring>& subst) {
  std::wstring format_string = GetStringResource(base_message_id);
  std::wstring formatted =
      base::ReplaceStringPlaceholders(format_string, subst, nullptr);

  return formatted;
}

std::wstring GetSelectedLanguage() {
  return GetLanguageSelector().matched_candidate();
}

void SecurelyClearDictionaryValue(base::optional_ref<base::Value::Dict> dict) {
  SecurelyClearDictionaryValueWithKey(dict, kKeyPassword);
}

void SecurelyClearDictionaryValueWithKey(
    base::optional_ref<base::Value::Dict> dict,
    const std::string& password_key) {
  if (!dict.has_value()) {
    return;
  }

  if (auto* password_value = dict->FindString(password_key)) {
    SecurelyClearString(*password_value);
  }

  dict->clear();
}

void SecurelyClearString(std::wstring& str) {
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
    const std::initializer_list<std::string_view>& path) {
  DCHECK_GT(path.size(), 0UL);

  std::optional<base::Value::Dict> json_obj =
      base::JSONReader::ReadDict(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json_obj) {
    LOGFN(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    return std::string();
  }
  const std::string* value =
      json_obj->FindStringByDottedPath(base::JoinString(path, "."));
  return value ? *value : std::string();
}

std::wstring GetDictString(const base::Value::Dict& dict, const char* name) {
  DCHECK(name);
  const std::string* value = dict.FindString(name);
  return value ? base::UTF8ToWide(*value) : std::wstring();
}

std::string GetDictStringUTF8(const base::Value::Dict& dict, const char* name) {
  DCHECK(name);
  const std::string* value = dict.FindString(name);
  return value ? *value : std::string();
}

HRESULT SearchForListInStringDictUTF8(
    const std::string& list_key,
    const std::string& json_string,
    const std::initializer_list<std::string_view>& path,
    std::vector<std::string>* output) {
  DCHECK_GT(path.size(), 0UL);

  std::optional<base::Value::Dict> json_obj =
      base::JSONReader::ReadDict(json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!json_obj) {
    LOGFN(ERROR) << "base::JSONReader::Read failed to translate to JSON";
    return E_FAIL;
  }

  auto* value = json_obj->FindListByDottedPath(base::JoinString(path, "."));
  if (value) {
    for (const base::Value& entry_val : *value) {
      const base::Value::Dict& entry = entry_val.GetDict();
      const std::string* list_key_str = entry.FindString(list_key);
      if (list_key_str) {
        output->push_back(*list_key_str);
      } else {
        return E_FAIL;
      }
    }
  }
  return S_OK;
}

base::FilePath::StringType GetInstallParentDirectoryName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return FILE_PATH_LITERAL("Google");
#else
  return FILE_PATH_LITERAL("Chromium");
#endif
}

std::wstring GetWindowsVersion() {
  wchar_t release_id[32];
  ULONG length = std::size(release_id);
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

bool ExtractKeysFromDict(
    const base::Value::Dict& dict,
    const std::vector<std::pair<std::string, std::string*>>& needed_outputs) {
  for (const std::pair<std::string, std::string*>& output : needed_outputs) {
    const std::string* output_value = dict.FindString(output.first);
    if (!output_value) {
      LOGFN(ERROR) << "Could not extract value '" << output.first
                   << "' from server response";
      return false;
    }
    DCHECK(output.second);
    *output.second = *output_value;
  }
  return true;
}

std::wstring GetSerialNumber() {
  if (g_use_test_serial_number) {
    return TestSerialNumber();
  }
  return base::win::WmiComputerSystemInfo::Get().serial_number();
}

// This approach was inspired by:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa365917(v=vs.85).aspx
std::vector<std::string> GetMacAddresses() {
  // Used for unit tests.
  if (g_use_test_mac_addresses) {
    return TestMacAddresses();
  }

  PIP_ADAPTER_INFO pAdapter;
  ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
  IP_ADAPTER_INFO* pAdapterInfo =
      new IP_ADAPTER_INFO[ulOutBufLen / sizeof(IP_ADAPTER_INFO)];
  // Get the right buffer size in case of overflow.
  if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
    delete[] pAdapterInfo;
    pAdapterInfo =
        new IP_ADAPTER_INFO[ulOutBufLen / sizeof(IP_ADAPTER_INFO) + 1];
  }
  std::vector<std::string> mac_addresses;
  if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_SUCCESS) {
    pAdapter = pAdapterInfo;
    while (pAdapter) {
      if (pAdapter->AddressLength == 6) {
        char mac_address[17 + 1];
        snprintf(mac_address, sizeof(mac_address),
                 "%02X-%02X-%02X-%02X-%02X-%02X",
                 static_cast<unsigned int>(pAdapter->Address[0]),
                 static_cast<unsigned int>(pAdapter->Address[1]),
                 static_cast<unsigned int>(pAdapter->Address[2]),
                 static_cast<unsigned int>(pAdapter->Address[3]),
                 static_cast<unsigned int>(pAdapter->Address[4]),
                 static_cast<unsigned int>(pAdapter->Address[5]));
        mac_addresses.push_back(mac_address);
      }
      pAdapter = pAdapter->Next;
    }
  }
  delete[] pAdapterInfo;
  return mac_addresses;
}

void GetOsVersionFallback(std::string* version) {
  int buffer_size = GetFileVersionInfoSize(kKernelLibFile, nullptr);
  if (buffer_size) {
    std::vector<wchar_t> buffer(buffer_size, 0);
    if (GetFileVersionInfo(kKernelLibFile, 0, buffer_size, buffer.data())) {
      UINT size;
      void* fixed_version_info_raw;
      if (VerQueryValue(buffer.data(), L"\\", &fixed_version_info_raw, &size)) {
        VS_FIXEDFILEINFO* fixed_version_info =
            static_cast<VS_FIXEDFILEINFO*>(fixed_version_info_raw);
        // https://stackoverflow.com/questions/38068477
        int major = HIWORD(fixed_version_info->dwProductVersionMS);
        int minor = LOWORD(fixed_version_info->dwProductVersionMS);
        int build = HIWORD(fixed_version_info->dwProductVersionLS);
        char version_buffer[kVersionStringSize];
        snprintf(version_buffer, kVersionStringSize, "%d.%d.%d", major, minor,
                 build);
        *version = version_buffer;
      }
    }
  }
}

// The current solution is based on registries or the version of the
// "kernel32.dll" file. A cleaner alternative would be to use the GetVersionEx
// API. However, since Windows 8.1 the values returned by that API are dependent
// on how the application is manifested, and might not be the actual OS version.
// The format of the OS version is <Major>.<Minor>.<BuildNumber>. Eg: 10.0.18363
void GetOsVersion(std::string* version) {
  if (g_use_test_os_version) {
    *version = TestOSVersion();
    return;
  }

  // Fetching Windows version from registries.
  // https://stackoverflow.com/questions/32729244
  // https://stackoverflow.com/questions/31072543
  DWORD major;
  DWORD minor;
  wchar_t build[32];
  ULONG length = std::size(build);

  HRESULT hr1 = GetMachineRegDWORD(kOsRegistryPath, kOsMajorName, &major);
  HRESULT hr2 = GetMachineRegDWORD(kOsRegistryPath, kOsMinorName, &minor);
  HRESULT hr3 =
      GetMachineRegString(kOsRegistryPath, kOsBuildName, build, &length);

  if (SUCCEEDED(hr1) && SUCCEEDED(hr2) && SUCCEEDED(hr3)) {
    char version_buffer[kVersionStringSize];
    snprintf(version_buffer, kVersionStringSize, "%lu.%lu.%ls", major, minor,
             build);
    *version = version_buffer;
    return;
  }
  LOGFN(ERROR) << "Error while fetching Os version hr=" << hr1 << "," << hr2
               << "," << hr3;

  // Try getting the version from kernel.dll in case there is a issue with
  // getting OS version from registries.
  GetOsVersionFallback(version);
}

HRESULT GenerateDeviceId(std::string* device_id) {
  // Build the json data encapsulating different device ids.
  base::Value::Dict device_ids_dict;

  // Add the serial number to the dictionary.
  std::wstring serial_number = GetSerialNumber();
  if (!serial_number.empty()) {
    device_ids_dict.Set("serial_number", base::WideToUTF8(serial_number));
  }

  // Add machine_guid to the dictionary.
  std::wstring machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);
  if (SUCCEEDED(hr) && !machine_guid.empty()) {
    device_ids_dict.Set("machine_guid", base::WideToUTF8(machine_guid));
  }

  std::string device_id_str;
  bool json_write_result =
      base::JSONWriter::Write(device_ids_dict, &device_id_str);
  if (!json_write_result) {
    LOGFN(ERROR) << "JSONWriter::Write(device_ids_dict)";
    return E_FAIL;
  }

  // Store the base64encoded device id json blob in the output.
  *device_id = base::Base64Encode(device_id_str);
  return S_OK;
}

HRESULT SetGaiaEndpointCommandLineIfNeeded(const wchar_t* override_registry_key,
                                           const std::string& default_endpoint,
                                           bool provide_deviceid,
                                           bool show_tos,
                                           base::CommandLine* command_line) {
  // Registry specified endpoint.
  wchar_t endpoint_url_setting[256];
  ULONG endpoint_url_length = std::size(endpoint_url_setting);
  HRESULT hr = GetGlobalFlag(override_registry_key, endpoint_url_setting,
                             &endpoint_url_length);
  if (SUCCEEDED(hr) && endpoint_url_setting[0]) {
    GURL endpoint_url(base::AsStringPiece16(endpoint_url_setting));
    if (endpoint_url.is_valid()) {
      command_line->AppendSwitchASCII(switches::kGaiaUrl,
                                      endpoint_url.GetWithEmptyPath().spec());
      command_line->AppendSwitchASCII(kGcpwEndpointPathSwitch,
                                      endpoint_url.path().substr(1));
    }
    return S_OK;
  }

  if (provide_deviceid || show_tos) {
    std::string device_id;
    hr = GenerateDeviceId(&device_id);
    if (SUCCEEDED(hr)) {
      command_line->AppendSwitchASCII(
          kGcpwEndpointPathSwitch,
          base::StringPrintf("%s?device_id=%s&show_tos=%d",
                             default_endpoint.c_str(), device_id.c_str(),
                             show_tos ? 1 : 0));
    } else if (show_tos) {
      command_line->AppendSwitchASCII(
          kGcpwEndpointPathSwitch,
          base::StringPrintf("%s?show_tos=1", default_endpoint.c_str()));
    }
  }
  return S_OK;
}

base::FilePath GetChromePath() {
  base::FilePath gls_path = GetSystemChromePath();

  wchar_t custom_gls_path_value[MAX_PATH];
  ULONG path_len = std::size(custom_gls_path_value);
  HRESULT hr = GetGlobalFlag(kRegGlsPath, custom_gls_path_value, &path_len);
  if (SUCCEEDED(hr)) {
    base::FilePath custom_gls_path(custom_gls_path_value);
    if (base::PathExists(custom_gls_path)) {
      gls_path = custom_gls_path;
    } else {
      LOGFN(ERROR) << "Specified gls path ('" << custom_gls_path.value()
                   << "') does not exist, using default gls path.";
    }
  }

  return gls_path;
}

base::FilePath GetSystemChromePath() {
  if (g_use_test_chrome_path) {
    return TestChromePath();
  }

  return chrome_launcher_support::GetChromePathForInstallationLevel(
      chrome_launcher_support::SYSTEM_LEVEL_INSTALLATION, false);
}

HRESULT GenerateGCPWDmToken(const std::wstring& sid) {
  std::wstring dm_token;
  return GetGCPWDmTokenInternal(sid, &dm_token, true);
}

HRESULT GetGCPWDmToken(const std::wstring& sid, std::wstring* token) {
  return GetGCPWDmTokenInternal(sid, token, false);
}

FakesForTesting::FakesForTesting() {}

FakesForTesting::~FakesForTesting() {}

GURL GetGcpwServiceUrl() {
  std::wstring dev = GetGlobalFlagOrDefault(kRegDeveloperMode, L"");
  if (!dev.empty())
    return GURL(
        base::AsStringPiece16(GetDevelopmentUrl(kDefaultGcpwServiceUrl, dev)));

  return GURL(base::AsStringPiece16(kDefaultGcpwServiceUrl));
}

std::wstring GetDevelopmentUrl(const std::wstring& url,
                               const std::wstring& dev) {
  std::string project;
  std::string final_part;
  if (re2::RE2::FullMatch(base::WideToUTF8(url),
                          "https://(.*).(googleapis.com.*)", &project,
                          &final_part)) {
    std::string url_prefix = "https://" + base::WideToUTF8(dev) + "-";
    return base::UTF8ToWide(
        base::JoinString({url_prefix + project, "sandbox", final_part}, "."));
  }
  return url;
}

std::unique_ptr<base::File> GetOpenedFileForUser(
    const std::wstring& sid,
    uint32_t open_flags,
    const std::wstring& file_dir,
    const std::wstring& file_name) {
  base::FilePath experiments_dir = GetDirectoryFilePath(sid, file_dir);
  if (!base::DirectoryExists(experiments_dir)) {
    base::File::Error error;
    if (!CreateDirectoryAndGetError(experiments_dir, &error)) {
      LOGFN(ERROR) << "Experiments data directory could not be created for "
                   << sid << " Error: " << error;
      return nullptr;
    }
  }

  base::FilePath experiments_file_path = experiments_dir.Append(file_name);
  std::unique_ptr<base::File> experiments_file(
      new base::File(experiments_file_path, open_flags));

  if (!experiments_file->IsValid()) {
    LOGFN(ERROR) << "Error opening experiments file for user " << sid
                 << " with flags " << open_flags
                 << " Error: " << experiments_file->error_details();
    return nullptr;
  }

  base::File::Error lock_error =
      experiments_file->Lock(base::File::LockMode::kExclusive);
  if (lock_error != base::File::FILE_OK) {
    LOGFN(ERROR)
        << "Failed to obtain exclusive lock on experiments file! Error: "
        << lock_error;
    return nullptr;
  }

  return experiments_file;
}

base::TimeDelta GetTimeDeltaSinceLastFetch(const std::wstring& sid,
                                           const std::wstring& flag) {
  wchar_t last_fetch_millis[512];
  ULONG last_fetch_size = std::size(last_fetch_millis);
  HRESULT hr = GetUserProperty(sid, flag, last_fetch_millis, &last_fetch_size);

  if (FAILED(hr)) {
    return base::TimeDelta::Max();
  }

  int64_t last_fetch_millis_int64;
  base::StringToInt64(last_fetch_millis, &last_fetch_millis_int64);

  int64_t time_delta_from_last_fetch_ms =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds() -
      last_fetch_millis_int64;

  return base::Milliseconds(time_delta_from_last_fetch_ms);
}

}  // namespace credential_provider
