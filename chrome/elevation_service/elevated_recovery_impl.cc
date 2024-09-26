// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/elevated_recovery_impl.h"

#include <objbase.h>

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/scoped_process_information.h"
#include "chrome/install_static/install_util.h"
#include "chrome/windows_services/service_program/scoped_client_impersonation.h"
#include "components/crx_file/crx_verifier.h"
#include "third_party/zlib/google/zip.h"

namespace elevation_service {

namespace {

// Input CRX files over 10 MB are considered invalid.
constexpr int64_t kMaxFileSize = 10u * 1000u * 1000u;

// The hard-coded file name that the Recovery CRX is copied to.
constexpr base::FilePath::CharType kCRXFileName[] =
    FILE_PATH_LITERAL("ChromeRecoveryCRX.crx");

// The hard-coded Recovery subdirectory where the CRX is unpacked and executed.
constexpr base::FilePath::CharType kRecoveryDirectory[] =
    FILE_PATH_LITERAL("ChromeRecovery");

// The hard-coded Recovery executable. This file comes from the CRX, and we
// create an elevated process from it.
constexpr base::FilePath::CharType kRecoveryExeName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

// The hard-coded SHA256 of the SubjectPublicKeyInfo used to sign the Recovery
// CRX which contains ChromeRecovery.exe.
std::vector<uint8_t> GetRecoveryCRXHash() {
  return std::vector<uint8_t>{0x5f, 0x94, 0xe0, 0x3c, 0x64, 0x30, 0x9f, 0xbc,
                              0xfe, 0x00, 0x9a, 0x27, 0x3e, 0x52, 0xbf, 0xa5,
                              0x84, 0xb9, 0xb3, 0x75, 0x07, 0x29, 0xde, 0xfa,
                              0x32, 0x76, 0xd9, 0x93, 0xb5, 0xa3, 0xce, 0x02};
}

// This function is only meant to be called when a Windows API function errors
// out, and the corresponding ::GetLastResult is expected to be set to an error.
// There could be cases where ::GetLastError() is not set correctly, this
// function returns E_FAIL in those cases.
HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != ERROR_SUCCESS) ? HRESULT_FROM_WIN32(error_code)
                                       : E_FAIL;
}

// Opens and returns the COM caller's |process| given the process id, or the
// current process if |proc_id| is 0.
HRESULT OpenCallingProcess(uint32_t proc_id, base::Process* process) {
  DCHECK(proc_id);
  DCHECK(process);

  ScopedClientImpersonation impersonate_client;
  if (!impersonate_client.is_valid()) {
    return impersonate_client.result();
  }

  *process = base::Process::OpenWithAccess(proc_id, PROCESS_DUP_HANDLE);
  return process->IsValid() ? S_OK : HRESULTFromLastError();
}

// Opens and returns a base::File instance for the |file_path|. We impersonate
// the COM caller when opening the base::File instance. This is to ensure that
// the COM caller has access to the file.
HRESULT OpenFileImpersonated(const base::FilePath& file_path,
                             int flags,
                             base::File* file) {
  DCHECK(file);

  ScopedClientImpersonation impersonate_client;
  if (!impersonate_client.is_valid()) {
    return impersonate_client.result();
  }

  file->Initialize(file_path, flags);
  if (!file->IsValid())
    return HRESULTFromLastError();

  if (::GetFileType(file->GetPlatformFile()) != FILE_TYPE_DISK)
    return E_INVALIDARG;

  int64_t from_length = file->GetLength();
  return from_length > 0 && from_length < kMaxFileSize ? S_OK : E_INVALIDARG;
}

