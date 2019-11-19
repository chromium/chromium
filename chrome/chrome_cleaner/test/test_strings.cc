// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_strings.h"

namespace chrome_cleaner {

// Command line switches.
const char kTestSleepMinutesSwitch[] = "test-sleep-minutes";
const char kTestEventToSignal[] = "test-event-to-signal";
const char kTestForceOverwriteZoneIdentifier[] =
    "test-force-overwrite-zone-identifier";

// Test file names.
const wchar_t kValidUtf8Name[] = L"unicode_file_\u79c1.exe";
const wchar_t kInvalidUtf8Name[] = L"unicode_file_\xd800.exe";

// File content data.
const char kFileContent[] = "This is the file content.";

// GUIDs for tests.
const GUID kGUID1 = {0x7698f759,
                     0xf5b0,
                     0x4328,
                     {0x92, 0x38, 0xbd, 0x70, 0x8a, 0x6d, 0xc9, 0x63}};
const GUID kGUID2 = {0xe2a9ee7b,
                     0x456a,
                     0x485f,
                     {0xa3, 0xf2, 0x5e, 0x7c, 0x59, 0x6b, 0xa7, 0xe5}};
const GUID kGUID3 = {0x61956963,
                     0x386,
                     0x437b,
                     {0x86, 0x5e, 0xaf, 0xad, 0x63, 0x7e, 0x3f, 0x16}};
const wchar_t kGUID1Str[] = L"{7698F759-F5B0-4328-9238-BD708A6DC963}";
const wchar_t kGUID2Str[] = L"{E2A9EE7B-456A-485F-A3F2-5E7C596BA7E5}";
const wchar_t kGUID3Str[] = L"{61956963-0386-437B-865E-AFAD637E3F16}";

}  // namespace chrome_cleaner
