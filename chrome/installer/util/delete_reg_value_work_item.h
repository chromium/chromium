// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_

#include <windows.h>

#include <string>

#include "chrome/installer/util/work_item.h"

// A WorkItem subclass that deletes a registry value with REG_SZ, REG_DWORD, or
// REG_QWORD type at the specified path. The value is only deleted if the target
// key exists.
class DeleteRegValueWorkItem : public WorkItem {
 public:
  ~DeleteRegValueWorkItem() override;

 private:
  friend class WorkItem;

  enum DeletionStatus {
    // The status before Do is called.
    DELETE_VALUE,
    // One possible outcome after Do(). Value is deleted.
    VALUE_DELETED,
    // One possible outcome after Do(). Value is not found.
    VALUE_NOT_FOUND,
    // The status after Do() and Rollback() is called.
    VALUE_ROLLED_BACK,
    // Another possible outcome after Do() (when there is an error).
    VALUE_UNCHANGED
  };

  DeleteRegValueWorkItem(HKEY predefined_root,
                         const std::wstring& key_path,
                         REGSAM wow64_acccess,
                         const std::wstring& value_name);

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

  // Whether to force 32-bit or 64-bit view of the target key.
  REGSAM wow64_access_;

  DeletionStatus status_;

  // Previous value.
  DWORD previous_type_;
  std::string previous_value_;
};

#endif  // CHROME_INSTALLER_UTIL_DELETE_REG_VALUE_WORK_ITEM_H_
