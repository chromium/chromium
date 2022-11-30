// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/google_update_util.h"

#include <windows.h>

#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"

namespace gcapi_internals {

const wchar_t kChromeRegClientsKey[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kChromeRegClientStateMediumKey[] =
    L"Software\\Google\\Update\\ClientStateMedium\\"
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Mirror the strategy used by GoogleUpdateSettings::GetBrand.
bool GetBrand(std::wstring* value) {
  const HKEY kRoots[] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
  for (HKEY root : kRoots) {
    if (base::win::RegKey(root, kChromeRegClientStateKey,
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY)
            .ReadValue(google_update::kRegBrandField, value) == ERROR_SUCCESS) {
      return true;
    }
  }
  return false;
}

// Mirror the strategy used by GoogleUpdateSettings::ReadExperimentLabels.
bool ReadExperimentLabels(bool system_install,
                          std::wstring* experiment_labels) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const wchar_t* const key_path = system_install
                                      ? kChromeRegClientStateMediumKey
                                      : kChromeRegClientStateKey;
  base::win::RegKey client_state;
  LONG result =
      client_state.Open(root_key, key_path, KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_SUCCESS) {
    result = client_state.ReadValue(google_update::kExperimentLabels,
                                    experiment_labels);
  }
  if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
    experiment_labels->clear();
    return true;
  }
  return result == ERROR_SUCCESS;
}

// Mirror the strategy used by GoogleUpdateSettings::SetExperimentLabels.
bool SetExperimentLabels(bool system_install,
                         const std::wstring& experiment_labels) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  const wchar_t* const key_path = system_install
                                      ? kChromeRegClientStateMediumKey
                                      : kChromeRegClientStateKey;
  base::win::RegKey client_state(root_key, key_path,
                                 KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (!client_state.Valid())
    return false;
  if (experiment_labels.empty()) {
    return client_state.DeleteValue(google_update::kExperimentLabels) ==
           ERROR_SUCCESS;
  }
  return client_state.WriteValue(google_update::kExperimentLabels,
                                 experiment_labels.c_str()) == ERROR_SUCCESS;
}

}  // namespace gcapi_internals
