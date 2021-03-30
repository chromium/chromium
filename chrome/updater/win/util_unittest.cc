// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/updater/win/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestUtil, HRESULTFromLastError) {
  ::SetLastError(ERROR_ACCESS_DENIED);
  EXPECT_EQ(E_ACCESSDENIED, HRESULTFromLastError());
  ::SetLastError(ERROR_SUCCESS);
  EXPECT_EQ(E_FAIL, HRESULTFromLastError());
}

TEST(UpdaterTestUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

}  // namespace updater
