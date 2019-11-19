// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/chrome_elf_constants.h"

namespace blacklist {

const wchar_t kRegistryBeaconKeyName[] = L"\\BLBeacon";
const wchar_t kBeaconVersion[] = L"version";
const wchar_t kBeaconState[] = L"state";
const wchar_t kBeaconAttemptCount[] = L"failed_count";

const DWORD kBeaconMaxAttempts = 2;

}  // namespace blacklist

namespace elf_sec {

const wchar_t kRegSecurityFinchKeyName[] = L"\\BrowserSboxFinch";

}  // namespace elf_sec
