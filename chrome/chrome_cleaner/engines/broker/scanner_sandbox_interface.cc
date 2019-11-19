// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/scanner_sandbox_interface.h"

#include <psapi.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/engines/common/sandbox_error_code.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/win_utils.h"

namespace chrome_cleaner_sandbox {

namespace {

using chrome_cleaner::SandboxErrorCode;
using KnownFolder = chrome_cleaner::mojom::KnownFolder;

bool SandboxKnownFolderIdToPathServiceKey(KnownFolder folder_id,
                                          int* path_service_key) {
  switch (folder_id) {
    case KnownFolder::kWindows:
      *path_service_key = base::DIR_WINDOWS;
      return true;
    case KnownFolder::kProgramFiles:
      *path_service_key = base::DIR_PROGRAM_FILES;
      return true;
    case KnownFolder::kProgramFilesX86:
      *path_service_key = base::DIR_PROGRAM_FILESX86;
      return true;
    case KnownFolder::kAppData:
      *path_service_key = base::DIR_APP_DATA;
      return true;
    default:
      LOG(ERROR) << "Unknown SandboxKnownFolderId given, " << folder_id;
  }
  return false;
}

NTSTATUS NtQueryInformationProcess(HANDLE ProcessHandle,
                                   PROCESSINFOCLASS ProcessInformationClass,
                                   PVOID ProcessInformation,
                                   ULONG ProcessInformationLength,
                                   PULONG ReturnLength) {
  static NtQueryInformationProcessFunction query_information_process = nullptr;
  if (!query_information_process) {
    ResolveNTFunctionPtr("NtQueryInformationProcess",
                         &query_information_process);
  }
  return query_information_process(ProcessHandle, ProcessInformationClass,
                                   ProcessInformation, ProcessInformationLength,
                                   ReturnLength);
}

// Retrieves command line for the given process by using only
// NtQueryInformationProcess. This method is not available on all platforms, but
// poses less security concern then reading the command line from the process'
// memory (see GetCommandLineLegacy below), so it is used wherever possible.
// If this feature is not available, sets |feature_available| to false.
// Returns true on success.
bool GetCommandLineUsingProcessInformation(base::ProcessId pid,
                                           bool* feature_available,
                                           base::string16* process_cmd) {
  DCHECK(feature_available);
  DCHECK(process_cmd);
  *feature_available = true;

  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_INFORMATION, /*inherit_handle=*/FALSE, pid));
  if (!process.IsValid()) {
    PLOG_IF(ERROR, GetLastError() != ERROR_ACCESS_DENIED)
        << "Failed to open process";
    return false;
  }

  // Use undocumented ProcessCommandLineInformation value (60) of the
  // PROCESSINFOCLASS enumeration to retrieve UNICODE_STRING value containing
  // command line of the process.
  constexpr PROCESSINFOCLASS kProcessCommandLineInformation =
      static_cast<PROCESSINFOCLASS>(60);

  // 1. Query length of the information structure.
  DWORD info_length = 0;
  NTSTATUS status = NtQueryInformationProcess(
      process.Get(), kProcessCommandLineInformation, nullptr, 0, &info_length);
  if (status == STATUS_INVALID_INFO_CLASS || status == STATUS_NOT_IMPLEMENTED) {
    *feature_available = false;
    return false;
  }
  if (status != STATUS_INFO_LENGTH_MISMATCH) {
    LOG(ERROR) << "Error querying process command line length " << status;
    return false;
  }

  // 2. Retrieve UNICODE_STRING structure.
  // Command line is a unicode string, but since its length is specified in
  // bytes and the buffer is used only for transferring data into the protobuf
  // message, define the buffer in bytes.
  // Leave enough space to enforce the terminating null character.
  std::vector<char> buffer(info_length + sizeof(WCHAR));
  DWORD bytes_read = 0;
  status =
      NtQueryInformationProcess(process.Get(), kProcessCommandLineInformation,
                                buffer.data(), info_length, &bytes_read);
  if (NT_ERROR(status)) {
    LOG(ERROR) << "Error querying process command line: error " << status;
    return false;
  }
  if (bytes_read < sizeof(UNICODE_STRING) || bytes_read != info_length) {
    LOG(ERROR) << "Invalid buffer length";
    return false;
  }

  // 3. Check the retrieved data for reasonable command line length and absence
  // of null terminating characters.
  PUNICODE_STRING command_line =
      reinterpret_cast<PUNICODE_STRING>(buffer.data());

