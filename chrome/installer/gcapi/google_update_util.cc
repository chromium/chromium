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

}  // namespace gcapi_internals
