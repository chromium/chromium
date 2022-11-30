// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_REGISTRY_UTIL_H_
#define CHROME_INSTALLER_UTIL_REGISTRY_UTIL_H_

#include <windows.h>

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"

namespace installer {

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool DeleteRegistryKey(HKEY root_key,
                       const std::wstring& key_path,
                       REGSAM wow64_access);

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool DeleteRegistryValue(HKEY reg_root,
                         const std::wstring& key_path,
                         REGSAM wow64_access,
                         const std::wstring& value_name);

// An interface to a predicate function for use by DeleteRegistryKeyIf and
// DeleteRegistryValueIf.
class RegistryValuePredicate {
 public:
  virtual ~RegistryValuePredicate() {}
  virtual bool Evaluate(const std::wstring& value) const = 0;
};

// The result of a conditional delete operation (i.e., DeleteFOOIf).
enum class ConditionalDeleteResult {
  NOT_FOUND,     // The condition was not satisfied.
  DELETED,       // The condition was satisfied and the delete succeeded.
  DELETE_FAILED  // The condition was satisfied but the delete failed.
};

// Deletes the key |key_to_delete_path| under |root_key| iff the value
// |value_name| in the key |key_to_test_path| under |root_key| satisfies
// |predicate|.  |value_name| may be either nullptr or an empty string to test
// the key's default value.
ConditionalDeleteResult DeleteRegistryKeyIf(
    HKEY root_key,
    const std::wstring& key_to_delete_path,
    const std::wstring& key_to_test_path,
    REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate);

// Deletes the value |value_name| in the key |key_path| under |root_key| iff
// its current value satisfies |predicate|.  |value_name| may be either
// nullptr or an empty string to test/delete the key's default value.
ConditionalDeleteResult DeleteRegistryValueIf(
    HKEY root_key,
    const wchar_t* key_path,
    REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate);

// A predicate that performs a case-sensitive string comparison.
class ValueEquals : public RegistryValuePredicate {
 public:
  explicit ValueEquals(const std::wstring& value_to_match)
      : value_to_match_(value_to_match) {}

  ValueEquals(const ValueEquals&) = delete;
  ValueEquals& operator=(const ValueEquals&) = delete;

  bool Evaluate(const std::wstring& value) const override;

 protected:
  std::wstring value_to_match_;
};

// A predicate that compares the program portion of a command line with a
// given file path.  First, the file paths are compared directly.  If they do
// not match, the filesystem is consulted to determine if the paths reference
// the same file.
class ProgramCompare : public RegistryValuePredicate {
 public:
  explicit ProgramCompare(const base::FilePath& path_to_match);

  ProgramCompare(const ProgramCompare&) = delete;
  ProgramCompare& operator=(const ProgramCompare&) = delete;

  ~ProgramCompare() override;
  bool Evaluate(const std::wstring& value) const override;
  bool EvaluatePath(const base::FilePath& path) const;

 protected:
  static bool OpenForInfo(const base::FilePath& path, base::File* file);
  static bool GetInfo(const base::File& file, BY_HANDLE_FILE_INFORMATION* info);

  base::FilePath path_to_match_;
  base::File file_;
  BY_HANDLE_FILE_INFORMATION file_info_;
};  // class ProgramCompare

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_REGISTRY_UTIL_H_
