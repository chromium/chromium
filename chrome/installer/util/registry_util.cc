// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/registry_util.h"

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"

using base::win::RegKey;

namespace installer {

bool DeleteRegistryKey(HKEY root_key,
                       const std::wstring& key_path,
                       REGSAM wow64_access) {
  VLOG(1) << "Deleting registry key " << key_path;
  RegKey target_key;
  LONG result =
      target_key.Open(root_key, key_path.c_str(), DELETE | wow64_access);

  if (result == ERROR_FILE_NOT_FOUND)
    return true;

  if (result == ERROR_SUCCESS)
    result = target_key.DeleteKey(L"");

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path
               << " error: " << result;
    return false;
  }
  return true;
}

bool DeleteRegistryValue(HKEY reg_root,
                         const std::wstring& key_path,
                         REGSAM wow64_access,
                         const std::wstring& value_name) {
  RegKey key;
  LONG result =
      key.Open(reg_root, key_path.c_str(), KEY_SET_VALUE | wow64_access);
  if (result == ERROR_SUCCESS)
    result = key.DeleteValue(value_name.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name
               << " error: " << result;
    return false;
  }
  return true;
}

ConditionalDeleteResult DeleteRegistryKeyIf(
    HKEY root_key,
    const std::wstring& key_to_delete_path,
    const std::wstring& key_to_test_path,
    const REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  ConditionalDeleteResult delete_result = ConditionalDeleteResult::NOT_FOUND;
  RegKey key;
  std::wstring actual_value;
  if (key.Open(root_key, key_to_test_path.c_str(),
               KEY_QUERY_VALUE | wow64_access) == ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    key.Close();
    delete_result =
        installer::DeleteRegistryKey(root_key, key_to_delete_path, wow64_access)
            ? ConditionalDeleteResult::DELETED
            : ConditionalDeleteResult::DELETE_FAILED;
  }
  return delete_result;
}

// static
ConditionalDeleteResult DeleteRegistryValueIf(
    HKEY root_key,
    const wchar_t* key_path,
    REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  DCHECK(key_path);
  ConditionalDeleteResult delete_result = ConditionalDeleteResult::NOT_FOUND;
  RegKey key;
  std::wstring actual_value;
  if (key.Open(root_key, key_path,
               KEY_QUERY_VALUE | KEY_SET_VALUE | wow64_access) ==
          ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    LONG result = key.DeleteValue(value_name);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete registry value: "
                 << (value_name ? value_name : L"(Default)")
                 << " error: " << result;
      delete_result = ConditionalDeleteResult::DELETE_FAILED;
    } else {
      delete_result = ConditionalDeleteResult::DELETED;
    }
  }
  return delete_result;
}

bool ValueEquals::Evaluate(const std::wstring& value) const {
  return value == value_to_match_;
}

// Open |path| with minimal access to obtain information about it, returning
// true and populating |file| on success.
// static
bool ProgramCompare::OpenForInfo(const base::FilePath& path, base::File* file) {
  DCHECK(file);
  file->Initialize(path,
                   base::File::FLAG_OPEN | base::File::FLAG_WIN_SHARE_DELETE);
  return file->IsValid();
}

// Populate |info| for |file|, returning true on success.
// static
bool ProgramCompare::GetInfo(const base::File& file,
                             BY_HANDLE_FILE_INFORMATION* info) {
  DCHECK(file.IsValid());
  return GetFileInformationByHandle(file.GetPlatformFile(), info) != 0;
}

ProgramCompare::ProgramCompare(const base::FilePath& path_to_match)
    : path_to_match_(path_to_match), file_info_() {
  DCHECK(!path_to_match_.empty());
  if (!OpenForInfo(path_to_match_, &file_)) {
    PLOG(WARNING) << "Failed opening " << path_to_match_.value()
                  << "; falling back to path string comparisons.";
  } else if (!GetInfo(file_, &file_info_)) {
    PLOG(WARNING) << "Failed getting information for " << path_to_match_.value()
                  << "; falling back to path string comparisons.";
    file_.Close();
  }
}

ProgramCompare::~ProgramCompare() {}

bool ProgramCompare::Evaluate(const std::wstring& value) const {
  // Suss out the exe portion of the value, which is expected to be a command
  // line kinda (or exactly) like:
  // "c:\foo\bar\chrome.exe" -- "%1"
  base::FilePath program(base::CommandLine::FromString(value).GetProgram());
  if (program.empty()) {
    LOG(WARNING) << "Failed to parse an executable name from command line: \""
                 << value << "\"";
    return false;
  }

  return EvaluatePath(program);
}

bool ProgramCompare::EvaluatePath(const base::FilePath& path) const {
  // Try the simple thing first: do the paths happen to match?
  if (base::FilePath::CompareEqualIgnoreCase(path_to_match_.value(),
                                             path.value()))
    return true;

  // If the paths don't match and we couldn't open the expected file, we've done
  // our best.
  if (!file_.IsValid())
    return false;

  // Open the program and see if it references the expected file.
  base::File file;
  BY_HANDLE_FILE_INFORMATION info = {};

  return (OpenForInfo(path, &file) && GetInfo(file, &info) &&
          info.dwVolumeSerialNumber == file_info_.dwVolumeSerialNumber &&
          info.nFileIndexHigh == file_info_.nFileIndexHigh &&
          info.nFileIndexLow == file_info_.nFileIndexLow);
}

}  // namespace installer