// Opens |from| while impersonating the COM caller, and then copies the contents
// of |from| to |to|. |from| is opened impersonated to ensure that the COM
// caller has access to the file.
HRESULT CopyFileImpersonated(const base::FilePath from,
                             const base::FilePath& to) {
  base::File from_file;
  HRESULT hr =
      OpenFileImpersonated(from,
                           base::File::FLAG_READ | base::File::FLAG_OPEN |
                               base::File::FLAG_WIN_SEQUENTIAL_SCAN,
                           &from_file);
  if (FAILED(hr))
    return hr;

  base::File to_file;
  to_file.Initialize(to, base::File::FLAG_WRITE |
                             base::File::FLAG_CREATE_ALWAYS |
                             base::File::FLAG_WIN_SEQUENTIAL_SCAN);
  if (!to_file.IsValid())
    return HRESULTFromLastError();

  constexpr size_t kBufferSize = 0x10000;
  std::vector<char> buffer(kBufferSize);

  for (uint64_t total_bytes_read = 0;;) {
    const int bytes_read =
        from_file.ReadAtCurrentPos(buffer.data(), buffer.size());
    if (bytes_read < 0)
      return HRESULTFromLastError();
    if (bytes_read == 0)
      return S_OK;

    total_bytes_read += bytes_read;
    if (total_bytes_read > kMaxFileSize)
      return E_INVALIDARG;

    const int bytes_written = to_file.WriteAtCurrentPos(&buffer[0], bytes_read);
    if (bytes_written < 0)
      return HRESULTFromLastError();
    if (bytes_written != bytes_read)
      return E_UNEXPECTED;
  }

  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

// Validates the provided CRX using the |crx_hash|, and if validation succeeds,
// unpacks the CRX under |unpack_under_path|. Returns the unpacked CRX
// directory in |unpacked_crx_dir|.
HRESULT ValidateAndUnpackCRX(const base::FilePath& from_crx_path,
                             const crx_file::VerifierFormat& crx_format,
                             const std::vector<uint8_t>& crx_hash,
                             const base::FilePath& unpack_under_path,
                             base::ScopedTempDir* unpacked_crx_dir) {
  DCHECK(unpacked_crx_dir);

  base::ScopedTempDir to_dir;
  if (!to_dir.CreateUniqueTempDirUnderPath(unpack_under_path))
    return HRESULTFromLastError();

  const base::FilePath to_crx_path = to_dir.GetPath().Append(kCRXFileName);

  // Copy |from_crx_path| impersonated. This is to prevent us from copying files
  // that may not be accessible to the calling COM user.
  HRESULT hr = CopyFileImpersonated(from_crx_path, to_crx_path);
  if (FAILED(hr))
    return hr;

  std::string public_key;
  if (crx_file::Verify(to_crx_path, crx_format, {crx_hash}, {}, &public_key,
                       nullptr, /*compressed_verified_contents=*/nullptr) !=
      crx_file::VerifierResult::OK_FULL) {
    return CRYPT_E_NO_MATCH;
  }

  if (!zip::Unzip(to_crx_path, to_dir.GetPath()))
    return E_UNEXPECTED;

  if (!base::DeleteFile(to_crx_path)) {
    PLOG(WARNING) << "Failed to delete " << to_crx_path;
  }

  if (!unpacked_crx_dir->Set(to_dir.Take())) {
    LOG(WARNING) << "Failed to transfer ownership of " << to_dir.GetPath();
  }
  return S_OK;
}

// Runs the executable |path_and_name| with the provided |args|. The returned
// |proc_handle| is a process handle that is valid for the |calling_process|
// process.
HRESULT LaunchCmd(const base::CommandLine& command_line,
                  const base::Process& calling_process,
                  base::win::ScopedHandle* proc_handle) {
  DCHECK(!command_line.GetCommandLineString().empty());
  DCHECK(proc_handle);

  base::LaunchOptions options = {};
  options.feedback_cursor_off = true;
  base::GetTempDir(&options.current_directory);
  base::Process proc = base::LaunchProcess(command_line, options);
  if (!proc.IsValid())
    return HRESULTFromLastError();

  HANDLE duplicate_proc_handle = nullptr;
  constexpr DWORD desired_access =
      PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
  bool res = ::DuplicateHandle(
                 ::GetCurrentProcess(),     // Current process.
                 proc.Handle(),             // Process handle to duplicate.
                 calling_process.Handle(),  // Process receiving the handle.
                 &duplicate_proc_handle,    // Duplicated handle.
                 desired_access,  // Access requested for the new handle.
                 FALSE,           // Don't inherit the new handle.
                 0) != 0;
  if (!res)
    return HRESULTFromLastError();

  proc_handle->Set(duplicate_proc_handle);
  return S_OK;
}

HRESULT ValidateCRXArgs(const std::wstring& browser_appid,
                        const std::wstring& browser_version,
                        const std::wstring& session_id) {
  if (!browser_appid.empty()) {
    GUID guid = {};
    HRESULT hr = ::IIDFromString(browser_appid.c_str(), &guid);
    if (FAILED(hr))
      return hr;
  }

  const base::Version version(base::WideToASCII(browser_version));
  if (!version.IsValid())
    return E_INVALIDARG;

  GUID session_guid = {};
  return ::IIDFromString(session_id.c_str(), &session_guid);
}

// Deletes all the files and subdirectories within |directory_path|. Errors are
// ignored.
void DeleteDirectoryFiles(const base::FilePath& directory_path) {
  base::FileEnumerator file_enum(
      directory_path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = file_enum.Next(); !current.empty();
       current = file_enum.Next()) {
    base::DeletePathRecursively(current);
  }
}

// Schedules deletion after reboot of |dir_name| as well as all the files and
// subdirectories within |dir_name|. Errors are ignored.
void ScheduleDirectoryForDeletion(const base::FilePath& dir_name) {
  // First schedule all the files within |dir_name| for deletion.
  base::FileEnumerator file_enum(dir_name, false, base::FileEnumerator::FILES);
  for (base::FilePath file = file_enum.Next(); !file.empty();
       file = file_enum.Next()) {
    base::DeleteFileAfterReboot(file);
  }

  // Then recurse to all the subdirectories.
  base::FileEnumerator dir_enum(dir_name, false,
                                base::FileEnumerator::DIRECTORIES);
  for (base::FilePath sub_dir = dir_enum.Next(); !sub_dir.empty();
       sub_dir = dir_enum.Next()) {
    ScheduleDirectoryForDeletion(sub_dir);
  }

  // Now schedule the empty directory itself.
  base::DeleteFileAfterReboot(dir_name);
}

// Returns the ChromeRecovery directory under Google\Chrome. For machine
// installs, this directory is under %ProgramFiles%, which is writeable only by
// adminstrators. We use this secure directory to validate and unpack the CRX to
// prevent tampering.
HRESULT GetChromeRecoveryDirectory(base::FilePath* dir) {
  base::FilePath recovery_dir;
  if (!base::PathService::Get(base::DIR_EXE, &recovery_dir))
    return HRESULTFromLastError();

  recovery_dir = recovery_dir.DirName().DirName().Append(kRecoveryDirectory);
  *dir = std::move(recovery_dir);
  return S_OK;
}

}  // namespace

