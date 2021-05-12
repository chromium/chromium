// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity.h"

#include <string>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/constants.h"

namespace updater {
namespace {
constexpr wchar_t kDidRun[] = L"dr";

std::wstring GetAppClientStateKey(const std::string& id) {
  return base::ASCIIToWide(base::StrCat({CLIENT_STATE_KEY, id}));
}

bool GetActiveBitUnderKey(HKEY rootkey, const std::wstring& key_name) {
  base::win::RegKey key;
  if (key.Open(rootkey, key_name.c_str(), KEY_READ | KEY_WOW64_32KEY) ==
      ERROR_SUCCESS) {
    std::wstring value;
    if (key.ReadValue(kDidRun, &value) == ERROR_SUCCESS && value == L"1")
      return true;
  }
  return false;
}

bool GetMachineActiveBit(const std::string& id) {
  // Read the active bit under each user in HKU\<sid>.
  for (base::win::RegistryKeyIterator it(HKEY_USERS, L"", KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    std::wstring user_state_key_name =
        std::wstring(it.Name()).append(L"\\").append(GetAppClientStateKey(id));
    if (GetActiveBitUnderKey(HKEY_USERS, user_state_key_name))
      return true;
  }

  return false;
}

void ClearActiveBitUnderKey(HKEY rootkey, const std::wstring& key_name) {
  base::win::RegKey key;
  if (key.Open(rootkey, key_name.c_str(), KEY_WRITE | KEY_WOW64_32KEY) !=
      ERROR_SUCCESS) {
    VLOG(2) << "Failed to open activity key with write for " << key_name;
    return;
  }

  const LONG result = key.WriteValue(kDidRun, L"0");
  VLOG_IF(2, result) << "Failed to clear activity key for " << key_name << ": "
                     << result;
}

void ClearMachineActiveBit(const std::string& id) {
  // Clear the active bit under each user in HKU\<sid>.
  for (base::win::RegistryKeyIterator it(HKEY_USERS, L"", KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    std::wstring user_state_key_name =
        std::wstring(it.Name()).append(L"\\").append(GetAppClientStateKey(id));
    ClearActiveBitUnderKey(HKEY_USERS, user_state_key_name);
  }
}

}  // namespace

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser:
      // TODO(crbug/1159498): Standardize registry access.
      return GetActiveBitUnderKey(HKEY_CURRENT_USER,
                                  GetAppClientStateKey(id).c_str());

    case UpdaterScope::kSystem:
      return GetMachineActiveBit(id);
  }
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser:
      // TODO(crbug/1159498): Standardize registry access.
      ClearActiveBitUnderKey(HKEY_CURRENT_USER,
                             GetAppClientStateKey(id).c_str());
      break;

    case UpdaterScope::kSystem:
      ClearMachineActiveBit(id);
      break;
  }
}

}  // namespace updater
