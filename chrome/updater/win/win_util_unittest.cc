// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_util.h"

#include <windows.h>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

// Converts an unsigned long integral to an HRESULT to avoid the
// warning about a narrowing conversion.
HRESULT MakeHRESULT(unsigned long x) {
  return static_cast<HRESULT>(x);
}

}  // namespace

TEST(WinUtil, HRESULTFromUpdaterError) {
  EXPECT_EQ(HRESULTFromUpdaterError(0), MakeHRESULT(0xa0430000L));
  EXPECT_EQ(HRESULTFromUpdaterError(ERROR_ACCESS_DENIED),
            MakeHRESULT(0xa0430005));
  EXPECT_EQ(HRESULTFromUpdaterError(-1), -1);
  EXPECT_EQ(HRESULTFromUpdaterError(-10), -10);
}

TEST(WinUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

TEST(WinUtil, GetServiceDisplayName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            L" ", kUpdaterVersionUtf16}),
              GetServiceDisplayName(is_internal_service));
  }
}

TEST(WinUtil, GetServiceName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            kUpdaterVersionUtf16}),
              GetServiceName(is_internal_service));
  }
}

}  // namespace updater
