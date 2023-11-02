// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/gcapi/gcapi_reactivation.h"

#include <stdint.h>

#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_constants.h"

using base::Time;
using base::win::RegKey;

namespace {
const wchar_t kReactivationHistoryKey[] = L"reactivation";

std::wstring GetReactivationHistoryKeyPath() {
  std::wstring reactivation_path(google_update::kRegPathClientState);
  reactivation_path += L"\\";
  reactivation_path += google_update::kChromeUpgradeCode;
  reactivation_path += L"\\";
  reactivation_path += kReactivationHistoryKey;
  return reactivation_path;
}
}  // namespace

bool HasBeenReactivated() {
  RegKey reactivation_key(HKEY_CURRENT_USER,
                          GetReactivationHistoryKeyPath().c_str(),
                          KEY_QUERY_VALUE | KEY_WOW64_32KEY);

  return reactivation_key.Valid();
}

bool SetReactivationBrandCode(const std::wstring& brand_code, int shell_mode) {
  bool success = false;

  // This function currently only should be run in a non-elevated shell,
  // so we return "true" if it is being invoked from an elevated shell.
  if (shell_mode == GCAPI_INVOKED_UAC_ELEVATION)
    return true;

  std::wstring path(google_update::kRegPathClientState);
  path += L"\\";
  path += google_update::kChromeUpgradeCode;

  RegKey client_state_key(HKEY_CURRENT_USER, path.c_str(),
                          KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (client_state_key.Valid()) {
    success = client_state_key.WriteValue(
                  google_update::kRegRLZReactivationBrandField,
                  brand_code.c_str()) == ERROR_SUCCESS;
  }

  if (success) {
    // Store this brand code in the reactivation history. Store it with a
    // a currently un-used timestamp for future proofing.
    RegKey reactivation_key(HKEY_CURRENT_USER,
                            GetReactivationHistoryKeyPath().c_str(),
                            KEY_WRITE | KEY_WOW64_32KEY);
    if (reactivation_key.Valid()) {
      int64_t timestamp = Time::Now().ToInternalValue();
      reactivation_key.WriteValue(brand_code.c_str(), &timestamp,
                                  sizeof(timestamp), REG_QWORD);
    }
  }

  return success;
}
