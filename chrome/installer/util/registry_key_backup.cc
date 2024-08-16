// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/registry_key_backup.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/win/registry.h"

using base::win::RegKey;

namespace {

const REGSAM kKeyReadNoNotify = (KEY_READ) & ~(KEY_NOTIFY);

// A container for a registry value.
class ValueData {
 public:
  ValueData();
  ~ValueData();

  // Initializes this object with a name (the first |name_size| characters in
  // |name_buffer|, |type|, and data (the first |data_size| bytes in |data|).
  void Initialize(const wchar_t* name_buffer,
                  DWORD name_size,
                  DWORD type,
                  const uint8_t* data,
                  DWORD data_size);

  // The possibly empty name of this value.
  const std::wstring& name_str() const { return name_; }

  // The name of this value, or nullptr for the default (unnamed) value.
  const wchar_t* name() const {
    return name_.empty() ? nullptr : name_.c_str();
  }

  // The type of this value.
  DWORD type() const { return type_; }

  // A pointer to a buffer of |data_len()| bytes containing the value's data,
  // or nullptr if the value has no data.
  const uint8_t* data() const { return data_.empty() ? nullptr : &data_[0]; }

  // The size, in bytes, of the value's data.
  DWORD data_len() const { return static_cast<DWORD>(data_.size()); }

 private:
  // This value's name, or the empty string if this is the default (unnamed)
  // value.
  std::wstring name_;
  // This value's data.
  std::vector<uint8_t> data_;
  // This value's type (e.g., REG_DWORD, REG_SZ, REG_QWORD, etc).
  DWORD type_;

  // Copy constructible and assignable for use in STL containers.
};

}  // namespace

// A container for a registry key, its values, and its subkeys.
class RegistryKeyBackup::KeyData {
 public:
  KeyData();
  ~KeyData();

  // Initializes this object by reading the values and subkeys of |key|.
  // Security descriptors are not backed up.  Returns true if the operation was
  // successful; false otherwise, in which case the state of this object is not
  // modified.
  bool Initialize(const RegKey& key);

  // Writes the contents of this object to |key|, which must have been opened
  // with at least REG_SET_VALUE and KEY_CREATE_SUB_KEY access rights.  Returns
  // true if the operation was successful; false otherwise, in which case the
  // contents of |key| may have been modified.
  bool WriteTo(RegKey* key) const;

 private:
  // The values of this key.
  std::vector<ValueData> values_;
  // Map of subkey names to the corresponding KeyData.
  std::map<std::wstring, KeyData> subkeys_;

  // Copy constructible and assignable for use in STL containers.
};

ValueData::ValueData() : type_(REG_NONE) {}

ValueData::~ValueData() {}

void ValueData::Initialize(const wchar_t* name_buffer,
                           DWORD name_size,
                           DWORD type,
                           const uint8_t* data,
                           DWORD data_size) {
  name_.assign(name_buffer, name_size);
  type_ = type;
  data_.assign(data, data + data_size);
}

RegistryKeyBackup::KeyData::KeyData() {}

RegistryKeyBackup::KeyData::~KeyData() {}

