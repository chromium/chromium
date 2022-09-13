// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
  DWORD size = std::size(computer_name);
  if (::GetComputerNameW(computer_name, &size)) {
    std::string result;
    bool conversion_successful = base::WideToUTF8(computer_name, size, &result);
    DCHECK(conversion_successful);
    return result;
  }
  return std::string();
}

}  // namespace syncer
