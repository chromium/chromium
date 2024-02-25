// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

using ProcessActiveBitUnderKeyCallback =
    base::FunctionRef<bool(HKEY, const std::wstring&)>;

constexpr wchar_t kDidRun[] = L"dr";

bool GetActiveBitUnderKey(HKEY rootkey, const std::wstring& key_name) {
  base::win::RegKey key;
  if (key.Open(rootkey, key_name.c_str(), Wow6432(KEY_QUERY_VALUE)) ==
      ERROR_SUCCESS) {
    // We support both string and DWORD formats for backward compatibility.
    std::wstring value;
    if ((key.ReadValue(kDidRun, &value) == ERROR_SUCCESS) && (value == L"1")) {
      return true;
    }

    DWORD value_dw = 0;
    if ((key.ReadValueDW(kDidRun, &value_dw) == ERROR_SUCCESS) &&
        (value_dw == 1)) {
      return true;
    }
  }
  return false;
}

// Always returns false to avoid the short circuit in ProcessActiveBit and
// the early return in ProcessSystemActiveBit.
bool ClearActiveBitUnderKey(HKEY rootkey, const std::wstring& key_name) {
  base::win::RegKey key;
  if (key.Open(rootkey, key_name.c_str(),
               Wow6432(KEY_QUERY_VALUE | KEY_SET_VALUE)) != ERROR_SUCCESS) {
    VLOG(3) << "Failed to open activity key with write for " << key_name;
    return false;
  }

  if (!key.HasValue(kDidRun)) {
    return false;
  }

  // We always clear the value as a string "0".
  const LONG result = key.WriteValue(kDidRun, L"0");
  VLOG_IF(2, result) << "Failed to clear activity key for " << key_name << ": "
                     << result;
  return false;
}

bool ProcessActiveBit(ProcessActiveBitUnderKeyCallback callback,
                      HKEY rootkey,
                      const std::wstring& sid,
                      const std::string& id) {
  const std::wstring rootkey_suffix =
      (rootkey == HKEY_USERS) ? base::StrCat({sid, L"\\"}) : L"";
  const bool process_success = callback(
      rootkey, base::StrCat({rootkey_suffix, GetAppClientStateKey(id)}));

  // For Google Toolbar and similar apps that run at low integrity, we need to
  // also look at the low integrity IE key. Note that we cannot use the
  // IEGetWriteableHKCU function since this function assumes that we are
  // running with the user's credentials. The path is as follows:
  // USER_REG_VISTA_LOW_INTEGRITY_HKCU\\SID
  // \\GOOPDATE_REG_RELATIVE_CLIENT_STATE\\app_guid
  const std::wstring low_integrity_key_name =
      base::StrCat({rootkey_suffix, USER_REG_VISTA_LOW_INTEGRITY_HKCU, L"\\",
                    sid, L"\\", GetAppClientStateKey(id)});

  return callback(rootkey, low_integrity_key_name) || process_success;
}

bool ProcessUserActiveBit(ProcessActiveBitUnderKeyCallback callback,
                          const std::string& id) {
  // Clear the active bit under HKCU.
  std::wstring sid;
  const HRESULT hr = GetProcessUser(nullptr, nullptr, &sid);
  if (FAILED(hr)) {
    VLOG(2) << "Failed to GetProcessUser " << hr;
    return false;
  }

  return ProcessActiveBit(callback, HKEY_CURRENT_USER, sid, id);
}

bool ProcessSystemActiveBit(ProcessActiveBitUnderKeyCallback callback,
                            const std::string& id) {
  // Clear the active bit under each user in HKU\<sid>.
  for (base::win::RegistryKeyIterator it(HKEY_USERS, L"", KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring sid = it.Name();
    if (ProcessActiveBit(callback, HKEY_USERS, sid, id)) {
      return true;
    }
  }

  return false;
}

bool GetUserActiveBit(const std::string& id) {
  // Read the active bit under HKCU.
  return ProcessUserActiveBit(&GetActiveBitUnderKey, id);
}

void ClearUserActiveBit(const std::string& id) {
  // Clear the active bit under HKCU.
  ProcessUserActiveBit(&ClearActiveBitUnderKey, id);
}

bool GetSystemActiveBit(const std::string& id) {
  // Read the active bit under each user in HKU\<sid>.
  return ProcessSystemActiveBit(&GetActiveBitUnderKey, id);
}

void ClearSystemActiveBit(const std::string& id) {
  // Clear the active bit under each user in HKU\<sid>.
  ProcessSystemActiveBit(&ClearActiveBitUnderKey, id);
}

}  // namespace

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser:
      return GetUserActiveBit(id);
    case UpdaterScope::kSystem:
      return GetSystemActiveBit(id);
  }
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser:
      ClearUserActiveBit(id);
      break;
    case UpdaterScope::kSystem:
      ClearSystemActiveBit(id);
      break;
  }
}

}  // namespace updater
