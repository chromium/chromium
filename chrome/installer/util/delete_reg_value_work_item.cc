// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_reg_value_work_item.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"

using base::win::RegKey;

DeleteRegValueWorkItem::DeleteRegValueWorkItem(HKEY predefined_root,
                                               const std::wstring& key_path,
                                               REGSAM wow64_access,
                                               const std::wstring& value_name)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      wow64_access_(wow64_access),
      status_(DELETE_VALUE),
      previous_type_(0) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
}

DeleteRegValueWorkItem::~DeleteRegValueWorkItem() {}

bool DeleteRegValueWorkItem::DoImpl() {
  DCHECK_EQ(DELETE_VALUE, status_);

  status_ = VALUE_UNCHANGED;

  RegKey key;
  DWORD type = 0;
  DWORD size = 0;
  LONG result = key.Open(predefined_root_, key_path_.c_str(),
                         KEY_READ | KEY_WRITE | wow64_access_);
  if (result == ERROR_SUCCESS)
    result = key.ReadValue(value_name_.c_str(), nullptr, &size, &type);

  if (result == ERROR_FILE_NOT_FOUND) {
    VLOG(1) << "(delete value) Key: " << key_path_
            << " or Value: " << value_name_ << " does not exist.";
    status_ = VALUE_NOT_FOUND;
    return true;
  }

  if (result == ERROR_SUCCESS) {
    if (!size) {
      previous_type_ = type;
    } else {
      previous_value_.resize(size);
      result = key.ReadValue(value_name_.c_str(), &previous_value_[0], &size,
                             &previous_type_);
      if (result != ERROR_SUCCESS) {
        previous_value_.erase();
        VLOG(1) << "Failed to save original value. Error: " << result;
      }
    }
  }

  result = key.DeleteValue(value_name_.c_str());
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Failed to delete value " << value_name_ << " error: " << result;
    return false;
  }

  status_ = VALUE_DELETED;
  return true;
}

void DeleteRegValueWorkItem::RollbackImpl() {
  DCHECK_NE(DELETE_VALUE, status_);
  DCHECK_NE(VALUE_ROLLED_BACK, status_);

  if (status_ == VALUE_UNCHANGED || status_ == VALUE_NOT_FOUND) {
    status_ = VALUE_ROLLED_BACK;
    VLOG(1) << "rollback: setting unchanged, nothing to do";
    return;
  }

  // At this point only possible state is VALUE_DELETED.
  DCHECK_EQ(VALUE_DELETED, status_);

  RegKey key;
  LONG result = key.Open(predefined_root_, key_path_.c_str(),
                         KEY_READ | KEY_WRITE | wow64_access_);
  if (result == ERROR_SUCCESS) {
    // try to restore the previous value
    DWORD previous_size = static_cast<DWORD>(previous_value_.size());
    const char* previous_value = previous_size ? &previous_value_[0] : nullptr;
    result = key.WriteValue(value_name_.c_str(), previous_value, previous_size,
                            previous_type_);
    VLOG_IF(1, result != ERROR_SUCCESS)
        << "rollback: restoring " << value_name_ << " error: " << result;
  } else {
    VLOG(1) << "can not open " << key_path_ << " error: " << result;
  }
}
