// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_

#include <windows.h>

#include <string>

#include "chrome/chrome_elf/third_party_dlls/status_codes.h"

namespace third_party_dlls {

// Apply hook.
// - Ensure the rest of third_party_dlls is initialized before hooking.
ThirdPartyStatus ApplyHook();

// Testing-only access to private GetDataFromImage() function.
bool GetDataFromImageForTesting(PVOID mapped_image,
                                DWORD* time_date_stamp,
                                DWORD* image_size,
                                std::string* image_name,
                                std::string* section_path,
                                std::string* section_basename);

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_
