// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/test_uws.h"

namespace chrome_cleaner {

const char kGoogleTestAUwEID[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kGoogleTestBUwEID[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
// invalid because the characters must be between 'a' and 'p' (inclusive).
const char kGoogleTestExtensionInvalid1[] =
    "abcdefghijklmnopqrstuvwxyzabcdefghijk";
const char kGoogleTestExtensionInvalid2[] = "";

// The contents of the Google A and Google B test UwS files.
// These files must be named with an extension recognized as
// an executable, such as .exe or .bat.
const char kTestUwsAFileContents[] =
    "#@$%!=<{(ESET-GOOGLE-PROTECTOR-TEST-FILE)(DNISJDJMKZMGMSMJPSRK)(A)}>=!%$@"
    "#";
// Don't include the null-terminator in the length.
const int kTestUwsAFileContentsSize = sizeof(kTestUwsAFileContents) - 1;

const char kTestUwsBFileContents[] =
    "#@$%!=<{(ESET-GOOGLE-PROTECTOR-TEST-FILE)(DNISJDJMKZMGMSMJPSRK)(B)}>=!%$@"
    "#";
// Don't include the null-terminator in the length.
const int kTestUwsBFileContentsSize = sizeof(kTestUwsBFileContents) - 1;

const wchar_t kTestUwsAFilename[] = L"TestUwsA.exe";
const wchar_t kTestUwsBFilename[] = L"TestUwsB.exe";

}  // namespace chrome_cleaner