  // NtQueryInformationProcess can return a buffer filled with 0's (for example
  // when querying LsaIso.exe, an Isolated User Mode process). When we cast a
  // buffer of 0's to a UNICODE_STRING struct, all members including the Buffer
  // pointer becomed 0.
  if (command_line->Buffer == nullptr) {
    LOG(ERROR) << "Invalid command line buffer";
    return false;
  }

  size_t max_data_length = bytes_read - sizeof(UNICODE_STRING);
  if (command_line->Length % sizeof(WCHAR) ||
      command_line->Length > max_data_length) {
    LOG(ERROR) << "Invalid command line length";
    return false;
  }
  size_t wide_char_len = command_line->Length / sizeof(WCHAR);
  // Enforce terminating null (needed for wcslen).
  command_line->Buffer[wide_char_len] = L'\0';
  if (wcslen(command_line->Buffer) != wide_char_len) {
    LOG(ERROR) << "Invalid command line, embedded NUL characters";
    return false;
  }

  process_cmd->assign(command_line->Buffer, wide_char_len);
  return true;
}

template <class T>
bool ReadStructFromProcess(HANDLE process, LPCVOID base_address, T* dest) {
  SIZE_T bytes_read = 0;
  if (!::ReadProcessMemory(process, base_address, dest, sizeof(T),
                           &bytes_read)) {
    PLOG(ERROR) << "ReadProcessMemory failed";
    return false;
  }
  if (bytes_read != sizeof(T)) {
    LOG(ERROR) << "ReadProcessMemory incorrect struct size";
    return false;
  }
  return true;
}

// Retrieves command line of a process by reading its memory. Use
// GetCommandLineUsingProcessInformation instead whenever possible.
// IMPORTANT: get security review when changing this function. Or better yet,
// move it to its own sandbox process.
bool GetCommandLineLegacy(base::ProcessId pid, base::string16* process_cmd) {
  DCHECK(process_cmd);

  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                    /*inherit_handle=*/FALSE, pid));
  if (!process.IsValid()) {
    PLOG_IF(ERROR, GetLastError() != ERROR_ACCESS_DENIED)
        << "Failed to open process";
    return false;
  }

  PROCESS_BASIC_INFORMATION process_basic_info;
  ULONG process_info_size = 0;
  NTSTATUS query_info_status = NtQueryInformationProcess(
      process.Get(), ProcessBasicInformation, &process_basic_info,
      sizeof(process_basic_info), &process_info_size);
  if (query_info_status != STATUS_SUCCESS) {
    LOG(ERROR) << "NtQueryInformationProcess failed with " << query_info_status;
    return false;
  } else if (process_info_size != sizeof(process_basic_info)) {
    LOG(ERROR) << "NtQueryInformationProcess read incorrect size";
    return false;
  }

  PEB process_environ_block;
  if (!ReadStructFromProcess(process.Get(), process_basic_info.PebBaseAddress,
                             &process_environ_block)) {
    return false;
  }
  RTL_USER_PROCESS_PARAMETERS process_params;
  if (!ReadStructFromProcess(process.Get(),
                             process_environ_block.ProcessParameters,
                             &process_params)) {
    return false;
  }
  if (process_params.CommandLine.Length % sizeof(WCHAR) != 0) {
    LOG(ERROR) << "Command line of wide characters has odd bytes length";
    return false;
  }

  // Command line is a unicode string, but its length is specified in bytes.
  size_t wide_char_len = process_params.CommandLine.Length / sizeof(WCHAR);
  std::vector<WCHAR> command_line(wide_char_len);
  SIZE_T bytes_read = 0;
  if (!::ReadProcessMemory(process.Get(), process_params.CommandLine.Buffer,
                           command_line.data(),
                           process_params.CommandLine.Length, &bytes_read)) {
    PLOG(ERROR) << "ReadProcessMemory failed";
    return false;
  } else if (bytes_read != process_params.CommandLine.Length) {
    LOG(ERROR) << "ReadProcessMemory read incorrect size";
    return false;
  }

  process_cmd->assign(command_line.data(), wide_char_len);
  return true;
}

}  // namespace

