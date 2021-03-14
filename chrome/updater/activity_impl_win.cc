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

}  // namespace

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser: {
      // TODO(crbug/1159498): Standardize registry access.
      base::win::RegKey key;
      if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                   KEY_READ | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
        std::wstring value;
        if (key.ReadValue(kDidRun, &value) == ERROR_SUCCESS && value == L"1")
          return true;
      }
      return false;
    }
    case UpdaterScope::kSystem:
      // TODO(crbug.com/1096654): Add support for the machine case. Machine
      // installs must look for values under HKLM and under every HKU\<sid>.
      return false;
  }
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  switch (scope) {
    case UpdaterScope::kUser: {
      // TODO(crbug/1159498): Standardize registry access.
      base::win::RegKey key;
      if (key.Open(HKEY_CURRENT_USER, GetAppClientStateKey(id).c_str(),
                   KEY_WRITE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
        const LONG result = key.WriteValue(kDidRun, L"0");
        if (result) {
          VLOG(2) << "Failed to clear HKCU activity key for " << id << ": "
                  << result;
        }
      } else {
        VLOG(2) << "Failed to open HKCU activity key with write for " << id;
      }
      break;
    }
    case UpdaterScope::kSystem:
      // TODO(crbug.com/1096654): Add support for the machine case. Machine
      // installs must clear values under HKLM and under every HKU\<sid>.
      break;
  }
}

}  // namespace updater
