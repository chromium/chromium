// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/set_reg_value_work_item.h"

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace {

// Transforms |str_value| into the byte-by-byte representation of its underlying
// string, stores the result in |binary_data|.
void StringToBinaryData(const std::wstring& str_value,
                        std::vector<uint8_t>* binary_data) {
  DCHECK(binary_data);
  const uint8_t* data = reinterpret_cast<const uint8_t*>(str_value.c_str());
  binary_data->assign(data, data + (str_value.length() + 1) * sizeof(wchar_t));
}

// Transforms |binary_data| into its wstring representation (assuming
// |binary_data| is a sequence of wchar_t's).
void BinaryDataToString(const std::vector<uint8_t>& binary_data,
                        std::wstring* str_value) {
  DCHECK(str_value);
  if (binary_data.size() < sizeof(wchar_t)) {
    str_value->clear();
    return;
  }

  str_value->assign(reinterpret_cast<const wchar_t*>(&binary_data[0]),
                    binary_data.size() / sizeof(wchar_t));

  // Trim off a trailing string terminator, if present.
  if (!str_value->empty()) {
    auto iter = str_value->end();
    if (*--iter == L'\0')
      str_value->erase(iter);
  }
}

}  // namespace

SetRegValueWorkItem::~SetRegValueWorkItem() {}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         REGSAM wow64_access,
                                         const std::wstring& value_name,
                                         const std::wstring& value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      wow64_access_(wow64_access),
      type_(REG_SZ),
      previous_type_(0),
      status_(SET_VALUE) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
  StringToBinaryData(value_data, &value_);
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         REGSAM wow64_access,
                                         const std::wstring& value_name,
                                         DWORD value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      wow64_access_(wow64_access),
      type_(REG_DWORD),
      previous_type_(0),
      status_(SET_VALUE) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value_data);
  value_.assign(data, data + sizeof(value_data));
}

SetRegValueWorkItem::SetRegValueWorkItem(HKEY predefined_root,
                                         const std::wstring& key_path,
                                         REGSAM wow64_access,
                                         const std::wstring& value_name,
                                         int64_t value_data,
                                         bool overwrite)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      overwrite_(overwrite),
      wow64_access_(wow64_access),
      type_(REG_QWORD),
      previous_type_(0),
      status_(SET_VALUE) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value_data);
  value_.assign(data, data + sizeof(value_data));
}

SetRegValueWorkItem::SetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    GetValueFromExistingCallback get_value_callback)
    : predefined_root_(predefined_root),
      key_path_(key_path),
      value_name_(value_name),
      get_value_callback_(std::move(get_value_callback)),
      overwrite_(true),
      wow64_access_(wow64_access),
      type_(REG_SZ),
      previous_type_(0),
      status_(SET_VALUE) {
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);
  // Nothing to do, |get_value_callback| will fill |value_| later.
}

bool SetRegValueWorkItem::DoImpl() {
  DCHECK_EQ(SET_VALUE, status_);

  status_ = VALUE_UNCHANGED;

  base::win::RegKey key;
  LONG result = key.Open(predefined_root_, key_path_.c_str(),
                         KEY_READ | KEY_SET_VALUE | wow64_access_);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "can not open " << key_path_ << " error: " << result;
    return false;
  }

  DWORD type = 0;
  DWORD size = 0;
  result = key.ReadValue(value_name_.c_str(), nullptr, &size, &type);
  // If the value exists but we don't want to overwrite then there's
  // nothing more to do.
  if ((result != ERROR_FILE_NOT_FOUND) && !overwrite_) {
    return true;
  }

  // If there's something to be saved, save it.
  if (result == ERROR_SUCCESS && (rollback_enabled() || get_value_callback_)) {
    if (!size) {
      previous_type_ = type;
    } else {
      // TODO(crbug.com/40706274): Remove after bug is resolved.
      DEBUG_ALIAS_FOR_CSTR(key_path_copy, base::WideToUTF8(key_path_).c_str(),
                           255);
      DEBUG_ALIAS_FOR_CSTR(value_name_copy,
                           base::WideToUTF8(value_name_).c_str(), 200);
      base::debug::Alias(&size);
      base::debug::Alias(&type);
      previous_value_.resize(size);
      result = key.ReadValue(value_name_.c_str(), &previous_value_[0], &size,
                             &previous_type_);
      if (result != ERROR_SUCCESS) {
        previous_value_.clear();
        VLOG(1) << "Failed to save original value. Error: " << result;
      }
    }
  }

  if (!get_value_callback_.is_null()) {
    // Although this could be made more generic, for now this assumes the
    // |type_| of |value_| is REG_SZ.
    DCHECK_EQ(static_cast<DWORD>(REG_SZ), type_);

    // Fill |previous_value_str| with the wstring representation of the binary
    // data in |previous_value_| as long as it's of type REG_SZ (leave it empty
    // otherwise).
    std::wstring previous_value_str;
    if (previous_type_ == REG_SZ)
      BinaryDataToString(previous_value_, &previous_value_str);

    StringToBinaryData(std::move(get_value_callback_).Run(previous_value_str),
                       &value_);
  }

  result = key.WriteValue(value_name_.c_str(), &value_[0],
                          static_cast<DWORD>(value_.size()), type_);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "Failed to write value " << key_path_ << " error: " << result;
    return false;
  }

  if (VLOG_IS_ON(1)) {
    if (type_ == REG_SZ) {
      std::wstring value_str;
      BinaryDataToString(value_, &value_str);
      VLOG(1) << "Successfully wrote value " << value_str << " into "
              << key_path_;

    } else {
      VLOG(1) << "Successfully wrote into " << key_path_;
    }
  }

  status_ = previous_type_ ? VALUE_OVERWRITTEN : NEW_VALUE_CREATED;
  return true;
}

void SetRegValueWorkItem::RollbackImpl() {
  DCHECK_NE(SET_VALUE, status_);
  DCHECK_NE(VALUE_ROLL_BACK, status_);

  if (status_ == VALUE_UNCHANGED) {
    status_ = VALUE_ROLL_BACK;
    VLOG(1) << "rollback: setting unchanged, nothing to do";
    return;
  }

  base::win::RegKey key;
  LONG result = key.Open(predefined_root_, key_path_.c_str(),
                         KEY_SET_VALUE | wow64_access_);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << "rollback: can not open " << key_path_ << " error: " << result;
    return;
  }

  if (status_ == NEW_VALUE_CREATED) {
    result = key.DeleteValue(value_name_.c_str());
    VLOG(1) << "rollback: deleting " << value_name_ << " error: " << result;
  } else if (status_ == VALUE_OVERWRITTEN) {
    const unsigned char* previous_value =
        previous_value_.empty() ? nullptr : &previous_value_[0];
    result = key.WriteValue(value_name_.c_str(), previous_value,
                            static_cast<DWORD>(previous_value_.size()),
                            previous_type_);
    VLOG(1) << "rollback: restoring " << value_name_ << " error: " << result;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  status_ = VALUE_ROLL_BACK;
}
