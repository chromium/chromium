// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/blocklist_constants.h"

namespace blocklist {

const wchar_t kRegistryBeaconKeyName[] = L"\\BLBeacon";
const wchar_t kBeaconVersion[] = L"version";
const wchar_t kBeaconState[] = L"state";
const wchar_t kBeaconAttemptCount[] = L"failed_count";

const DWORD kBeaconMaxAttempts = 2;

}  // namespace blocklist
