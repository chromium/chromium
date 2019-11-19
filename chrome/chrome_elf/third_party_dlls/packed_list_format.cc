// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"

#include <stddef.h>

namespace third_party_dlls {

// Subkey relative to install_static::GetRegistryPath().
const wchar_t kThirdPartyRegKeyName[] = L"\\ThirdParty";

// Subkey value of type REG_SZ to hold a full path to a packed-list file.
const wchar_t kBlFilePathRegValue[] = L"BlFilePath";

std::string GetFingerprintString(uint32_t time_data_stamp,
                                 uint32_t image_size) {
  // Max hex 32-bit value is 8 characters long.  2*8+1.
  char buffer[17] = {};
  ::snprintf(buffer, sizeof(buffer), "%08X%x", time_data_stamp, image_size);

  return std::string(buffer);
}

}  // namespace third_party_dlls
