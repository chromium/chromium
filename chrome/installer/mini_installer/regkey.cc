// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/regkey.h"

#include "build/branding_buildflags.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/mini_string.h"

namespace mini_installer {

LONG RegKey::Open(HKEY key, const wchar_t* sub_key, REGSAM access) {
  Close();
  return ::RegOpenKeyEx(key, sub_key, NULL, access, &key_);
}

LONG RegKey::ReadSZValue(const wchar_t* value_name,
                         wchar_t* value,
                         size_t value_size) const {
  DWORD type = 0;
  DWORD byte_length = static_cast<DWORD>(value_size * sizeof(wchar_t));
  LONG result = ::RegQueryValueEx(key_, value_name, NULL, &type,
                                  reinterpret_cast<BYTE*>(value),
                                  &byte_length);
  if (result == ERROR_SUCCESS) {
    if (type != REG_SZ) {
      result = ERROR_NOT_SUPPORTED;
    } else if (byte_length < 2) {
      *value = L'\0';
    } else if (value[byte_length/sizeof(wchar_t) - 1] != L'\0') {
      if ((byte_length / sizeof(wchar_t)) < value_size)
        value[byte_length / sizeof(wchar_t)] = L'\0';
      else
        result = ERROR_MORE_DATA;
    }
  }
  return result;
}

LONG RegKey::ReadDWValue(const wchar_t* value_name, DWORD* value) const {
  DWORD type = 0;
  DWORD byte_length = sizeof(*value);
  LONG result = ::RegQueryValueEx(key_, value_name, NULL, &type,
                                  reinterpret_cast<BYTE*>(value),
                                  &byte_length);
  if (result == ERROR_SUCCESS) {
    if (type != REG_DWORD) {
      result = ERROR_NOT_SUPPORTED;
    } else if (byte_length != sizeof(*value)) {
      result = ERROR_NO_DATA;
    }
  }
  return result;
}

LONG RegKey::WriteSZValue(const wchar_t* value_name, const wchar_t* value) {
  return ::RegSetValueEx(key_, value_name, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(value),
                         (lstrlen(value) + 1) * sizeof(wchar_t));
}

LONG RegKey::WriteDWValue(const wchar_t* value_name, DWORD value) {
  return ::RegSetValueEx(key_, value_name, 0, REG_DWORD,
                         reinterpret_cast<const BYTE*>(&value),
                         sizeof(value));
}

void RegKey::Close() {
  if (key_ != NULL) {
    ::RegCloseKey(key_);
    key_ = NULL;
  }
}

// static
bool RegKey::ReadSZValue(HKEY root_key, const wchar_t *sub_key,
                         const wchar_t *value_name, wchar_t *value,
                         size_t size) {
  RegKey key;
  return (key.Open(root_key, sub_key, KEY_QUERY_VALUE) == ERROR_SUCCESS &&
          key.ReadSZValue(value_name, value, size) == ERROR_SUCCESS);
}

// Opens the Google Update Clients key for a product.
LONG OpenClientsKey(HKEY root_key,
                    const wchar_t* app_guid,
                    REGSAM access,
                    RegKey* key) {
  StackString<MAX_PATH> clients_key;
  if (!clients_key.assign(kClientsKeyBase))
    return ERROR_BUFFER_OVERFLOW;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!clients_key.append(app_guid))
    return ERROR_BUFFER_OVERFLOW;
#endif
  return key->Open(root_key, clients_key.get(), access | KEY_WOW64_32KEY);
}

// Opens the Google Update ClientState key for a product.
LONG OpenClientStateKey(HKEY root_key,
                        const wchar_t* app_guid,
                        REGSAM access,
                        RegKey* key) {
  StackString<MAX_PATH> client_state_key;
  if (!client_state_key.assign(kClientStateKeyBase))
    return ERROR_BUFFER_OVERFLOW;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!client_state_key.append(app_guid))
    return ERROR_BUFFER_OVERFLOW;
#endif
  return key->Open(root_key, client_state_key.get(), access | KEY_WOW64_32KEY);
}

}  // namespace mini_installer
