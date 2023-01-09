// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H_

#include <windows.h>

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that sets a registry value with REG_SZ, REG_DWORD, or
// REG_QWORD type at the specified path. The value is only set if the target key
// exists.
class SetRegValueWorkItem : public WorkItem {
 public:
  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      const std::wstring& value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      DWORD value_data,
                      bool overwrite);

  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      int64_t value_data,
                      bool overwrite);

  // Implies |overwrite_| and TYPE_SZ for now.
  SetRegValueWorkItem(HKEY predefined_root,
                      const std::wstring& key_path,
                      REGSAM wow64_access,
                      const std::wstring& value_name,
                      GetValueFromExistingCallback get_value_callback);

  ~SetRegValueWorkItem() override;

 private:
  enum SettingStatus {
    // The status before Do is called.
    SET_VALUE,
    // One possible outcome after Do(). A new value is created under the key.
    NEW_VALUE_CREATED,
    // One possible outcome after Do(). The previous value under the key has
    // been overwritten.
    VALUE_OVERWRITTEN,
    // One possible outcome after Do(). No change is applied, either
    // because we are not allowed to overwrite the previous value, or due to
    // some errors like the key does not exist.
    VALUE_UNCHANGED,
    // The status after Do and Rollback is called.
    VALUE_ROLL_BACK
  };

  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Root key of the target key under which the value is set. The root key can
  // only be one of the predefined keys on Windows.
  HKEY predefined_root_;

  // Path of the target key under which the value is set.
  std::wstring key_path_;

  // Name of the value to be set.
  std::wstring value_name_;

  // If this is set, it will be used to get the desired value to be set based on
  // the existing value in the registry.
  GetValueFromExistingCallback get_value_callback_;

  // Whether to overwrite the existing value under the target key.
  bool overwrite_;

  // Whether to force 32-bit or 64-bit view of the target key.
  REGSAM wow64_access_;

  // Type of data to store
  DWORD type_;
  std::vector<uint8_t> value_;
  DWORD previous_type_;
  std::vector<uint8_t> previous_value_;

  SettingStatus status_;
};

#endif  // CHROME_INSTALLER_UTIL_SET_REG_VALUE_WORK_ITEM_H_
