// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_TEST_UWS_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_TEST_UWS_H_

#include "chrome/chrome_cleaner/constants/uws_id.h"

namespace chrome_cleaner {

constexpr UwSId kGoogleTestAUwSID = 341;
constexpr UwSId kGoogleTestBUwSID = 342;
constexpr UwSId kGoogleTestCUwSID = 343;

extern const char kGoogleTestAUwEID[];
extern const char kGoogleTestBUwEID[];
extern const char kGoogleTestExtensionInvalid1[];
extern const char kGoogleTestExtensionInvalid2[];

extern const char kTestUwsAFileContents[];
extern const int kTestUwsAFileContentsSize;
extern const wchar_t kTestUwsAFilename[];

extern const char kTestUwsBFileContents[];
extern const int kTestUwsBFileContentsSize;
extern const wchar_t kTestUwsBFilename[];

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_TEST_UWS_H_
