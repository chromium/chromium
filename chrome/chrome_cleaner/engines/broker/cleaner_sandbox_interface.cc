// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/cleaner_sandbox_interface.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/constants/common_registry_names.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/engines/common/registry_util.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/win_utils.h"

// This typedef is not included in sandbox/'s nt_internals.h.
typedef NTSTATUS(WINAPI* NtDeleteValueKeyFunction)(IN HANDLE KeyHandle,
                                                   IN PUNICODE_STRING
                                                       ValueName);

using chrome_cleaner::String16EmbeddedNulls;

namespace chrome_cleaner_sandbox {

namespace {

void NormalizeValue(String16EmbeddedNulls* value) {
  for (unsigned int i = 0; i < value->size(); i++) {
    if (value->data()[i] == L' ')
      value->data()[i] = L',';
  }
}

}  // namespace

bool SandboxNtDeleteRegistryKey(const String16EmbeddedNulls& key) {
  // TODO(joenotcharles): Add some sanity checks from the old RegistryRemover.
  NtRegistryParamError param_error = ValidateNtRegistryKey(key);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "SandboxNtDeleteRegistryKey called with invalid key: "
               << param_error
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  bool success = true;
  HANDLE delete_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      NativeOpenKey(/*parent_key=*/NULL, key, KEY_ALL_ACCESS, &delete_handle);
  if (status != STATUS_SUCCESS || delete_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "SandboxNtDeleteRegistryKey: Call to NtOpenKey failed. "
               << "Returned: " << status
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    success = false;
  }

  status = NativeDeleteKey(delete_handle);
  if (status != STATUS_SUCCESS) {
    LOG(ERROR) << "SandboxNtDeleteRegistryKey: Call to NtDeleteKey failed. "
               << "Returned: " << status
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    success = false;
  }

  LOG(INFO) << "Successfully deleted registry key, path: "
            << FormatNtRegistryMemberForLogging(key);

  ::CloseHandle(delete_handle);
  return success;
}

bool SandboxNtDeleteRegistryValue(
    const chrome_cleaner::String16EmbeddedNulls& key,
    const chrome_cleaner::String16EmbeddedNulls& value_name) {
  // TODO(joenotcharles): Add some sanity checks from the old RegistryRemover.
  NtRegistryParamError param_error = ValidateNtRegistryKey(key);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "SandboxNtDeleteRegistryValue called with invalid key: "
               << param_error
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  param_error = ValidateNtRegistryNullTerminatedParam(value_name);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "SandboxNtDeleteRegistryValue called with invalid "
               << "value_name: " << FormatNtRegistryMemberForLogging(value_name)
               << " under key: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  HANDLE registry_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      NativeOpenKey(/*parent_key=*/NULL, key, KEY_ALL_ACCESS, &registry_handle);
  if (status != STATUS_SUCCESS || registry_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR)
        << "SandboxNtDeleteRegistryValue: Call to NtOpenKey failed. Returned: "
        << status << ", path: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  UNICODE_STRING uni_value_name = {};
  std::vector<wchar_t> value_name_buffer(value_name.data());
  InitUnicodeString(&uni_value_name, &value_name_buffer);

  static NtDeleteValueKeyFunction NtDeleteValueKey = nullptr;
  if (!NtDeleteValueKey)
    ResolveNTFunctionPtr("NtDeleteValueKey", &NtDeleteValueKey);

  status = NtDeleteValueKey(registry_handle, &uni_value_name);
  if (status != STATUS_SUCCESS) {
    LOG_IF(ERROR, status != STATUS_OBJECT_NAME_NOT_FOUND)
        << "SandboxNtDeleteRegistryValue: Failed to delete registry value: "
        << base::string16(value_name_buffer.begin(), value_name_buffer.end())
        << " under key: " << FormatNtRegistryMemberForLogging(key)
        << " error: " << status;
  }

  LOG(INFO) << "Successfully deleted registry value, value_name: "
            << FormatNtRegistryMemberForLogging(value_name)
            << " under key: " << FormatNtRegistryMemberForLogging(key);

  ::CloseHandle(registry_handle);

  return status == STATUS_SUCCESS;
}

bool DefaultShouldValueBeNormalized(const String16EmbeddedNulls& key,
                                    const String16EmbeddedNulls& value_name) {
  return (base::EqualsCaseInsensitiveASCII(
              key.CastAsStringPiece16(), chrome_cleaner::kAppInitDllsKeyPath) ||
          base::EqualsCaseInsensitiveASCII(key.CastAsStringPiece16(),
                                           L"\\REGISTRY\\MACHINE\\SOFTWARE\\WOW"
                                           L"6432Node\\Microsoft\\Windows "
                                           L"NT\\CurrentVersion\\Windows")) &&
         base::EqualsCaseInsensitiveASCII(
             value_name.CastAsStringPiece16(),
             chrome_cleaner::kAppInitDllsValueName);
}

bool SandboxNtChangeRegistryValue(
    const String16EmbeddedNulls& key,
    const String16EmbeddedNulls& value_name,
    const String16EmbeddedNulls& new_value,
    const ShouldNormalizeRegistryValue& should_normalize_callback) {
  // Input checks.
  // TODO(joenotcharles): Implmement additional sanity checks to ensure that the
  // new value given by |new_value| is some kind of substring of the original
  // value.
  // TODO(joenotcharles): Add some sanity checks from the old RegistryRemover.
  NtRegistryParamError param_error = ValidateNtRegistryKey(key);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue called with invalid key: "
               << param_error
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  // |value_name| can be empty to change the default value.
  param_error = ValidateNtRegistryNullTerminatedParam(value_name);
  if (param_error != NtRegistryParamError::None &&
      param_error != NtRegistryParamError::ZeroLength) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue called with invalid "
               << "value_name: " << FormatNtRegistryMemberForLogging(value_name)
               << " under key: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  // |new_value| does not need to be NULL-terminated since it may be binary
  // data instead of a string.
  param_error = ValidateNtRegistryValue(new_value);
  if (param_error != NtRegistryParamError::None) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue called with invalid new_value: "
               << param_error
               << " under key: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  // Open a handle to the key.
  HANDLE registry_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      NativeOpenKey(/*parent_key=*/NULL, key, KEY_ALL_ACCESS, &registry_handle);
  if (status != STATUS_SUCCESS || registry_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue: Call to NtOpenKey failed. "
               << "Returned: " << status
               << ", path: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  // Make sure the handle gets cleaned up.
  base::ScopedClosureRunner native_close_handle(
      base::BindOnce(base::IgnoreResult(&::CloseHandle), registry_handle));

  DWORD value_type = 0;
  String16EmbeddedNulls current_value;
  if (!NativeQueryValueKey(registry_handle, value_name, &value_type,
                           &current_value)) {
    LOG(ERROR) << "Failed to read registry value type from key "
               << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  if (value_type != REG_DWORD && value_type != REG_QWORD &&
      value_type != REG_SZ && value_type != REG_EXPAND_SZ &&
      value_type != REG_MULTI_SZ) {
    LOG(ERROR) << "Attempt to modify unsupported registry data type: "
               << value_type
               << " under key: " << FormatNtRegistryMemberForLogging(key);
    return false;
  }

  // Require callers to normalize AppInit_dlls to only use comma separators, no
  // spaces.
  if (should_normalize_callback.Run(key, value_name))
    NormalizeValue(&current_value);

  if (!ValidateRegistryValueChange(current_value, new_value)) {
    LOG(ERROR) << "Invalid new value in NtChangeRegistryValue. Not a subset of "
                  "previous value. Key: '"
               << FormatNtRegistryMemberForLogging(key) << "', value name: '"
               << FormatNtRegistryMemberForLogging(value_name)
               << "'. Current: '"
               << FormatNtRegistryMemberForLogging(current_value) << "', new: '"
               << FormatNtRegistryMemberForLogging(new_value) << "'";
    return false;
  }

  status =
      NativeSetValueKey(registry_handle, value_name, value_type, new_value);
  if (status != STATUS_SUCCESS) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue: Call to NtSetValueKey failed. "
               << "Returned: " << status
               << ", path: " << FormatNtRegistryMemberForLogging(key);
  }

  LOG(INFO) << "Successfully changed registry value, value_name: "
            << FormatNtRegistryMemberForLogging(value_name)
            << " under key: " << FormatNtRegistryMemberForLogging(key)
            << ", to " << FormatNtRegistryMemberForLogging(new_value);

  return status == STATUS_SUCCESS;
}

bool SandboxDeleteService(const base::string16& name) {
  if (name.empty()) {
    LOG(ERROR) << "Sandbox called DeleteService with empty name.";
    return false;
  }

  // https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-createservicea?redirectedfrom=MSDN
  // says that the maximum service name length is 256 characters.
  if (name.size() > 256) {
    LOG(ERROR) << "Sandbox called DeleteService with a long string (length "
               << name.size() << ")";
    return false;
  }

  // Attempt to stop the service, but don't let failure to stop the service
  // prevent deletion.
  chrome_cleaner::StopService(name.c_str());

  if (!chrome_cleaner::DeleteService(name.c_str()))
    return false;

  // Wait for the service to be deleted, but don't treat it as an error if it
  // isn't deleted right now, since something else could be holding a handle to
  // the service preventing it's deletion.
  chrome_cleaner::WaitForServiceDeleted(name.c_str());

  return true;
}

bool SandboxDeleteTask(const base::string16& name) {
  // TODO(joenotcharles): Add some sanity checks.
  std::unique_ptr<chrome_cleaner::TaskScheduler> task_scheduler(
      chrome_cleaner::TaskScheduler::CreateInstance());
  return task_scheduler->DeleteTask(name.c_str());
}

TerminateProcessResult SandboxTerminateProcess(uint32_t process_id) {
  if (process_id == ::GetCurrentProcessId()) {
    LOG(ERROR) << "Sandbox attempted to terminate the broker process.";
    return TerminateProcessResult::kDenied;
  }

  base::win::ScopedHandle handle_to_kill(::OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, process_id));
  if (!handle_to_kill.IsValid()) {
    PLOG(ERROR) << "Failed to open handle to process id " << process_id
                << " in TerminateProcess.";
    return TerminateProcessResult::kFailed;
  }

  base::string16 exec_path;
  base::string16 sanitized_exec_path(L"<unknown>");
  if (chrome_cleaner::GetProcessExecutablePath(handle_to_kill.Get(),
                                               &exec_path)) {
    sanitized_exec_path =
        chrome_cleaner::SanitizePath(base::FilePath(exec_path));
  } else {
    LOG(ERROR) << "Failed to get executable path for process id " << process_id
               << " in TerminateProcess.";
  }

  // Compute the digest for the file corresponding to the process.
  std::string sha256("<unknown>");
  if (!chrome_cleaner::ComputeSHA256DigestOfPath(base::FilePath(exec_path),
                                                 &sha256)) {
    LOG(ERROR) << "Unable to compute digest SHA256 for: '"
               << sanitized_exec_path << "'.";
  }

  // Do not allow Chrome to be terminated. During pre-reboot cleanup,
  // terminating Chrome would give users a very bad experience since their
  // Chrome would simply die without any indication to the user. During a
  // post-reboot run, Chrome should not need to be killed since presumably
  // required file deletions should have happened already.
  base::FilePath chrome_exe_path;
  if (chrome_cleaner::RetrieveChromeExePathFromCommandLine(&chrome_exe_path) &&
      chrome_cleaner::PathEqual(chrome_exe_path, base::FilePath(exec_path))) {
    LOG(INFO)
        << "TerminateProcess did not terminate Chrome process with process id "
        << process_id << ", executable path '"
        << chrome_cleaner::SanitizePath(chrome_exe_path) << "', digest '"
        << sha256 << "'.";
    return TerminateProcessResult::kDenied;
  }

  // Kill the process with a failure exit code of 1, just like the task manager
  // does.
  if (::TerminateProcess(handle_to_kill.Get(), 1) == FALSE) {
    PLOG(ERROR) << "TerminateProcess failed for process id " << process_id
                << " with executable path '" << sanitized_exec_path
                << "', digest '" << sha256 << "'.";
    return TerminateProcessResult::kFailed;
  }

  LOG(INFO) << "Successfully terminated process with process id " << process_id
            << ", executable path '" << sanitized_exec_path << "', digest '"
            << sha256 << "'.";
  return TerminateProcessResult::kSuccess;
}

}  // namespace chrome_cleaner_sandbox