HRESULT CleanupChromeRecoveryDirectory() {
  base::FilePath recovery_dir;
  HRESULT hr = GetChromeRecoveryDirectory(&recovery_dir);
  if (FAILED(hr))
    return hr;

  DeleteDirectoryFiles(recovery_dir);

  return S_OK;
}

HRESULT RunChromeRecoveryCRX(const base::FilePath& crx_path,
                             const std::wstring& browser_appid,
                             const std::wstring& browser_version,
                             const std::wstring& session_id,
                             uint32_t caller_proc_id,
                             base::win::ScopedHandle* proc_handle) {
  if (crx_path.empty() || !caller_proc_id || !proc_handle)
    return E_INVALIDARG;

  HRESULT hr = ValidateCRXArgs(browser_appid, browser_version, session_id);
  if (FAILED(hr))
    return hr;

  base::CommandLine args(base::CommandLine::NO_PROGRAM);
  if (!browser_appid.empty())
    args.AppendSwitchNative("appguid", browser_appid);
  args.AppendSwitchNative("browser-version", browser_version);
  args.AppendSwitchNative("sessionid", session_id);
  args.AppendSwitch("system");

  base::FilePath unpack_dir;
  hr = GetChromeRecoveryDirectory(&unpack_dir);
  if (FAILED(hr))
    return hr;

  return RunCRX(crx_path, args,
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
                GetRecoveryCRXHash(), unpack_dir,
                base::FilePath(kRecoveryExeName), caller_proc_id, proc_handle);
}

HRESULT RunCRX(const base::FilePath& crx_path,
               const base::CommandLine& args,
               const crx_file::VerifierFormat& crx_format,
               const std::vector<uint8_t>& crx_hash,
               const base::FilePath& unpack_under_path,
               const base::FilePath& exe_filename,
               uint32_t caller_proc_id,
               base::win::ScopedHandle* proc_handle) {
  DCHECK(!crx_path.empty());
  DCHECK(!crx_hash.empty());
  DCHECK(!unpack_under_path.empty());
  DCHECK(!exe_filename.empty());
  DCHECK(caller_proc_id);
  DCHECK(proc_handle);

  base::Process calling_process;
  HRESULT hr = OpenCallingProcess(caller_proc_id, &calling_process);
  if (FAILED(hr))
    return hr;

  base::ScopedTempDir unpacked_crx_dir;
  hr = ValidateAndUnpackCRX(crx_path, crx_format, crx_hash, unpack_under_path,
                            &unpacked_crx_dir);
  if (FAILED(hr))
    return hr;

  const base::FilePath path_and_name =
      unpacked_crx_dir.GetPath().Append(exe_filename);
  base::CommandLine command_line(path_and_name);
  command_line.AppendArguments(args, false);

  base::win::ScopedHandle scoped_proc_handle;
  hr = LaunchCmd(command_line, calling_process, &scoped_proc_handle);
  if (FAILED(hr))
    return hr;

  ScheduleDirectoryForDeletion(unpacked_crx_dir.Take());
  *proc_handle = std::move(scoped_proc_handle);
  return hr;
}

}  // namespace elevation_service