uint32_t SandboxFindFirstFile(const base::FilePath& file_name,
                              LPWIN32_FIND_DATAW lpFindFileData,
                              HANDLE* handle) {
  if (!chrome_cleaner::ValidateSandboxFilePath(file_name)) {
    *handle = INVALID_HANDLE_VALUE;
    return SandboxErrorCode::INVALID_FILE_PATH;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  *handle = ::FindFirstFile(file_name.value().c_str(), lpFindFileData);

  if (*handle == INVALID_HANDLE_VALUE)
    return GetLastError();

  // Skip remote files.
  if (!chrome_cleaner::IsLocalFileAttributes(
          lpFindFileData->dwFileAttributes)) {
    uint32_t found_next_result = SandboxFindNextFile(*handle, lpFindFileData);
    if (found_next_result != ERROR_SUCCESS) {
      SandboxFindClose(*handle);
      *handle = INVALID_HANDLE_VALUE;
      return found_next_result;
    }
  }

  return 0;
}

uint32_t SandboxFindNextFile(HANDLE hFindFile,
                             LPWIN32_FIND_DATAW lpFindFileData) {
  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  // Return the first local file found.
  while (::FindNextFile(hFindFile, lpFindFileData)) {
    if (chrome_cleaner::IsLocalFileAttributes(lpFindFileData->dwFileAttributes))
      return 0;
  }
  return GetLastError();
}

uint32_t SandboxFindClose(HANDLE hFindFile) {
  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  if (::FindClose(hFindFile) == FALSE) {
    return GetLastError();
  }
  return 0;
}

uint32_t SandboxGetFileAttributes(const base::FilePath& file_name,
                                  uint32_t* attributes) {
  DCHECK(attributes);
  if (!chrome_cleaner::ValidateSandboxFilePath(file_name)) {
    *attributes = INVALID_FILE_ATTRIBUTES;
    return SandboxErrorCode::INVALID_FILE_PATH;
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;
  *attributes = ::GetFileAttributes(file_name.value().c_str());
  if (*attributes == INVALID_FILE_ATTRIBUTES)
    return ::GetLastError();
  // Pretend remote files don't exist.
  if (!chrome_cleaner::IsLocalFileAttributes(*attributes)) {
    *attributes = INVALID_FILE_ATTRIBUTES;
    return ERROR_FILE_NOT_FOUND;
  }
  return ERROR_SUCCESS;
}

bool SandboxGetKnownFolderPath(KnownFolder folder_id,
                               base::FilePath* folder_path) {
  if (!folder_path)
    return false;

  int path_service_key;
  if (!SandboxKnownFolderIdToPathServiceKey(folder_id, &path_service_key))
    return false;

  if (!base::PathService::Get(path_service_key, folder_path))
    LOG(ERROR)
        << "base::PathService::Get failed to convert key to folder path.";

  return true;
}

bool SandboxGetProcesses(std::vector<base::ProcessId>* processes) {
  if (!processes)
    return false;

  processes->resize(512);
  DWORD bytes_returned;
  DWORD current_byte_size;
  // Double array size until all process IDs fit into it.
  do {
    processes->resize(processes->size() * 2);
    current_byte_size = processes->size() * sizeof(DWORD);
    if (!(::EnumProcesses(reinterpret_cast<DWORD*>(processes->data()),
                          current_byte_size, &bytes_returned))) {
      PLOG(ERROR) << "Failed to enumerate processes";
      return false;
    }
  } while (bytes_returned == current_byte_size);

  size_t processes_size = bytes_returned / sizeof(DWORD);
  processes->resize(processes_size);
  return true;
}

bool SandboxGetTasks(
    std::vector<chrome_cleaner::TaskScheduler::TaskInfo>* task_list) {
  if (!task_list)
    return false;

  std::unique_ptr<chrome_cleaner::TaskScheduler> task_scheduler(
      chrome_cleaner::TaskScheduler::CreateInstance());
  std::vector<base::string16> registered_task_names;
  if (!task_scheduler->GetTaskNameList(&registered_task_names)) {
    LOG(ERROR) << "Failed to enumerate scheduled tasks.";
    return false;
  }

  task_list->clear();
  for (const auto& task_name : registered_task_names) {
    chrome_cleaner::TaskScheduler::TaskInfo task_info = {};
    if (!task_scheduler->GetTaskInfo(task_name.c_str(), &task_info)) {
      LOG(ERROR) << "Failed to get info for task '" << task_name << "'";
      continue;
    }
    task_list->push_back(std::move(task_info));
  }

  return true;
}

bool SandboxGetProcessImagePath(base::ProcessId pid,
                                base::FilePath* image_path) {
  if (!image_path)
    return false;

  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!process.IsValid())
    return false;

  base::string16 image_path_str;
  if (!chrome_cleaner::GetProcessExecutablePath(process.Get(), &image_path_str))
    return false;

  *image_path = base::FilePath(image_path_str);
  return true;
}

bool SandboxGetLoadedModules(base::ProcessId pid,
                             std::set<base::string16>* module_names) {
  if (!module_names)
    return false;

  // Note: code below does not work when running as 32bit process on x64 system
  // and enumerating loaded modules of a 64bit process, but that is okay since
  // this function should only be called when the bitness matches.
  base::win::ScopedHandle process(
      ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
  if (!process.IsValid()) {
    // OpenProcess is expected to fail on System Process, System Idle Process
    // and CSRSS processes. Reduce the error message to warning or info if it
    // spams too much.
    PLOG_IF(ERROR, GetLastError() != ERROR_ACCESS_DENIED)
        << "Failed to open process";
    return false;
  }

  return chrome_cleaner::GetLoadedModuleFileNames(process.Get(), module_names);
}

bool SandboxGetProcessCommandLine(base::ProcessId pid,
                                  base::string16* command_line) {
  if (!command_line)
    return false;

  // Use GetCommandLineUsingProcessInformation whenever it's possible, fall back
  // to GetCommandLineLegacy otherwise.
  static bool process_info_class_available = true;
  if (process_info_class_available) {
    bool result = GetCommandLineUsingProcessInformation(
        pid, &process_info_class_available, command_line);
    if (process_info_class_available)
      return result;
  }

  return GetCommandLineLegacy(pid, command_line);
}

bool SandboxGetUserInfoFromSID(
    const SID* const sid,
    chrome_cleaner::mojom::UserInformation* user_info) {
  if (!user_info)
    return false;

  DWORD name_size = 0;
  DWORD domain_size = 0;
  SID_NAME_USE side_type;
  // Both |name_size| and |domain_size| will be set to the required length.
  LookupAccountSid(/*lpSystemName=*/NULL, const_cast<SID*>(sid), NULL,
                   &name_size, NULL, &domain_size, &side_type);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    PLOG(ERROR) << "LookupAccountSid failed to return sizes";
    return false;
  }

  std::vector<wchar_t> name(name_size);
  std::vector<wchar_t> domain(domain_size);
  if (!LookupAccountSid(/*lpSystemName=*/NULL, const_cast<SID*>(sid),
                        name.data(), &name_size, domain.data(), &domain_size,
                        &side_type)) {
    PLOG(ERROR) << "LookupAccountSid failed";
    return false;
  }

  user_info->name.assign(name.data(), name_size);
  user_info->domain.assign(domain.data(), domain_size);
  user_info->account_type = side_type;
  return true;
}

base::win::ScopedHandle SandboxOpenReadOnlyFile(const base::FilePath& file_name,
                                                uint32_t dwFlagsAndAttributes) {
  if (!chrome_cleaner::ValidateSandboxFilePath(file_name)) {
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
  }

  static constexpr DWORD acceptable_dw_flags_and_attributes_values =
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING |
      FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_RANDOM_ACCESS |
      FILE_FLAG_OPEN_REPARSE_POINT;
  if ((dwFlagsAndAttributes & acceptable_dw_flags_and_attributes_values) !=
      dwFlagsAndAttributes) {
    LOG(ERROR)
        << "OpenReadOnlyFile called with invalid |dwFlagsAndAttributes|. "
           "|dwFlagsAndAttributes| can only contain 0x"
        << std::hex << acceptable_dw_flags_and_attributes_values
        << ", called with 0x" << dwFlagsAndAttributes;
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
  }

  chrome_cleaner::ScopedDisableWow64Redirection disable_wow64_redirection;

  if (!chrome_cleaner::IsFilePresentLocally(file_name))
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);

  // Don't lock this file when opening it and allow other processes to do
  // anything they want to it while this handle is open.
  DWORD dwShareMode = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE;
  base::win::ScopedHandle handle(::CreateFile(
      file_name.value().c_str(), GENERIC_READ, dwShareMode,
      /*lpSecurityDisposition=*/nullptr, OPEN_EXISTING, dwFlagsAndAttributes,
      /*hTemplateFile=*/nullptr));
  PLOG_IF(ERROR, !handle.IsValid())
      << "Failed to open " << chrome_cleaner::SanitizePath(file_name);

  int64_t size_limit =
      chrome_cleaner::Settings::GetInstance()->open_file_size_limit();
  if (size_limit > 0) {
    LARGE_INTEGER size;
    if (!::GetFileSizeEx(handle.Get(), &size)) {
      PLOG(ERROR) << "Failed to get size of file "
                  << chrome_cleaner::SanitizePath(file_name);
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }
    if (size.QuadPart > size_limit) {
      LOG(ERROR) << "File " << chrome_cleaner::SanitizePath(file_name)
                 << " is too big, size: " << size.QuadPart;
      return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
    }
  }

  return handle;
}