bool RegistryKeyBackup::KeyData::Initialize(const RegKey& key) {
  std::vector<ValueData> values;
  std::map<std::wstring, KeyData> subkeys;

  DWORD num_subkeys = 0;
  DWORD max_subkey_name_len = 0;
  DWORD num_values = 0;
  DWORD max_value_name_len = 0;
  DWORD max_value_len = 0;
  LONG result =
      RegQueryInfoKey(key.Handle(), nullptr, nullptr, nullptr, &num_subkeys,
                      &max_subkey_name_len, nullptr, &num_values,
                      &max_value_name_len, &max_value_len, nullptr, nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed getting info of key to backup, result: " << result;
    return false;
  }
  DWORD max_name_len = std::max(max_subkey_name_len, max_value_name_len) + 1;
  std::vector<wchar_t> name_buffer(max_name_len);

  // Backup the values.
  if (num_values != 0) {
    values.reserve(num_values);
    std::vector<uint8_t> value_buffer(max_value_len != 0 ? max_value_len : 1);
    DWORD name_size = 0;
    DWORD value_type = REG_NONE;
    DWORD value_size = 0;

    for (DWORD i = 0; i < num_values;) {
      name_size = static_cast<DWORD>(name_buffer.size());
      value_size = static_cast<DWORD>(value_buffer.size());
      result =
          RegEnumValue(key.Handle(), i, &name_buffer[0], &name_size, nullptr,
                       &value_type, &value_buffer[0], &value_size);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          num_values = i;
          break;
        case ERROR_SUCCESS:
          values.push_back(ValueData());
          values.back().Initialize(&name_buffer[0], name_size, value_type,
                                   &value_buffer[0], value_size);
          ++i;
          break;
        case ERROR_MORE_DATA:
          if (value_size > value_buffer.size())
            value_buffer.resize(value_size);
          // |name_size| does not include space for the string terminator.
          if (name_size + 1 > name_buffer.size())
            name_buffer.resize(name_size + 1);
          break;
        default:
          LOG(ERROR) << "Failed backing up value " << i
                     << ", result: " << result;
          return false;
      }
    }
    DLOG_IF(WARNING, RegEnumValue(key.Handle(), num_values, &name_buffer[0],
                                  &name_size, nullptr, &value_type, nullptr,
                                  nullptr) != ERROR_NO_MORE_ITEMS)
        << "Concurrent modifications to registry key during backup operation.";
  }

  // Backup the subkeys.
  if (num_subkeys != 0) {
    DWORD name_size = 0;

    // Get the names of them.
    for (DWORD i = 0; i < num_subkeys;) {
      name_size = static_cast<DWORD>(name_buffer.size());
      result = RegEnumKeyEx(key.Handle(), i, &name_buffer[0], &name_size,
                            nullptr, nullptr, nullptr, nullptr);
      switch (result) {
        case ERROR_NO_MORE_ITEMS:
          num_subkeys = i;
          break;
        case ERROR_SUCCESS:
          subkeys.insert(std::make_pair(&name_buffer[0], KeyData()));
          ++i;
          break;
        case ERROR_MORE_DATA:
          name_buffer.resize(name_size + 1);
          break;
        default:
          LOG(ERROR) << "Failed getting name of subkey " << i
                     << " for backup, result: " << result;
          return false;
      }
    }
    DLOG_IF(WARNING, RegEnumKeyEx(key.Handle(), num_subkeys, nullptr,
                                  &name_size, nullptr, nullptr, nullptr,
                                  nullptr) != ERROR_NO_MORE_ITEMS)
        << "Concurrent modifications to registry key during backup operation.";

    // Get their values.
    RegKey subkey;
    for (std::map<std::wstring, KeyData>::iterator it = subkeys.begin();
         it != subkeys.end(); ++it) {
      result = subkey.Open(key.Handle(), it->first.c_str(), kKeyReadNoNotify);
      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Failed opening subkey \"" << it->first
                   << "\" for backup, result: " << result;
        return false;
      }
      if (!it->second.Initialize(subkey)) {
        LOG(ERROR) << "Failed backing up subkey \"" << it->first << "\"";
        return false;
      }
    }
  }

  values_.swap(values);
  subkeys_.swap(subkeys);

  return true;
}

bool RegistryKeyBackup::KeyData::WriteTo(RegKey* key) const {
  DCHECK(key);

  LONG result = ERROR_SUCCESS;

  // Write the values.
  for (std::vector<ValueData>::const_iterator it = values_.begin();
       it != values_.end(); ++it) {
    const ValueData& value = *it;
    result = RegSetValueEx(key->Handle(), value.name(), 0, value.type(),
                           value.data(), value.data_len());
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed writing value \"" << value.name_str()
                 << "\", result: " << result;
      return false;
    }
  }

  // Write the subkeys.
  RegKey subkey;
  for (std::map<std::wstring, KeyData>::const_iterator it = subkeys_.begin();
       it != subkeys_.end(); ++it) {
    const std::wstring& name = it->first;

    result = subkey.Create(key->Handle(), name.c_str(), KEY_WRITE);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed creating subkey \"" << name
                 << "\", result: " << result;
      return false;
    }
    if (!it->second.WriteTo(&subkey)) {
      LOG(ERROR) << "Failed writing subkey \"" << name
                 << "\", result: " << result;
      return false;
    }
  }

  return true;
}

RegistryKeyBackup::RegistryKeyBackup() {}

RegistryKeyBackup::~RegistryKeyBackup() {}

bool RegistryKeyBackup::Initialize(HKEY root,
                                   const wchar_t* key_path,
                                   REGSAM wow64_access) {
  DCHECK(key_path);
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);

  RegKey key;
  std::unique_ptr<KeyData> key_data;

  // Does the key exist?
  LONG result = key.Open(root, key_path, kKeyReadNoNotify | wow64_access);
  if (result == ERROR_SUCCESS) {
    key_data = std::make_unique<KeyData>();
    if (!key_data->Initialize(key)) {
      LOG(ERROR) << "Failed to backup key at " << key_path;
      return false;
    }
  } else if (result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to open key at " << key_path
               << " to create backup, result: " << result;
    return false;
  }

  key_data_.swap(key_data);
  return true;
}

bool RegistryKeyBackup::WriteTo(HKEY root,
                                const wchar_t* key_path,
                                REGSAM wow64_access) const {
  DCHECK(key_path);
  DCHECK(wow64_access == 0 || wow64_access == KEY_WOW64_32KEY ||
         wow64_access == KEY_WOW64_64KEY);

  bool success = false;

  if (key_data_) {
    RegKey dest_key;
    LONG result = dest_key.Create(root, key_path, KEY_WRITE | wow64_access);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to create destination key at " << key_path
                 << " to write backup, result: " << result;
    } else {
      success = key_data_->WriteTo(&dest_key);
      LOG_IF(ERROR, !success) << "Failed to write key data.";
    }
  } else {
    success = true;
  }

  return success;
}
