// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_REGKEY_H_
#define CHROME_UPDATER_WIN_INSTALLER_REGKEY_H_

#include <stddef.h>
#include <windows.h>

namespace updater {

// A helper class used to manipulate the Windows registry.
class RegKey {
 public:
  RegKey() : key_(nullptr) {}
  ~RegKey() { Close(); }

  // Opens the key named |sub_key| with given |access| rights.  Returns
  // ERROR_SUCCESS or some other error.
  LONG Open(HKEY key, const wchar_t* sub_key, REGSAM access);

  // Returns true if a key is open.
  bool is_valid() const { return key_ != nullptr; }

  // Read a value from the registry into the memory indicated by |value|
  // (of |value_size| wchar_t units).  Returns ERROR_SUCCESS,
  // ERROR_FILE_NOT_FOUND, ERROR_MORE_DATA, or some other error.  |value| is
  // guaranteed to be null-terminated on success.
  LONG ReadSZValue(const wchar_t* value_name,
                   wchar_t* value,
                   size_t value_size) const;
  LONG ReadDWValue(const wchar_t* value_name, DWORD* value) const;

  // Write a value to the registry.  SZ |value| must be null-terminated.
  // Returns ERROR_SUCCESS or an error code.
  LONG WriteSZValue(const wchar_t* value_name, const wchar_t* value);
  LONG WriteDWValue(const wchar_t* value_name, DWORD value);

  // Closes the key if it was open.
  void Close();

  // Helper function to read a value from registry.  Returns true if value
  // is read successfully and stored in parameter value. Returns false
  // otherwise. |size| is measured in wchar_t units.
  static bool ReadSZValue(HKEY root_key,
                          const wchar_t* sub_key,
                          const wchar_t* value_name,
                          wchar_t* value,
                          size_t value_size);

 private:
  RegKey(const RegKey&);
  RegKey& operator=(const RegKey&);

  HKEY key_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_REGKEY_H_