uint32_t SandboxOpenReadOnlyRegistry(HANDLE root_key,
                                     const base::string16& sub_key,
                                     uint32_t dw_access,
                                     HKEY* registry_handle) {
  if (registry_handle == nullptr) {
    LOG(ERROR) << "Null registry_handle passed to OpenReadOnlyRegistry";
    return SandboxErrorCode::NULL_OUTPUT_HANDLE;
  }
  *registry_handle = static_cast<HKEY>(INVALID_HANDLE_VALUE);

  // The acceptable values for dw_access.
  static constexpr DWORD acceptable_dw_access_values =
      KEY_WOW64_32KEY | KEY_WOW64_64KEY | KEY_READ;
  if ((dw_access & acceptable_dw_access_values) != dw_access) {
    LOG(ERROR) << "OpenReadOnlyRegistry called with invalid parameters for "
                  "dw_access. dw_access can only contain 0x"
               << std::hex << acceptable_dw_access_values << ", called with 0x "
               << dw_access;
    return SandboxErrorCode::INVALID_DW_ACCESS;
  }

  if (root_key == nullptr) {
    LOG(ERROR) << "NULL root_key passed to OpenReadOnlyRegistry";
    return SandboxErrorCode::NULL_ROOT_KEY;
  }

  LONG result = RegOpenKeyEx(static_cast<HKEY>(root_key), sub_key.c_str(), 0,
                             dw_access, registry_handle);
  if (result != ERROR_SUCCESS) {
    // Don't log anything if the open failed because the key doesn't exist
    // because a lot of keys that won't always exist are checked, and logging
    // each time just spams the console or log file.
    LOG_IF(ERROR, result != ERROR_FILE_NOT_FOUND)
        << "Failed to open registry key: 0x" << std::hex << result;
    *registry_handle = static_cast<HKEY>(INVALID_HANDLE_VALUE);
    return result;
  }

  return 0;
}

uint32_t SandboxNtOpenReadOnlyRegistry(
    HANDLE root_key,
    const chrome_cleaner::String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    HANDLE* registry_handle) {
  if (registry_handle == nullptr) {
    LOG(ERROR) << "NtOpenReadOnlyRegistry called with NULL registry handle.";
    return SandboxErrorCode::NULL_OUTPUT_HANDLE;
  }

  *registry_handle = INVALID_HANDLE_VALUE;

  if (root_key == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "NtOpenReadOnlyRegistry called with invalid root_key.";
    return SandboxErrorCode::INVALID_KEY;
  }

  NtRegistryParamError param_error = ValidateNtRegistryKey(sub_key);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "NtOpenReadOnlyRegistry called with invalid sub_key: "
               << param_error
               << ", path: " << FormatNtRegistryMemberForLogging(sub_key);
    switch (param_error) {
      case NtRegistryParamError::NullParam:
      case NtRegistryParamError::ZeroLength:
        return SandboxErrorCode::NULL_SUB_KEY;
      default:
        return SandboxErrorCode::INVALID_SUBKEY_STRING;
    }
  }

  // The acceptable flags for dw_access.
  static constexpr DWORD acceptable_dw_access_values =
      KEY_READ | KEY_WOW64_32KEY | KEY_WOW64_64KEY;
  if ((dw_access & acceptable_dw_access_values) != dw_access) {
    LOG(ERROR) << "NtOpenReadOnlyRegistry called with invalid parameters for "
                  "|dw_access|. |dw_access| can only contain 0x"
               << std::hex << acceptable_dw_access_values << ", called with 0x"
               << dw_access;
    return SandboxErrorCode::INVALID_DW_ACCESS;
  }

  // Ensure that sub_key is fully qualified if the root_key is null.
  if (root_key == nullptr && sub_key.CastAsWCharArray()[0] != L'\\') {
    LOG(ERROR) << "NtOpenReadOnlyRegistry called with null root_key and "
                  "relative sub_key: "
               << FormatNtRegistryMemberForLogging(sub_key);
    return SandboxErrorCode::NULL_ROOT_AND_RELATIVE_SUB_KEY;
  }

  NTSTATUS status =
      NativeOpenKey(root_key, sub_key, dw_access, registry_handle);
  if (status != STATUS_SUCCESS) {
    LOG_IF(ERROR, status != STATUS_OBJECT_NAME_NOT_FOUND)
        << "Call to NtOpenKey failed. Returned: " << status
        << ", path: " << FormatNtRegistryMemberForLogging(sub_key);
    *registry_handle = INVALID_HANDLE_VALUE;
    return status;
  }

  return 0;
}

}  // namespace chrome_cleaner_sandbox
